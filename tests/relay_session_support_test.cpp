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

#include <vector>

#include "monitostr/net/relay_session_support.hpp"

namespace monitostr::net {

TEST_CASE("ShouldReconnectAttempt allows retryable transport stages", "[relay_session]") {
  CHECK(ShouldReconnectAttempt(false, 0, "resolve"));
  CHECK(ShouldReconnectAttempt(false, 2, "read"));
  CHECK(ShouldReconnectAttempt(false, 5, "write_req"));
}

TEST_CASE("BuildMonitorReqMessage builds the expected nostr REQ payload", "[relay_session]") {
  const auto payload = BuildMonitorReqMessage("abc123");

  CHECK(payload == R"(["REQ","monitostr-sub",{"authors":["abc123"],"kinds":[1],"limit":1}])");
}

TEST_CASE("ParseRelayMessageEffect identifies AUTH challenges", "[relay_session]") {
  const auto effect = ParseRelayMessageEffect(R"(["AUTH","challenge-token"])");

  CHECK(effect.type == RelayMessageEffect::Type::kAuthChallenge);
  CHECK(effect.challenge == "challenge-token");
}

TEST_CASE("ParseRelayMessageEffect identifies EVENT and EOSE messages", "[relay_session]") {
  const auto event_effect = ParseRelayMessageEffect(R"(["EVENT","sub",{"kind":1}])");
  const auto eose_effect = ParseRelayMessageEffect(R"(["EOSE","sub"])");

  CHECK(event_effect.type == RelayMessageEffect::Type::kEventCount);
  CHECK(eose_effect.type == RelayMessageEffect::Type::kEndOfStoredEvents);
}

TEST_CASE("ParseRelayMessageEffect ignores malformed or unsupported payloads", "[relay_session]") {
  CHECK(ParseRelayMessageEffect("not-json").type == RelayMessageEffect::Type::kIgnore);
  CHECK(ParseRelayMessageEffect(R"({"type":"EVENT"})").type == RelayMessageEffect::Type::kIgnore);
  CHECK(ParseRelayMessageEffect(R"(["NOTICE","hello"])").type == RelayMessageEffect::Type::kIgnore);
}

TEST_CASE("ShouldReconnectAttempt blocks non-retryable or exhausted cases", "[relay_session]") {
  CHECK_FALSE(ShouldReconnectAttempt(true, 0, "read"));
  CHECK_FALSE(ShouldReconnectAttempt(false, 6, "read"));
  CHECK_FALSE(ShouldReconnectAttempt(false, 0, "ws_handshake"));
  CHECK_FALSE(ShouldReconnectAttempt(false, 0, "set_sni"));
  CHECK_FALSE(ShouldReconnectAttempt(false, 0, "auth"));
}

TEST_CASE("ParseSupportedNipsFromNip11Body returns sorted unique integers", "[relay_session]") {
  const auto nips = ParseSupportedNipsFromNip11Body(R"({"supported_nips":[11,1,65,1,2]})");

  CHECK(nips == std::vector<int>({1, 2, 11, 65}));
}

TEST_CASE("ParseSupportedNipsFromNip11Body ignores malformed payloads", "[relay_session]") {
  CHECK(ParseSupportedNipsFromNip11Body("not-json").empty());
  CHECK(ParseSupportedNipsFromNip11Body(R"({"supported_nips":"nope"})").empty());
}

TEST_CASE("ComputeReconnectDelayMs is bounded and deterministic for same inputs", "[relay_session]") {
  const unsigned delay_a = ComputeReconnectDelayMs("wss://relay.example", 3, 12345);
  const unsigned delay_b = ComputeReconnectDelayMs("wss://relay.example", 3, 12345);

  CHECK(delay_a == delay_b);
  CHECK(delay_a >= 2000);
  CHECK(delay_a <= 2250);
}

}  // namespace monitostr::net
