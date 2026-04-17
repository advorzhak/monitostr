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

#include "monitostr/net/relay_target.hpp"

namespace monitostr::net {

RelayHostTarget SplitRelayHostAndTarget(std::string_view relay) {
  const std::size_t target_pos = relay.find_first_of("/?");
  if (target_pos == std::string_view::npos) {
    return {std::string(relay), "/"};
  }

  if (relay[target_pos] == '?') {
    return {std::string(relay.substr(0, target_pos)), "/" + std::string(relay.substr(target_pos))};
  }

  return {std::string(relay.substr(0, target_pos)), std::string(relay.substr(target_pos))};
}

std::optional<ParsedRelayUrl> ParseRelayUrl(std::string_view relay_url) {
  ParsedRelayUrl parsed;
  std::string_view value = relay_url;
  if (value.rfind("wss://", 0) == 0) {
    parsed.secure = true;
    parsed.port = "443";
    value.remove_prefix(6);
  } else if (value.rfind("ws://", 0) == 0) {
    parsed.secure = false;
    parsed.port = "80";
    value.remove_prefix(5);
  } else {
    return std::nullopt;
  }

  const RelayHostTarget host_target = SplitRelayHostAndTarget(value);
  parsed.host = host_target.host;
  parsed.target = host_target.target;

  const std::size_t colon = parsed.host.find(':');
  if (colon != std::string::npos) {
    parsed.port = parsed.host.substr(colon + 1);
    parsed.host = parsed.host.substr(0, colon);
  }

  if (parsed.host.empty()) {
    return std::nullopt;
  }

  return parsed;
}

}  // namespace monitostr::net
