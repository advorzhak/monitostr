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
#include <ctime>
#include <deque>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace monitostr::model {

enum class LogLevel {
  kInfo,
  kWarn,
  kError,
};

// Fix #10: returns a short uppercase tag for a log level, used wherever the
// level must be rendered as human-readable text (wlogs output, TUI chip, etc.).
inline const char* ToString(LogLevel level) {
  switch (level) {
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarn:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
  }
  return "INFO";
}

struct LogEntry {
  std::string timestamp;
  LogLevel level = LogLevel::kInfo;
  // Fix #10: message stores the raw text with no embedded level prefix.
  // Use the 'level' field for display; ToString(level) for formatted output.
  std::string message;
};

class LogBuffer {
 public:
  explicit LogBuffer(std::size_t max_entries = 400) : max_entries_(max_entries) {}

  void Info(const std::string& message) { Push(LogLevel::kInfo, message); }
  void Warn(const std::string& message) { Push(LogLevel::kWarn, message); }
  void Error(const std::string& message) { Push(LogLevel::kError, message); }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
  }

  std::vector<LogEntry> Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {entries_.begin(), entries_.end()};
  }

 private:
  static std::string NowString() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
  }

  // Fix #10: message is stored as-is without a level prefix; the level field
  // on LogEntry carries that information separately.
  void Push(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(LogEntry{
        .timestamp = NowString(),
        .level = level,
        .message = message,
    });
    while (entries_.size() > max_entries_) {
      entries_.pop_front();
    }
  }

  std::size_t max_entries_;
  mutable std::mutex mutex_;
  std::deque<LogEntry> entries_;
};

}  // namespace monitostr::model
