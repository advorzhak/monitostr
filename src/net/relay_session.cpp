// Copyright (C) 2026 advorzhak
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "monitostr/net/relay_session.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "monitostr/net/relay_session_support.hpp"
#include "monitostr/net/relay_target.hpp"
#include "monitostr/nostr/signer.hpp"

#include <chrono>
#include <optional>
#include <random>

namespace monitostr::net {
namespace {

template <typename Stream>
void SetIoDeadline(Stream& stream, std::chrono::seconds timeout) {
  boost::beast::get_lowest_layer(stream).expires_after(timeout);
}

std::optional<std::vector<int>> FetchRelaySupportedNips(
    const ParsedRelayUrl& parsed_relay, const std::string& relay_url, boost::asio::ssl::context& ssl_context,
    const std::shared_ptr<monitostr::model::LogBuffer>& log_buffer) {
  namespace http = boost::beast::http;
  constexpr auto kTimeout = std::chrono::seconds(8);

  try {
    boost::asio::io_context ioc;

    boost::asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(parsed_relay.host, parsed_relay.port);

    std::vector<int> nips;
    if (parsed_relay.secure) {
      boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ssl_context);
      if (!SSL_set_tlsext_host_name(stream.native_handle(), parsed_relay.host.c_str())) {
        if (log_buffer) {
          log_buffer->Warn("[" + relay_url + "] NIP-11 SNI setup failed");
        }
        return std::nullopt;
      }
      stream.set_verify_mode(boost::asio::ssl::verify_peer);
      stream.set_verify_callback(boost::asio::ssl::host_name_verification(parsed_relay.host));

      SetIoDeadline(stream, kTimeout);
      boost::beast::get_lowest_layer(stream).connect(results);
      SetIoDeadline(stream, kTimeout);
      stream.handshake(boost::asio::ssl::stream_base::client);

      http::request<http::string_body> req{http::verb::get, "/", 11};
      req.set(http::field::host, parsed_relay.host);
      req.set(http::field::user_agent, "monitostr");
      req.set(http::field::accept, "application/nostr+json");

      SetIoDeadline(stream, kTimeout);
      http::write(stream, req);

      boost::beast::flat_buffer buffer;
      http::response<http::string_body> res;
      SetIoDeadline(stream, kTimeout);
      http::read(stream, buffer, res);

      nips = ParseSupportedNipsFromNip11Body(res.body());

      boost::system::error_code shutdown_ec;
      stream.shutdown(shutdown_ec);
    } else {
      boost::beast::tcp_stream stream(ioc);
      SetIoDeadline(stream, kTimeout);
      stream.connect(results);

      http::request<http::string_body> req{http::verb::get, "/", 11};
      req.set(http::field::host, parsed_relay.host);
      req.set(http::field::user_agent, "monitostr");
      req.set(http::field::accept, "application/nostr+json");

      SetIoDeadline(stream, kTimeout);
      http::write(stream, req);

      boost::beast::flat_buffer buffer;
      http::response<http::string_body> res;
      SetIoDeadline(stream, kTimeout);
      http::read(stream, buffer, res);

      nips = ParseSupportedNipsFromNip11Body(res.body());

      boost::system::error_code shutdown_ec;
      stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
    }

    return nips;
  } catch (const std::exception& ex) {
    if (log_buffer) {
      log_buffer->Warn("[" + relay_url + "] NIP-11 fetch failed: " + std::string(ex.what()));
    }
    return std::nullopt;
  }
}

}  // namespace

RelaySession::RelaySession(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
                           boost::asio::thread_pool& background_pool,
                           std::shared_ptr<monitostr::model::RelayStats> shared_stats,
                           std::shared_ptr<monitostr::model::LogBuffer> log_buffer, std::string relay_url,
                           std::string hex_pubkey)
    : strand_(boost::asio::make_strand(io_context)),
      resolver_(io_context),
      ws_(io_context, ssl_context),
      reconnect_timer_(io_context),
      ssl_context_(ssl_context),
      background_pool_(background_pool),
      shared_stats_(std::move(shared_stats)),
      log_buffer_(std::move(log_buffer)),
      relay_url_(std::move(relay_url)),
      hex_pubkey_(std::move(hex_pubkey)) {
  if (const auto parsed = monitostr::net::ParseRelayUrl(relay_url_); parsed.has_value()) {
    parsed_relay_ = *parsed;
  }
}

void RelaySession::Start() {
  boost::asio::dispatch(strand_, [self = shared_from_this()]() {
    self->stopped_ = false;
    self->reconnect_timer_.cancel();
    self->ParseRelayUrl();
    if (self->parsed_relay_.host.empty()) {
      self->shared_stats_->EnsureRelay(self->relay_url_);
      self->shared_stats_->SetStatus(self->relay_url_, monitostr::model::RelayStatus::kError, "invalid relay URL");
      if (self->log_buffer_) {
        self->log_buffer_->Error("[" + self->relay_url_ + "] invalid relay URL");
      }
      return;
    }
    if (!self->parsed_relay_.secure) {
      self->shared_stats_->EnsureRelay(self->relay_url_);
      self->shared_stats_->SetStatus(self->relay_url_, monitostr::model::RelayStatus::kError,
                                     "unsupported ws:// relay URL");
      if (self->log_buffer_) {
        self->log_buffer_->Warn("[" + self->relay_url_ + "] ws:// relay URLs are not supported yet; skipping session");
      }
      return;
    }
    if (self->log_buffer_) {
      self->log_buffer_->Info("[" + self->relay_url_ + "] resolving host " + self->parsed_relay_.host);
    }
    self->shared_stats_->EnsureRelay(self->relay_url_);
    self->shared_stats_->SetStatus(self->relay_url_, monitostr::model::RelayStatus::kResolving);
    self->StartResolve();
  });
}

void RelaySession::Stop() {
  boost::asio::dispatch(strand_, [self = shared_from_this()]() {
    self->stopped_ = true;
    self->reconnect_timer_.cancel();
    boost::system::error_code ignored;
    self->resolver_.cancel();
    self->read_buffer_.consume(self->read_buffer_.size());
    self->ws_.next_layer().shutdown(ignored);
    self->ws_.next_layer().next_layer().socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    boost::system::error_code ec;
    self->ws_.next_layer().shutdown(ec);
    self->ws_.next_layer().next_layer().socket().close(ec);
    self->shared_stats_->SetStatus(self->relay_url_, monitostr::model::RelayStatus::kDisconnected);
  });
}

void RelaySession::ParseRelayUrl() {
  parsed_relay_ = {};
  if (const auto parsed = monitostr::net::ParseRelayUrl(relay_url_); parsed.has_value()) {
    parsed_relay_ = *parsed;
  }
}

void RelaySession::StartResolve() {
  resolver_.async_resolve(
      parsed_relay_.host, parsed_relay_.port,
      boost::asio::bind_executor(strand_, [self = shared_from_this()](const boost::system::error_code& ec,
                                                                      const Resolver::results_type& results) {
        self->OnResolve(ec, results);
      }));
}

void RelaySession::OnResolve(const boost::system::error_code& ec, const Resolver::results_type& results) {
  if (ec) {
    MarkError("resolve", ec);
    return;
  }
  if (log_buffer_) {
    log_buffer_->Info("[" + relay_url_ + "] resolved, connecting...");
  }
  shared_stats_->SetStatus(relay_url_, monitostr::model::RelayStatus::kConnecting);

  boost::beast::get_lowest_layer(ws_).async_connect(
      results, boost::asio::bind_executor(
                   strand_, [self = shared_from_this()](const boost::system::error_code& connect_ec,
                                                        const Resolver::results_type::endpoint_type& endpoint) {
                     self->OnConnect(connect_ec, endpoint);
                   }));
}

void RelaySession::OnConnect(const boost::system::error_code& ec,
                             const Resolver::results_type::endpoint_type& endpoint) {
  (void)endpoint;
  if (ec) {
    MarkError("connect", ec);
    return;
  }

  if (log_buffer_) {
    log_buffer_->Info("[" + relay_url_ + "] TCP connected, starting TLS handshake");
  }

  // Configure SNI + host verification per relay before TLS handshake.
  if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), parsed_relay_.host.c_str())) {
    const auto ssl_error = static_cast<int>(::ERR_get_error());
    boost::system::error_code sni_ec(ssl_error, boost::asio::error::get_ssl_category());
    MarkError("set_sni", sni_ec);
    return;
  }
  ws_.next_layer().set_verify_mode(boost::asio::ssl::verify_peer);
  ws_.next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(parsed_relay_.host));

  shared_stats_->SetStatus(relay_url_, monitostr::model::RelayStatus::kTlsHandshake);
  ws_.next_layer().async_handshake(
      boost::asio::ssl::stream_base::client,
      boost::asio::bind_executor(strand_, [self = shared_from_this()](const boost::system::error_code& hs_ec) {
        self->OnTlsHandshake(hs_ec);
      }));
}

void RelaySession::OnTlsHandshake(const boost::system::error_code& ec) {
  if (ec) {
    MarkError("tls_handshake", ec);
    return;
  }

  if (log_buffer_) {
    log_buffer_->Info("[" + relay_url_ + "] TLS handshake OK, starting WebSocket handshake");
  }
  shared_stats_->SetStatus(relay_url_, monitostr::model::RelayStatus::kWsHandshake);
  ws_.set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
    req.set(boost::beast::http::field::user_agent, "monitostr");
    req.set("Sec-WebSocket-Protocol", "nostr");
  }));
  ws_.async_handshake(
      parsed_relay_.host, parsed_relay_.target,
      boost::asio::bind_executor(strand_, [self = shared_from_this()](const boost::system::error_code& ws_ec) {
        self->OnWsHandshake(ws_ec);
      }));
}

void RelaySession::OnWsHandshake(const boost::system::error_code& ec) {
  if (ec) {
    MarkError("ws_handshake", ec);
    return;
  }

  reconnect_attempt_ = 0;

  if (log_buffer_) {
    log_buffer_->Info("[" + relay_url_ + "] subscribed");
  }
  shared_stats_->SetStatus(relay_url_, monitostr::model::RelayStatus::kSubscribed);
  req_sent_at_ = std::chrono::steady_clock::now();

  const auto parsed_relay = parsed_relay_;
  boost::asio::post(background_pool_, [self = shared_from_this(), parsed_relay]() {
    const auto nips = FetchRelaySupportedNips(parsed_relay, self->relay_url_, self->ssl_context_, self->log_buffer_);
    if (!nips.has_value()) {
      return;
    }
    boost::asio::post(self->strand_, [self, nips = std::move(*nips)]() mutable {
      if (self->stopped_) {
        return;
      }
      self->shared_stats_->SetSupportedNips(self->relay_url_, std::move(nips));
      if (self->log_buffer_) {
        self->log_buffer_->Info("[" + self->relay_url_ + "] NIP-11 capabilities loaded");
      }
    });
  });

  const std::string req = BuildReqMessage();
  ws_.async_write(
      boost::asio::buffer(req),
      boost::asio::bind_executor(strand_, [self = shared_from_this()](const boost::system::error_code& write_ec,
                                                                      std::size_t bytes_transferred) {
        self->OnWriteReq(write_ec, bytes_transferred);
      }));
}

void RelaySession::OnWriteReq(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  (void)bytes_transferred;
  if (ec) {
    MarkError("write_req", ec);
    return;
  }
  DoRead();
}

void RelaySession::DoRead() {
  ws_.async_read(
      read_buffer_,
      boost::asio::bind_executor(strand_, [self = shared_from_this()](const boost::system::error_code& ec,
                                                                      std::size_t bytes) { self->OnRead(ec, bytes); }));
}

void RelaySession::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  (void)bytes_transferred;
  if (ec) {
    MarkError("read", ec);
    return;
  }

  const std::string payload = boost::beast::buffers_to_string(read_buffer_.data());
  read_buffer_.consume(read_buffer_.size());
  HandleMessage(payload);

  if (!stopped_) {
    DoRead();
  }
}

std::string RelaySession::BuildReqMessage() const { return BuildMonitorReqMessage(hex_pubkey_); }

void RelaySession::HandleMessage(const std::string& payload) {
  const auto effect = ParseRelayMessageEffect(payload);
  switch (effect.type) {
    case RelayMessageEffect::Type::kAuthChallenge:
      OnAuthChallenge(effect.challenge);
      break;
    case RelayMessageEffect::Type::kEventCount:
      shared_stats_->IncrementEvent(relay_url_);
      break;
    case RelayMessageEffect::Type::kEndOfStoredEvents: {
      const auto now = std::chrono::steady_clock::now();
      const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - req_sent_at_).count();
      shared_stats_->RecordLatency(relay_url_, static_cast<double>(elapsed_ms));
      if (log_buffer_) {
        log_buffer_->Info("[" + relay_url_ + "] EOSE received in " + std::to_string(elapsed_ms) + " ms");
      }
      break;
    }
    case RelayMessageEffect::Type::kIgnore:
      break;
  }
}

void RelaySession::OnAuthChallenge(const std::string& challenge) {
  if (!auth_key_) {
    if (log_buffer_) {
      log_buffer_->Warn("[" + relay_url_ + "] NIP-42 AUTH challenge received — no private key set; skipping");
    }
    return;
  }

  if (log_buffer_) {
    log_buffer_->Info("[" + relay_url_ + "] NIP-42 AUTH challenge received — signing");
  }

  const std::string event_json = monitostr::nostr::BuildAuthEvent(relay_url_, challenge, auth_key_->privkey_bytes());
  if (event_json.empty()) {
    if (log_buffer_) {
      log_buffer_->Error("[" + relay_url_ + "] NIP-42 AUTH signing failed");
    }
    return;
  }

  // Send ["AUTH", <event_object>].
  const std::string msg = nlohmann::json::array({"AUTH", nlohmann::json::parse(event_json)}).dump();

  auto self = shared_from_this();
  ws_.async_write(boost::asio::buffer(msg),
                  boost::asio::bind_executor(
                      strand_, [self, relay_url = relay_url_](const boost::system::error_code& ec, std::size_t) {
                        if (ec) {
                          if (self->log_buffer_) {
                            self->log_buffer_->Error("[" + relay_url + "] NIP-42 AUTH write failed: " + ec.message());
                          }
                          return;
                        }
                        if (self->log_buffer_) {
                          self->log_buffer_->Info("[" + relay_url + "] NIP-42 AUTH sent");
                        }
                      }));
}

void RelaySession::MarkError(const std::string& where, const boost::system::error_code& ec) {
  shared_stats_->SetStatus(relay_url_, monitostr::model::RelayStatus::kError, where + ": " + ec.message());
  if (log_buffer_) {
    log_buffer_->Error("[" + relay_url_ + "] " + where + ": " + ec.message());
  }

  if (ShouldReconnect(where)) {
    ScheduleReconnect(where + ": " + ec.message());
  }
}

bool RelaySession::ShouldReconnect(const std::string& where) const {
  return ShouldReconnectAttempt(stopped_, reconnect_attempt_, where);
}

void RelaySession::ResetTransportForReconnect() {
  boost::system::error_code ignored;
  resolver_.cancel();
  read_buffer_.consume(read_buffer_.size());
  ws_.next_layer().shutdown(ignored);
  ws_.next_layer().next_layer().socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
  ws_.next_layer().next_layer().socket().close(ignored);
}

void RelaySession::ScheduleReconnect(const std::string& reason) {
  ++reconnect_attempt_;
  const auto delay_ms =
      ComputeReconnectDelayMs(relay_url_, reconnect_attempt_,
                              static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));

  if (log_buffer_) {
    log_buffer_->Warn("[" + relay_url_ + "] reconnect " + std::to_string(reconnect_attempt_) + "/" +
                      std::to_string(kMaxReconnectAttempts) + " in " + std::to_string(delay_ms) + " ms after " +
                      reason);
  }

  reconnect_timer_.expires_after(std::chrono::milliseconds(delay_ms));
  reconnect_timer_.async_wait(
      boost::asio::bind_executor(strand_, [self = shared_from_this()](const boost::system::error_code& timer_ec) {
        if (timer_ec || self->stopped_) {
          return;
        }
        self->ResetTransportForReconnect();
        self->shared_stats_->SetStatus(self->relay_url_, monitostr::model::RelayStatus::kResolving);
        self->StartResolve();
      }));
}

}  // namespace monitostr::net
