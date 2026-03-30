#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "monitostr/model/relay_stats.hpp"

namespace monitostr::model {

TEST_CASE("RelayStats aggregates connection and error totals", "[relay_stats]") {
  RelayStats stats;

  stats.SetStatus("wss://a.example", RelayStatus::kSubscribed);
  stats.SetStatus("wss://b.example", RelayStatus::kError, "boom");
  stats.IncrementEvent("wss://a.example");
  stats.IncrementEvent("wss://a.example");
  stats.IncrementEvent("wss://b.example");

  const AggregateStats aggregate = stats.Aggregate();

  CHECK(aggregate.connected_relays == 1);
  CHECK(aggregate.errored_relays == 1);
  CHECK(aggregate.total_events == 3);
}

TEST_CASE("RelayStats keeps latency history window bounded", "[relay_stats]") {
  RelayStats stats;
  const char* relay = "wss://window.example";

  for (int i = 1; i <= 80; ++i) {
    stats.RecordLatency(relay, static_cast<double>(i));
  }

  const auto snapshot = stats.Snapshot();
  REQUIRE(snapshot.size() == 1);
  const RelayStat& entry = snapshot.front();

  REQUIRE(entry.latency_ms.has_value());
  CHECK(*entry.latency_ms == 80.0);
  CHECK(entry.latency_history_ms.size() == 60);
  CHECK(entry.latency_history_ms.front() == 21.0);
  CHECK(entry.latency_history_ms.back() == 80.0);
}

TEST_CASE("RelayStats sorts and deduplicates supported NIPs", "[relay_stats]") {
  RelayStats stats;

  stats.SetSupportedNips("wss://relay.example", {11, 1, 11, 65, 2, 2});

  const auto snapshot = stats.Snapshot();
  REQUIRE(snapshot.size() == 1);
  CHECK(snapshot.front().supported_nips == std::vector<int>({1, 2, 11, 65}));
}

TEST_CASE("ToString covers known relay statuses", "[relay_stats]") {
  CHECK(std::string(ToString(RelayStatus::kDisconnected)) == "Disconnected");
  CHECK(std::string(ToString(RelayStatus::kResolving)) == "Resolving");
  CHECK(std::string(ToString(RelayStatus::kConnecting)) == "Connecting");
  CHECK(std::string(ToString(RelayStatus::kTlsHandshake)) == "TLS");
  CHECK(std::string(ToString(RelayStatus::kWsHandshake)) == "WebSocket");
  CHECK(std::string(ToString(RelayStatus::kSubscribed)) == "Subscribed");
  CHECK(std::string(ToString(RelayStatus::kError)) == "Error");
}

}  // namespace monitostr::model
