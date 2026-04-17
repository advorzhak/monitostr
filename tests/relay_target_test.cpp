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

TEST_CASE("ParseRelayUrl parses secure relay URLs", "[relay_target]") {
  const auto parsed = ParseRelayUrl("wss://relay.example/ws?global=all");

  REQUIRE(parsed.has_value());
  CHECK(parsed->secure);
  CHECK(parsed->host == "relay.example");
  CHECK(parsed->port == "443");
  CHECK(parsed->target == "/ws?global=all");
}

TEST_CASE("ParseRelayUrl parses insecure relay URLs with explicit port", "[relay_target]") {
  const auto parsed = ParseRelayUrl("ws://relay.example:8080");

  REQUIRE(parsed.has_value());
  CHECK_FALSE(parsed->secure);
  CHECK(parsed->host == "relay.example");
  CHECK(parsed->port == "8080");
  CHECK(parsed->target == "/");
}

TEST_CASE("ParseRelayUrl rejects URLs without websocket scheme", "[relay_target]") {
  const auto parsed = ParseRelayUrl("relay.example");

  CHECK_FALSE(parsed.has_value());
}

}  // namespace monitostr::net
