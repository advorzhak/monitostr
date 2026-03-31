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
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "monitostr/nostr/signer.hpp"

#include <chrono>
#include <random>
#include <thread>

namespace monitostr::net {
namespace {

RelaySession::ParsedRelay ParseRelay(const std::string& relay_url) {
  RelaySession::ParsedRelay parsed;

  std::string value = relay_url;
  if (value.rfind("wss://", 0) == 0) {
    value = value.substr(6);
    parsed.port = "443";
  } else if (value.rfind("ws://", 0) == 0) {
    value = value.substr(5);
    parsed.port = "80";
  } else {
    parsed.port = "443";
  }

  const std::size_t slash = value.find('/');
  if (slash == std::string::npos) {
    parsed.host = value;
    parsed.target = "/";
  } else {
    parsed.host = value.substr(0, slash);
    parsed.target = value.substr(slash);
  }

  const std::size_t colon = parsed.host.find(':');
  if (colon != std::string::npos) {
    parsed.port = parsed.host.substr(colon + 1);
    parsed.host = parsed.host.substr(0, colon);
  }

  return parsed;
}

}  // namespace

RelaySession::RelaySession(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
                           std::shared_ptr<monitostr::model::RelayStats> shared_stats,
                           std::shared_ptr<monitostr::model::LogBuffer> log_buffer, std::string relay_url,
                           std::string hex_pubkey)
    : strand_(boost::asio::make_strand(io_context)),
      resolver_(io_context),
      ws_(io_context, ssl_context),
      reconnect_timer_(io_context),
      shared_stats_(std::move(shared_stats)),
      log_buffer_(std::move(log_buffer)),
      relay_url_(std::move(relay_url)),
      hex_pubkey_(std::move(hex_pubkey)) {
  parsed_relay_ = ParseRelay(relay_url_);
}

void RelaySession::Start() {
  boost::asio::dispatch(strand_, [self = shared_from_this()]() {
    self->stopped_ = false;
    self->reconnect_timer_.cancel();
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

void RelaySession::ParseRelayUrl() { parsed_relay_ = ParseRelay(relay_url_); }

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

  // Query relay metadata over NIP-11 in a separate thread so monitoring starts immediately.
  {
    const auto shared_stats = shared_stats_;
    const auto log_buffer = log_buffer_;
    const auto relay_url = relay_url_;
    const auto host = parsed_relay_.host;
    const auto port = parsed_relay_.port.empty() ? std::string("443") : parsed_relay_.port;
    const bool is_tls = relay_url.rfind("wss://", 0) == 0;

    std::thread([shared_stats, log_buffer, relay_url, host, port, is_tls]() {
      namespace http = boost::beast::http;
      try {
        boost::asio::io_context ioc;
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
        ssl_ctx.set_default_verify_paths();

        boost::asio::ip::tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, port);

        if (is_tls) {
          boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ssl_ctx);
          if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            if (log_buffer) {
              log_buffer->Warn("[" + relay_url + "] NIP-11 SNI setup failed");
            }
            return;
          }
          stream.set_verify_mode(boost::asio::ssl::verify_peer);
          stream.set_verify_callback(boost::asio::ssl::host_name_verification(host));

          boost::beast::get_lowest_layer(stream).connect(results);
          stream.handshake(boost::asio::ssl::stream_base::client);

          http::request<http::string_body> req{http::verb::get, "/", 11};
          req.set(http::field::host, host);
          req.set(http::field::user_agent, "monitostr");
          req.set(http::field::accept, "application/nostr+json");

          http::write(stream, req);

          boost::beast::flat_buffer buffer;
          http::response<http::string_body> res;
          http::read(stream, buffer, res);

          std::vector<int> nips;
          auto json = nlohmann::json::parse(res.body(), nullptr, false);
          if (!json.is_discarded() && json.contains("supported_nips") && json["supported_nips"].is_array()) {
            for (const auto& v : json["supported_nips"]) {
              if (v.is_number_integer()) {
                nips.push_back(v.get<int>());
              }
            }
          }
          shared_stats->SetSupportedNips(relay_url, std::move(nips));
          if (log_buffer) {
            log_buffer->Info("[" + relay_url + "] NIP-11 capabilities loaded");
          }

          boost::system::error_code shutdown_ec;
          stream.shutdown(shutdown_ec);
        } else {
          boost::beast::tcp_stream stream(ioc);
          stream.connect(results);

          http::request<http::string_body> req{http::verb::get, "/", 11};
          req.set(http::field::host, host);
          req.set(http::field::user_agent, "monitostr");
          req.set(http::field::accept, "application/nostr+json");

          http::write(stream, req);

          boost::beast::flat_buffer buffer;
          http::response<http::string_body> res;
          http::read(stream, buffer, res);

          std::vector<int> nips;
          auto json = nlohmann::json::parse(res.body(), nullptr, false);
          if (!json.is_discarded() && json.contains("supported_nips") && json["supported_nips"].is_array()) {
            for (const auto& v : json["supported_nips"]) {
              if (v.is_number_integer()) {
                nips.push_back(v.get<int>());
              }
            }
          }
          shared_stats->SetSupportedNips(relay_url, std::move(nips));
          if (log_buffer) {
            log_buffer->Info("[" + relay_url + "] NIP-11 capabilities loaded");
          }

          boost::system::error_code ec_shutdown;
          stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec_shutdown);
        }
      } catch (const std::exception& ex) {
        if (log_buffer) {
          log_buffer->Warn("[" + relay_url + "] NIP-11 fetch failed: " + std::string(ex.what()));
        }
      }
    }).detach();
  }

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

std::string RelaySession::BuildReqMessage() const {
  nlohmann::json filter = {
      {"authors", nlohmann::json::array({hex_pubkey_})},
      {"kinds", nlohmann::json::array({1})},
      {"limit", 1},
  };

  nlohmann::json req = nlohmann::json::array({"REQ", "monitostr-sub", filter});
  return req.dump();
}

void RelaySession::HandleMessage(const std::string& payload) {
  auto parsed = nlohmann::json::parse(payload, nullptr, false);
  if (parsed.is_discarded() || !parsed.is_array() || parsed.empty() || !parsed[0].is_string()) {
    return;
  }

  const std::string kind = parsed[0].get<std::string>();
  if (kind == "AUTH") {
    if (parsed.size() >= 2 && parsed[1].is_string()) {
      OnAuthChallenge(parsed[1].get<std::string>());
    }
  } else if (kind == "EVENT") {
    shared_stats_->IncrementEvent(relay_url_);
  } else if (kind == "EOSE") {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - req_sent_at_).count();
    shared_stats_->RecordLatency(relay_url_, static_cast<double>(elapsed_ms));
    if (log_buffer_) {
      log_buffer_->Info("[" + relay_url_ + "] EOSE received in " + std::to_string(elapsed_ms) + " ms");
    }
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
  if (stopped_ || reconnect_attempt_ >= kMaxReconnectAttempts) {
    return false;
  }

  // Handshake declines are often policy/path issues; avoid retry storms.
  if (where == "ws_handshake" || where == "set_sni") {
    return false;
  }

  return where == "read" || where == "resolve" || where == "connect" || where == "tls_handshake" ||
         where == "write_req";
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
  const auto exp = static_cast<unsigned>(std::min<std::size_t>(reconnect_attempt_ - 1, 8));
  const auto base_ms = 500U * (1U << exp);
  std::mt19937 rng(static_cast<std::mt19937::result_type>(
      std::hash<std::string>{}(relay_url_) ^
      static_cast<std::size_t>(std::chrono::steady_clock::now().time_since_epoch().count())));
  std::uniform_int_distribution<int> jitter(0, 250);
  const auto delay_ms = std::min<unsigned>(30000U, base_ms + static_cast<unsigned>(jitter(rng)));

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
