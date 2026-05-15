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

#include "monitostr/net/relay_session_support.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <random>

namespace monitostr::net {

std::string BuildMonitorReqMessage(std::string_view hex_pubkey) {
  nlohmann::json filter = {
      {"authors", nlohmann::json::array({hex_pubkey})},
      {"kinds", nlohmann::json::array({1})},
      {"limit", 1},
  };

  return nlohmann::json::array({"REQ", "monitostr-sub", filter}).dump();
}

RelayMessageEffect ParseRelayMessageEffect(std::string_view payload) {
  const auto parsed = nlohmann::json::parse(payload, nullptr, false);
  if (parsed.is_discarded() || !parsed.is_array() || parsed.empty() || !parsed[0].is_string()) {
    return {};
  }

  const std::string kind = parsed[0].get<std::string>();
  if (kind == "AUTH" && parsed.size() >= 2 && parsed[1].is_string()) {
    return {
        .type = RelayMessageEffect::Type::kAuthChallenge,
        .challenge = parsed[1].get<std::string>(),
    };
  }
  if (kind == "EVENT") {
    return {.type = RelayMessageEffect::Type::kEventCount};
  }
  if (kind == "EOSE") {
    return {.type = RelayMessageEffect::Type::kEndOfStoredEvents};
  }

  return {};
}

bool ShouldReconnectAttempt(bool stopped, std::size_t reconnect_attempt, std::string_view where) {
  // Fix #13: use the single shared constant rather than a local copy.
  if (stopped || reconnect_attempt >= kMaxReconnectAttempts) {
    return false;
  }

  if (where == "ws_handshake" || where == "set_sni") {
    return false;
  }

  return where == "read" || where == "resolve" || where == "connect" || where == "tls_handshake" ||
         where == "write_req";
}

std::vector<int> ParseSupportedNipsFromNip11Body(std::string_view response_body) {
  std::vector<int> nips;
  const auto json = nlohmann::json::parse(response_body, nullptr, false);
  if (!json.is_discarded() && json.contains("supported_nips") && json["supported_nips"].is_array()) {
    for (const auto& value : json["supported_nips"]) {
      if (value.is_number_integer()) {
        nips.push_back(value.get<int>());
      }
    }
  }

  std::sort(nips.begin(), nips.end());
  nips.erase(std::unique(nips.begin(), nips.end()), nips.end());
  return nips;
}

unsigned ComputeReconnectDelayMs(const std::string& relay_url, std::size_t reconnect_attempt, std::uint64_t entropy) {
  const auto exp = static_cast<unsigned>(std::min<std::size_t>(reconnect_attempt > 0 ? reconnect_attempt - 1 : 0, 8));
  const auto base_ms = 500U * (1U << exp);
  std::mt19937 rng(static_cast<std::mt19937::result_type>(std::hash<std::string>{}(relay_url) ^ entropy));
  std::uniform_int_distribution<int> jitter(0, 250);
  return std::min<unsigned>(30000U, base_ms + static_cast<unsigned>(jitter(rng)));
}

}  // namespace monitostr::net
