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

#include "monitostr/net/bootstrap_client.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_set>

#include "monitostr/net/relay_target.hpp"

namespace monitostr::net {
namespace {

bool IsRelayUrlLike(const std::string& value) { return value.rfind("wss://", 0) == 0 || value.rfind("ws://", 0) == 0; }

std::vector<std::string> ExtractRelayUrlsFromEvent(const nlohmann::json& event) {
  std::vector<std::string> relays;

  if (event.contains("tags") && event["tags"].is_array()) {
    for (const auto& tag : event["tags"]) {
      if (!tag.is_array() || tag.size() < 2 || !tag[0].is_string() || !tag[1].is_string()) {
        continue;
      }
      const std::string key = tag[0].get<std::string>();
      const std::string value = tag[1].get<std::string>();
      if (key == "r" && IsRelayUrlLike(value)) {
        relays.push_back(value);
      }
    }
  }

  if (event.contains("content") && event["content"].is_string()) {
    const std::string content = event["content"].get<std::string>();
    auto parsed = nlohmann::json::parse(content, nullptr, false);
    if (!parsed.is_discarded()) {
      if (parsed.is_object()) {
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
          if (IsRelayUrlLike(it.key())) {
            relays.push_back(it.key());
          }
        }
      } else if (parsed.is_array()) {
        for (const auto& item : parsed) {
          if (item.is_string()) {
            const std::string value = item.get<std::string>();
            if (IsRelayUrlLike(value)) {
              relays.push_back(value);
            }
          }
        }
      }
    }
  }

  return relays;
}

std::vector<std::string> Deduplicate(std::vector<std::string> values) {
  std::vector<std::string> out;
  out.reserve(values.size());
  std::unordered_set<std::string> seen;
  for (auto& value : values) {
    const auto parsed = ParseRelayUrl(value);
    if (!parsed.has_value()) {
      continue;
    }
    if (seen.emplace(value).second) {
      out.push_back(std::move(value));
    }
  }
  return out;
}

template <typename WsStream>
void SetReadDeadline(WsStream& ws, std::chrono::seconds timeout) {
  boost::beast::get_lowest_layer(ws).expires_after(timeout);
}

std::vector<std::string> FilterSupportedRelayUrls(std::vector<std::string> relay_urls,
                                                  const std::shared_ptr<monitostr::model::LogBuffer>& log_buffer) {
  std::vector<std::string> filtered;
  filtered.reserve(relay_urls.size());

  std::size_t insecure_count = 0;
  for (auto& relay_url : relay_urls) {
    const auto parsed = ParseRelayUrl(relay_url);
    if (!parsed.has_value()) {
      continue;
    }
    if (!parsed->secure) {
      ++insecure_count;
      continue;
    }
    filtered.push_back(std::move(relay_url));
  }

  if (insecure_count > 0 && log_buffer) {
    log_buffer->Warn("Bootstrap ignored " + std::to_string(insecure_count) +
                     " insecure ws:// relay URLs; monitostr currently monitors wss:// relays only");
  }

  return Deduplicate(std::move(filtered));
}

template <typename WsStream>
std::vector<std::string> QueryKindWithWs(WsStream& ws, const std::string& hex_pubkey, int kind,
                                         const std::string& sub_id) {
  constexpr auto kReadTimeout = std::chrono::seconds(8);
  const nlohmann::json filter = {
      {"authors", nlohmann::json::array({hex_pubkey})},
      {"kinds", nlohmann::json::array({kind})},
      {"limit", 20},
  };

  const nlohmann::json req = nlohmann::json::array({"REQ", sub_id, filter});
  ws.write(boost::asio::buffer(req.dump()));

  std::vector<std::string> relay_urls;
  boost::beast::flat_buffer buffer;
  const auto started = std::chrono::steady_clock::now();

  while (std::chrono::steady_clock::now() - started < std::chrono::seconds(8)) {
    SetReadDeadline(ws, kReadTimeout);
    ws.read(buffer);
    const std::string payload = boost::beast::buffers_to_string(buffer.data());
    buffer.consume(buffer.size());

    auto message = nlohmann::json::parse(payload, nullptr, false);
    if (message.is_discarded() || !message.is_array() || message.empty() || !message[0].is_string()) {
      continue;
    }

    const std::string type = message[0].get<std::string>();
    if (type == "EVENT" && message.size() >= 3 && message[1].is_string() && message[1].get<std::string>() == sub_id &&
        message[2].is_object()) {
      const auto urls = ExtractRelayUrlsFromEvent(message[2]);
      relay_urls.insert(relay_urls.end(), urls.begin(), urls.end());
    }
    if (type == "EOSE" && message.size() >= 2 && message[1].is_string() && message[1].get<std::string>() == sub_id) {
      break;
    }
  }

  const nlohmann::json close = nlohmann::json::array({"CLOSE", sub_id});
  boost::system::error_code ignored;
  ws.write(boost::asio::buffer(close.dump()), ignored);

  return Deduplicate(std::move(relay_urls));
}

BootstrapResult QuerySeedRelay(const std::string& seed_relay_url, const std::string& hex_pubkey,
                               const std::shared_ptr<monitostr::model::LogBuffer>& log_buffer) {
  BootstrapResult result;
  if (hex_pubkey.empty()) {
    result.ok = false;
    result.error = "empty hex pubkey";
    return result;
  }

  const auto parsed = ParseRelayUrl(seed_relay_url);
  if (!parsed.has_value()) {
    result.ok = false;
    result.error = "invalid seed relay url";
    return result;
  }

  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver resolver(ioc);
  auto const endpoints = resolver.resolve(parsed->host, parsed->port);

  if (parsed->secure) {
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    ssl_ctx.set_default_verify_paths();

    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws(ioc, ssl_ctx);
    boost::beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(8));
    boost::beast::get_lowest_layer(ws).connect(endpoints);

    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), parsed->host.c_str())) {
      result.ok = false;
      result.error = "failed to set SNI";
      return result;
    }
    ws.next_layer().set_verify_mode(boost::asio::ssl::verify_peer);
    ws.next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(parsed->host));
    boost::beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(8));
    ws.next_layer().handshake(boost::asio::ssl::stream_base::client);
    ws.set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
      req.set("User-Agent", "monitostr");
      req.set("Sec-WebSocket-Protocol", "nostr");
    }));
    boost::beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(8));
    ws.handshake(parsed->host, parsed->target);

    auto relays = QueryKindWithWs(ws, hex_pubkey, 10002, "bootstrap-10002");
    if (!relays.empty()) {
      result.ok = true;
      result.source_event_kind = "10002";
      result.relay_urls = std::move(relays);
    } else {
      relays = QueryKindWithWs(ws, hex_pubkey, 3, "bootstrap-3");
      result.ok = !relays.empty();
      result.source_event_kind = result.ok ? "3" : "none";
      result.relay_urls = std::move(relays);
      if (!result.ok) {
        result.error = "no relay list found in kinds 10002/3";
      }
    }

    boost::system::error_code close_ec;
    ws.close(boost::beast::websocket::close_code::normal, close_ec);
  } else {
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws(ioc);
    boost::beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(8));
    boost::beast::get_lowest_layer(ws).connect(endpoints);
    ws.set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
      req.set("User-Agent", "monitostr");
      req.set("Sec-WebSocket-Protocol", "nostr");
    }));
    boost::beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(8));
    ws.handshake(parsed->host, parsed->target);

    auto relays = QueryKindWithWs(ws, hex_pubkey, 10002, "bootstrap-10002");
    if (!relays.empty()) {
      result.ok = true;
      result.source_event_kind = "10002";
      result.relay_urls = std::move(relays);
    } else {
      relays = QueryKindWithWs(ws, hex_pubkey, 3, "bootstrap-3");
      result.ok = !relays.empty();
      result.source_event_kind = result.ok ? "3" : "none";
      result.relay_urls = std::move(relays);
      if (!result.ok) {
        result.error = "no relay list found in kinds 10002/3";
      }
    }

    boost::system::error_code close_ec;
    ws.close(boost::beast::websocket::close_code::normal, close_ec);
  }

  result.relay_urls = FilterSupportedRelayUrls(std::move(result.relay_urls), log_buffer);
  if (result.relay_urls.empty()) {
    result.relay_urls = FilterSupportedRelayUrls(
        {
            seed_relay_url,
            "wss://nos.lol",
            "wss://relay.snort.social",
        },
        log_buffer);
    result.ok = true;
    result.source_event_kind = "fallback";
    if (log_buffer) {
      log_buffer->Warn("Bootstrap query failed, using fallback relay set");
    }
  }

  return result;
}

}  // namespace

BootstrapClient::BootstrapClient(boost::asio::io_context& io_context) : io_context_(io_context) {}

void BootstrapClient::SetLogBuffer(std::shared_ptr<monitostr::model::LogBuffer> log_buffer) {
  log_buffer_ = std::move(log_buffer);
}

void BootstrapClient::ResolveRelaysForPubkey(const std::string& hex_pubkey, Completion completion,
                                             const std::string& seed_relay_url) {
  const auto log_buffer = log_buffer_;
  if (log_buffer) {
    log_buffer->Info("Bootstrap starting (primary seed: " + seed_relay_url + ")");
  }

  boost::asio::post(worker_pool_, [this, hex_pubkey, seed_relay_url, completion = std::move(completion),
                                   log_buffer]() mutable {
    // Build ordered candidate list: caller-supplied seed first, then well-known fallback seeds.
    const std::vector<std::string> candidates = {
        seed_relay_url,     "wss://nos.lol",          "wss://relay.nostr.band", "wss://relay.snort.social",
        "wss://nostr.wine", "wss://relay.primal.net",
    };

    BootstrapResult result;
    for (const auto& candidate : candidates) {
      if (log_buffer) {
        log_buffer->Info("Bootstrap trying seed relay: " + candidate);
      }
      try {
        result = QuerySeedRelay(candidate, hex_pubkey, log_buffer);
        if (result.ok) {
          if (log_buffer) {
            log_buffer->Info("Bootstrap succeeded via " + candidate + " [kind:" + result.source_event_kind +
                             "] | relays discovered: " + std::to_string(result.relay_urls.size()));
          }
          break;
        }
        if (log_buffer) {
          log_buffer->Warn("Bootstrap seed " + candidate + " failed: " + result.error + " — trying next");
        }
      } catch (const std::exception& ex) {
        result.ok = false;
        result.error = ex.what();
        if (log_buffer) {
          log_buffer->Warn("Bootstrap seed " + candidate + " exception: " + std::string(ex.what()) + " — trying next");
        }
      }
    }

    if (!result.ok) {
      if (log_buffer) {
        log_buffer->Error("Bootstrap failed on all seed relays — using emergency fallback set");
      }
    }

    boost::asio::post(io_context_, [completion = std::move(completion), result = std::move(result)]() mutable {
      completion(std::move(result));
    });
  });
}

}  // namespace monitostr::net
