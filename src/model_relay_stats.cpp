#include "monitostr/model/relay_stats.hpp"

#include <algorithm>

namespace monitostr::model {

void RelayStats::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  by_relay_.clear();
}

void RelayStats::EnsureRelay(const std::string& relay_url) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (by_relay_.find(relay_url) == by_relay_.end()) {
    RelayStat stat;
    stat.relay_url = relay_url;
    by_relay_.emplace(relay_url, std::move(stat));
  }
}

void RelayStats::SetStatus(const std::string& relay_url, RelayStatus status, const std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  RelayStat& stat = by_relay_[relay_url];
  stat.relay_url = relay_url;
  stat.status = status;
  stat.last_error = error;
}

void RelayStats::RecordLatency(const std::string& relay_url, double latency_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  RelayStat& stat = by_relay_[relay_url];
  stat.relay_url = relay_url;
  stat.latency_ms = latency_ms;
  stat.latency_history_ms.push_back(latency_ms);
  while (stat.latency_history_ms.size() > kLatencyHistoryWindow) {
    stat.latency_history_ms.pop_front();
  }
}

void RelayStats::IncrementEvent(const std::string& relay_url) {
  std::lock_guard<std::mutex> lock(mutex_);
  RelayStat& stat = by_relay_[relay_url];
  stat.relay_url = relay_url;
  ++stat.events_count;
}

void RelayStats::SetSupportedNips(const std::string& relay_url, std::vector<int> supported_nips) {
  std::lock_guard<std::mutex> lock(mutex_);
  RelayStat& stat = by_relay_[relay_url];
  stat.relay_url = relay_url;
  std::sort(supported_nips.begin(), supported_nips.end());
  supported_nips.erase(std::unique(supported_nips.begin(), supported_nips.end()), supported_nips.end());
  stat.supported_nips = std::move(supported_nips);
}

std::vector<RelayStat> RelayStats::Snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<RelayStat> out;
  out.reserve(by_relay_.size());
  for (const auto& [_, stat] : by_relay_) {
    out.push_back(stat);
  }
  std::sort(out.begin(), out.end(),
            [](const RelayStat& lhs, const RelayStat& rhs) { return lhs.relay_url < rhs.relay_url; });
  return out;
}

AggregateStats RelayStats::Aggregate() const {
  std::lock_guard<std::mutex> lock(mutex_);
  AggregateStats aggregate;
  for (const auto& [_, stat] : by_relay_) {
    if (stat.status == RelayStatus::kSubscribed) {
      ++aggregate.connected_relays;
    }
    if (stat.status == RelayStatus::kError) {
      ++aggregate.errored_relays;
    }
    aggregate.total_events += stat.events_count;
  }
  return aggregate;
}

const char* ToString(RelayStatus status) {
  switch (status) {
    case RelayStatus::kDisconnected:
      return "Disconnected";
    case RelayStatus::kResolving:
      return "Resolving";
    case RelayStatus::kConnecting:
      return "Connecting";
    case RelayStatus::kTlsHandshake:
      return "TLS";
    case RelayStatus::kWsHandshake:
      return "WebSocket";
    case RelayStatus::kSubscribed:
      return "Subscribed";
    case RelayStatus::kError:
      return "Error";
  }
  return "Unknown";
}

}  // namespace monitostr::model
