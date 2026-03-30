#pragma once

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

struct LogEntry {
  std::string timestamp;
  LogLevel level = LogLevel::kInfo;
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

  static const char* LevelTag(LogLevel level) {
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

  void Push(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(LogEntry{
        .timestamp = NowString(),
        .level = level,
        .message = std::string("[") + LevelTag(level) + "] " + message,
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
