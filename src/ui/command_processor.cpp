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

#include "monitostr/ui/command_processor.hpp"

#include <cstdio>
#include <cctype>
#include <fstream>
#include <vector>

namespace monitostr::ui {
namespace {

struct CommandEntry {
  std::string name;
  std::string desc;
};

std::string ToLowerCopy(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

std::string TrimCopy(const std::string& s) {
  std::size_t begin = 0;
  while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
    ++begin;
  }
  std::size_t end = s.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
    --end;
  }
  return s.substr(begin, end - begin);
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string JoinLogsText(const std::vector<monitostr::model::LogEntry>& logs) {
  std::string out;
  for (const auto& entry : logs) {
    out += entry.timestamp + " " + entry.message + "\n";
  }
  return out;
}

bool CopyTextToClipboardMac(const std::string& text) {
  FILE* pipe = popen("pbcopy", "w");
  if (!pipe) {
    return false;
  }
  const std::size_t written = fwrite(text.data(), 1, text.size(), pipe);
  const int rc = pclose(pipe);
  return written == text.size() && rc == 0;
}

bool SaveTextToFile(const std::string& path, const std::string& text) {
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << text;
  return out.good();
}

}  // namespace

std::string ComputeCommandHint(const std::string& input) {
  static const std::vector<CommandEntry> kCommands = {
      {"q", "quit"},
      {"quit", "quit"},
      {"qa", "quit all"},
      {"qall", "quit all"},
      {"cl", "clear logs"},
      {"clearlogs", "clear logs"},
      {"set follow", "enable log follow"},
      {"set nofollow", "disable log follow"},
      {"set nipswrap", "wrap NIPs"},
      {"set nonipswrap", "unwrap NIPs"},
      {"set compact", "force compact layout"},
      {"set nocompact", "force wide layout"},
      {"set autocompact", "auto layout"},
      {"filter", "filter logs [pattern|clear]"},
      {"copylogs", "copy logs to clipboard"},
      {"yanklogs", "copy logs to clipboard"},
      {"copyrelay", "copy selected relay URL"},
      {"yankrelay", "copy selected relay URL"},
      {"relayinfo", "print selected relay details"},
      {"ri", "print selected relay details"},
      {"wlogs", "write logs to file"},
      {"auth", "set NIP-42 key (nsec1...)"},
      {"help", "show help"},
      {"h", "show help"},
  };

  if (input.empty()) {
    return "q  cl  set  filter  auth  copylogs  copyrelay  relayinfo  wlogs  help";
  }

  const std::string lower = ToLowerCopy(input);
  std::string hint;
  int count = 0;
  for (const auto& entry : kCommands) {
    if (StartsWith(entry.name, lower)) {
      if (!hint.empty()) {
        hint += "  ";
      }
      hint += ":" + entry.name + "(" + entry.desc + ")";
      if (++count >= 5) {
        break;
      }
    }
  }
  return hint;
}

void ExecuteCommand(const std::string& raw_command, CommandState& state,
                    const std::shared_ptr<monitostr::model::LogBuffer>& log_buffer, const CommandCallbacks& callbacks) {
  const auto copy_text = callbacks.copy_text ? callbacks.copy_text : CopyTextToClipboardMac;
  const std::string raw = TrimCopy(raw_command);
  const std::string cmd = ToLowerCopy(raw);

  if (cmd == "q" || cmd == "quit" || cmd == "qa" || cmd == "qall") {
    if (log_buffer) {
      log_buffer->Info("Command :" + raw_command + " executed (exit)");
    }
    if (callbacks.request_exit) {
      callbacks.request_exit();
    }
  } else if (cmd == "clearlogs" || cmd == "cl") {
    if (log_buffer) {
      log_buffer->Clear();
      log_buffer->Info("Logs cleared by command :" + raw_command);
    }
    state.logs_scroll_lines = 0;
    state.logs_follow = true;
  } else if (cmd == "set follow") {
    state.logs_follow = true;
    if (log_buffer) {
      log_buffer->Info("Logs follow mode enabled (:set follow)");
    }
  } else if (cmd == "set nofollow") {
    state.logs_follow = false;
    if (log_buffer) {
      log_buffer->Info("Logs follow mode disabled (:set nofollow)");
    }
  } else if (cmd == "set nipswrap") {
    state.nips_wrap_selected_row = true;
    if (log_buffer) {
      log_buffer->Info("Selected-row NIPs wrapping enabled (:set nipswrap)");
    }
  } else if (cmd == "set nonipswrap") {
    state.nips_wrap_selected_row = false;
    if (log_buffer) {
      log_buffer->Info("Selected-row NIPs wrapping disabled (:set nonipswrap)");
    }
  } else if (cmd == "set compact") {
    state.compact_mode = CompactMode::kForceCompact;
    if (log_buffer) {
      log_buffer->Info("Compact layout forced (:set compact)");
    }
  } else if (cmd == "set nocompact") {
    state.compact_mode = CompactMode::kForceWide;
    if (log_buffer) {
      log_buffer->Info("Wide layout forced (:set nocompact)");
    }
  } else if (cmd == "set autocompact") {
    state.compact_mode = CompactMode::kAuto;
    if (log_buffer) {
      log_buffer->Info("Layout mode returned to auto detection (:set autocompact)");
    }
  } else if (StartsWith(cmd, "filter")) {
    std::string pattern;
    if (raw.size() > 6) {
      pattern = TrimCopy(raw.substr(6));
    }
    if (pattern.empty() || ToLowerCopy(pattern) == "clear") {
      state.logs_search_query.clear();
      state.logs_search_hit_ordinal = 0;
      if (log_buffer) {
        log_buffer->Info("Log filter cleared (:filter clear)");
      }
    } else {
      state.logs_search_query = pattern;
      state.logs_search_hit_ordinal = 0;
      state.logs_follow = false;
      if (log_buffer) {
        log_buffer->Info("Log filter set to: " + pattern);
      }
    }
  } else if (cmd == "copylogs" || cmd == "yanklogs") {
    if (log_buffer) {
      const auto logs = log_buffer->Snapshot();
      const std::string text = JoinLogsText(logs);
      if (copy_text(text)) {
        log_buffer->Info("Copied " + std::to_string(logs.size()) + " log lines to clipboard");
      } else {
        log_buffer->Error("Failed to copy logs to clipboard (pbcopy unavailable?)");
      }
    }
  } else if (cmd == "copyrelay" || cmd == "yankrelay") {
    const auto relay_url = callbacks.selected_relay_text ? callbacks.selected_relay_text() : std::nullopt;
    if (!relay_url.has_value() || relay_url->empty()) {
      if (log_buffer) {
        log_buffer->Warn("No relay is currently selected");
      }
    } else if (log_buffer) {
      if (copy_text(*relay_url)) {
        log_buffer->Info("Copied selected relay URL to clipboard: " + *relay_url);
      } else {
        log_buffer->Error("Failed to copy selected relay URL to clipboard");
      }
    }
  } else if (cmd == "relayinfo" || cmd == "ri") {
    const auto relay_report = callbacks.selected_relay_report ? callbacks.selected_relay_report() : std::nullopt;
    if (!relay_report.has_value() || relay_report->empty()) {
      if (log_buffer) {
        log_buffer->Warn("No relay is currently selected");
      }
    } else if (log_buffer) {
      log_buffer->Info(*relay_report);
    }
  } else if (StartsWith(cmd, "wlogs")) {
    std::string file_path;
    if (raw.size() > 5) {
      file_path = TrimCopy(raw.substr(5));
    }
    if (file_path.empty()) {
      if (log_buffer) {
        log_buffer->Warn("Usage: :wlogs <path>");
      }
    } else if (log_buffer) {
      const auto logs = log_buffer->Snapshot();
      const std::string text = JoinLogsText(logs);
      if (SaveTextToFile(file_path, text)) {
        log_buffer->Info("Saved " + std::to_string(logs.size()) + " log lines to " + file_path);
      } else {
        log_buffer->Error("Failed to save logs to " + file_path);
      }
    }
  } else if (StartsWith(cmd, "auth")) {
    std::string nsec_arg;
    if (raw.size() > 4) {
      nsec_arg = TrimCopy(raw.substr(4));
    }
    if (nsec_arg.empty()) {
      if (log_buffer) {
        log_buffer->Warn("Usage: :auth nsec1...");
      }
    } else if (callbacks.on_auth) {
      callbacks.on_auth(std::move(nsec_arg));
    } else if (log_buffer) {
      log_buffer->Warn("Auth handler not configured");
    }
  } else if (cmd == "help" || cmd == "h") {
    if (log_buffer) {
      log_buffer->Info(
          "Commands: :q :quit :qa :qall :clearlogs (:cl) :set follow :set nofollow "
          ":set nipswrap :set nonipswrap :set compact :set nocompact :set autocompact "
          ":filter <pattern> :filter clear "
          ":copylogs (:yanklogs) :copyrelay (:yankrelay) :relayinfo (:ri) :wlogs <path> "
          ":auth nsec1... (NIP-42 auth) "
          ":help");
    }
  } else if (!cmd.empty()) {
    if (log_buffer) {
      log_buffer->Warn("Unknown command :" + raw_command);
    }
  }
}

}  // namespace monitostr::ui
