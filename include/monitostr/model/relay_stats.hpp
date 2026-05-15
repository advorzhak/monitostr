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
  std::optional<std::chrono::steady_clock::time_point> connected_at;
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
