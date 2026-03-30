#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace monitostr::model {

enum class RelayStatus {
  kDisconnected,
  kResolving,
  kConnecting,
  kTlsHandshake,
  kWsHandshake,
  kSubscribed,
  kError,
};

struct RelayStat {
  std::string relay_url;
  RelayStatus status = RelayStatus::kDisconnected;
  std::optional<double> latency_ms;
  std::uint64_t events_count = 0;
  std::vector<int> supported_nips;
  std::deque<double> latency_history_ms;
  std::string last_error;
};

struct AggregateStats {
  std::size_t connected_relays = 0;
  std::size_t errored_relays = 0;
  std::uint64_t total_events = 0;
};

class RelayStats {
 public:
  void Reset();
  void EnsureRelay(const std::string& relay_url);
  void SetStatus(const std::string& relay_url, RelayStatus status, const std::string& error = "");
  void RecordLatency(const std::string& relay_url, double latency_ms);
  void IncrementEvent(const std::string& relay_url);
  void SetSupportedNips(const std::string& relay_url, std::vector<int> supported_nips);

  std::vector<RelayStat> Snapshot() const;
  AggregateStats Aggregate() const;

 private:
  static constexpr std::size_t kLatencyHistoryWindow = 60;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, RelayStat> by_relay_;
};

const char* ToString(RelayStatus status);

}  // namespace monitostr::model
