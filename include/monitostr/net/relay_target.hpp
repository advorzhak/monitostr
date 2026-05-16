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

#include <string>
#include <string_view>
#include <optional>

namespace monitostr::net {

struct RelayHostTarget {
  std::string host;
  std::string target;
};

struct ParsedRelayUrl {
  bool secure = true;
  std::string host;
  std::string port;
  std::string target;
};

[[nodiscard]] RelayHostTarget SplitRelayHostAndTarget(std::string_view relay);
[[nodiscard]] std::optional<ParsedRelayUrl> ParseRelayUrl(std::string_view relay_url);

}  // namespace monitostr::net
