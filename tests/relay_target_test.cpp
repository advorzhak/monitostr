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

#include <catch2/catch_test_macros.hpp>

#include "monitostr/net/relay_target.hpp"

namespace monitostr::net {

TEST_CASE("SplitRelayHostAndTarget preserves query-only relay targets", "[relay_target]") {
  const RelayHostTarget parsed = SplitRelayHostAndTarget("filter.nostr.wine?global=all");

  CHECK(parsed.host == "filter.nostr.wine");
  CHECK(parsed.target == "/?global=all");
}

TEST_CASE("SplitRelayHostAndTarget preserves paths and query parameters", "[relay_target]") {
  const RelayHostTarget parsed = SplitRelayHostAndTarget("relay.example/ws?global=all");

  CHECK(parsed.host == "relay.example");
  CHECK(parsed.target == "/ws?global=all");
}

}  // namespace monitostr::net
