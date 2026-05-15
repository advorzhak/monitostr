#pragma once

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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace monitostr::net {

// Shared reconnect limit used by RelaySession and ShouldReconnectAttempt.
inline constexpr std::size_t kMaxReconnectAttempts = 6;

struct RelayMessageEffect {
  enum class Type {
    kIgnore,
    kAuthChallenge,
    kEventCount,
    kEndOfStoredEvents,
  };

  Type type = Type::kIgnore;
  std::string challenge;
};

std::string BuildMonitorReqMessage(std::string_view hex_pubkey);
RelayMessageEffect ParseRelayMessageEffect(std::string_view payload);
bool ShouldReconnectAttempt(bool stopped, std::size_t reconnect_attempt, std::string_view where);
std::vector<int> ParseSupportedNipsFromNip11Body(std::string_view response_body);
unsigned ComputeReconnectDelayMs(const std::string& relay_url, std::size_t reconnect_attempt, std::uint64_t entropy);

}  // namespace monitostr::net
