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

#include "monitostr/ui/app.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

#include "monitostr/security/secure_memory.hpp"

namespace monitostr::ui {
namespace {

using monitostr::model::RelayStat;

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

bool LooksLikeNsecInput(const std::string& value) { return StartsWith(ToLowerCopy(TrimCopy(value)), "nsec1"); }

struct CommandEntry {
  std::string name;
  std::string desc;
};

// Returns a hint string listing commands that match the current command_line_ input.
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
      {"wlogs", "write logs to file"},
      {"auth", "set NIP-42 key (nsec1...)"},
      {"help", "show help"},
      {"h", "show help"},
  };
  if (input.empty()) {
    return "q  cl  set  filter  auth  copylogs  wlogs  help";
  }
  const std::string lower = ToLowerCopy(input);
  std::string hint;
  int count = 0;
  for (const auto& entry : kCommands) {
    if (StartsWith(entry.name, lower)) {
      if (!hint.empty()) hint += "  ";
      hint += ":" + entry.name + "(" + entry.desc + ")";
      if (++count >= 5) break;
    }
  }
  return hint;
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

std::string FormatNips(const std::vector<int>& nips) {
  if (nips.empty()) {
    return "-";
  }
  std::string out;
  for (std::size_t i = 0; i < nips.size(); ++i) {
    if (i > 0) {
      out += ",";
    }
    out += std::to_string(nips[i]);
  }
  return out;
}

std::string TruncateWithEllipsis(const std::string& value, std::size_t max_len) {
  if (value.size() <= max_len) {
    return value;
  }
  if (max_len <= 3) {
    return value.substr(0, max_len);
  }
  return value.substr(0, max_len - 3) + "...";
}

ftxui::Element RenderVerticalScrollBar(std::size_t total_items, std::size_t first_visible, std::size_t visible_items,
                                       std::size_t height, bool focused) {
  using namespace ftxui;

  if (height == 0) {
    return text("");
  }

  Elements cells;
  cells.reserve(height);

  const Color track_color = focused ? Color::RGB(120, 130, 155) : Color::RGB(90, 100, 120);
  const Color thumb_color = focused ? Color::RGB(97, 175, 239) : Color::RGB(130, 150, 185);

  std::size_t thumb_pos = 0;
  std::size_t thumb_size = height;
  if (total_items > 0 && visible_items > 0 && total_items > visible_items) {
    thumb_size = std::max<std::size_t>(1, (height * visible_items) / total_items);
    const std::size_t track_space = height > thumb_size ? height - thumb_size : 0;
    const std::size_t scroll_space = total_items - visible_items;
    thumb_pos = scroll_space > 0 ? (track_space * first_visible) / scroll_space : 0;
  }

  for (std::size_t i = 0; i < height; ++i) {
    const bool in_thumb = i >= thumb_pos && i < thumb_pos + thumb_size;
    cells.push_back(text(in_thumb ? "█" : "│") | color(in_thumb ? thumb_color : track_color));
  }

  return vbox(std::move(cells)) | size(WIDTH, EQUAL, 1) | size(HEIGHT, EQUAL, static_cast<int>(height));
}

ftxui::Element RenderLogs(const std::vector<monitostr::model::LogEntry>& logs, std::size_t scroll_lines, bool focused,
                          std::size_t visible_lines) {
  using namespace ftxui;

  const std::size_t total = logs.size();
  std::size_t start = 0;
  if (total > visible_lines) {
    const std::size_t max_start = total - visible_lines;
    start = (scroll_lines > max_start) ? max_start : scroll_lines;
  }

  Elements lines;
  for (std::size_t i = start; i < logs.size() && lines.size() < visible_lines; ++i) {
    const auto& entry = logs[i];
    lines.push_back(text(entry.timestamp + " " + entry.message));
  }

  if (lines.empty()) {
    lines.push_back(text("No logs yet."));
  }

  const std::string title_text = focused ? " LOGS [+] " : " LOGS ";
  auto title = text(title_text) | bold | color(focused ? Color::RGB(200, 220, 255) : Color::RGB(160, 170, 190));

  auto content = hbox({
      vbox(std::move(lines)) | size(HEIGHT, EQUAL, static_cast<int>(visible_lines)) | flex,
      separator(),
      RenderVerticalScrollBar(total, start, visible_lines, visible_lines, focused),
  });

  return vbox({title, separator(), content}) | border;
}

// FTXUI v6 removed sparkline; we use graph(GraphFunction) instead.
// GraphFunction signature: std::vector<int>(int width, int height)
// Each returned value is a column height in [0, height].
ftxui::Element RenderSparkline(const RelayStat& stat) {
  // Copy history into a plain vector for capture by value.
  std::vector<double> history(stat.latency_history_ms.begin(), stat.latency_history_ms.end());

  double max_val = 1.0;
  for (double v : history) {
    if (v > max_val) max_val = v;
  }

  auto graph_fn = [history, max_val](int width, int height) -> std::vector<int> {
    const int n = static_cast<int>(history.size());
    std::vector<int> result(static_cast<std::size_t>(width), 0);
    for (int col = 0; col < width; ++col) {
      // Right-align: most recent sample at rightmost column.
      const int src = n - width + col;
      if (src >= 0 && src < n) {
        result[static_cast<std::size_t>(col)] =
            static_cast<int>(history[static_cast<std::size_t>(src)] / max_val * static_cast<double>(height));
      }
    }
    return result;
  };

  return ftxui::graph(graph_fn) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 24) |
         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 2);
}

ftxui::Element RenderRelayTable(const std::vector<RelayStat>& stats, std::size_t selected_idx, bool focused,
                                bool compact, bool wrap_selected_nips, std::size_t visible_rows) {
  using namespace ftxui;

  const int relay_w = compact ? 24 : 36;
  const int status_w = compact ? 9 : 12;
  const int latency_w = compact ? 7 : 10;
  const int events_w = compact ? 6 : 8;
  const int nips_w = compact ? 18 : 24;
  const int spark_w = compact ? 0 : 24;
  const bool show_sparkline = !compact;

  std::vector<Element> rows;
  const std::string pane_title = focused ? " RELAYS [+] " : " RELAYS ";
  rows.push_back(text(pane_title) | bold | color(focused ? Color::RGB(200, 220, 255) : Color::RGB(160, 170, 190)));
  rows.push_back(separator());

  rows.push_back(hbox({
                     text("Relay") | bold | size(WIDTH, EQUAL, relay_w),
                     separator(),
                     text("Status") | bold | size(WIDTH, EQUAL, status_w),
                     separator(),
                     text("Lat") | bold | size(WIDTH, EQUAL, latency_w),
                     separator(),
                     text("Ev") | bold | size(WIDTH, EQUAL, events_w),
                     separator(),
                     text("NIPs") | bold | size(WIDTH, EQUAL, nips_w),
                     separator(),
                     show_sparkline ? text("Spark") | bold : text(""),
                 }) |
                 bold | color(Color::RGB(185, 195, 215)));

  rows.push_back(separator());

  std::size_t start = 0;
  if (visible_rows > 0 && selected_idx >= visible_rows) {
    start = selected_idx - visible_rows + 1;
  }
  if (start > stats.size()) {
    start = 0;
  }
  const std::size_t end = visible_rows == 0 ? stats.size() : std::min(stats.size(), start + visible_rows);

  for (std::size_t idx = start; idx < end; ++idx) {
    const auto& stat = stats[idx];
    std::string latency = "-";
    if (stat.latency_ms.has_value()) {
      latency = std::to_string(static_cast<int>(*stat.latency_ms)) + " ms";
    }
    const std::string nips_full = FormatNips(stat.supported_nips);
    const bool is_selected = idx == selected_idx;
    const std::string nips_compact = TruncateWithEllipsis(nips_full, static_cast<std::size_t>(nips_w - 2));

    Element nips_cell;
    if (is_selected && wrap_selected_nips) {
      nips_cell = paragraph(nips_full) | size(WIDTH, EQUAL, nips_w) | size(HEIGHT, LESS_THAN, compact ? 3 : 5);
    } else {
      nips_cell = text(nips_compact) | size(WIDTH, EQUAL, nips_w);
    }

    Elements cols = {
        text(TruncateWithEllipsis(stat.relay_url, static_cast<std::size_t>(relay_w - 1))) | size(WIDTH, EQUAL, relay_w),
        separator(),
        text(monitostr::model::ToString(stat.status)) | size(WIDTH, EQUAL, status_w),
        separator(),
        text(latency) | size(WIDTH, EQUAL, latency_w),
        separator(),
        text(std::to_string(stat.events_count)) | size(WIDTH, EQUAL, events_w),
        separator(),
        nips_cell,
    };

    if (show_sparkline) {
      cols.push_back(separator());
      cols.push_back(RenderSparkline(stat) | size(WIDTH, EQUAL, spark_w));
    }

    auto row = hbox(std::move(cols));

    if (is_selected) {
      row = row | bgcolor(Color::RGB(48, 54, 70)) | color(Color::RGB(230, 235, 255));
    }
    rows.push_back(row);
    rows.push_back(separator());
  }

  const std::size_t shown_rows = end > start ? end - start : 0;
  const std::size_t relay_panel_height = 4 + shown_rows * 2;

  auto body = hbox({
                  vbox(std::move(rows)) | flex,
                  separator(),
                  RenderVerticalScrollBar(stats.size(), start, visible_rows, relay_panel_height, focused),
              }) |
              border;
  if (focused) {
    body = body | color(Color::RGB(215, 225, 245));
  }
  return body;
}

}  // namespace

App::App(std::shared_ptr<monitostr::model::RelayStats> shared_stats,
         std::shared_ptr<monitostr::model::LogBuffer> log_buffer, NpubSubmit on_submit, NsecSubmit on_auth)
    : shared_stats_(std::move(shared_stats)),
      log_buffer_(std::move(log_buffer)),
      on_submit_(std::move(on_submit)),
      on_auth_(std::move(on_auth)) {}

void App::SetHeaderContext(HeaderContext context) { header_context_ = std::move(context); }

ftxui::Component App::BuildComponentTree() {
  using namespace ftxui;

  auto renderer = Renderer([this] {
    using namespace ftxui;

    const auto term = Terminal::Size();
    bool compact = term.dimx < 140;
    if (compact_mode_ == CompactMode::kForceCompact) {
      compact = true;
    } else if (compact_mode_ == CompactMode::kForceWide) {
      compact = false;
    }
    const bool tiny = term.dimx < 110;
    const std::size_t relay_rows = 10U;
    const std::size_t log_lines = 8U;

    const auto snapshot = shared_stats_->Snapshot();
    const auto aggregate = shared_stats_->Aggregate();
    const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
    if (logs_follow_) {
      if (logs.size() > log_lines) {
        logs_scroll_lines_ = logs.size() - log_lines;
      } else {
        logs_scroll_lines_ = 0;
      }
    }
    if (!snapshot.empty() && selected_relay_ >= snapshot.size()) {
      selected_relay_ = snapshot.size() - 1;
    }

    std::vector<monitostr::model::LogEntry> visible_logs;
    std::vector<std::size_t> hit_positions;
    visible_logs.reserve(logs.size());

    if (logs_search_query_.empty()) {
      visible_logs = logs;
    } else {
      const std::string q = ToLowerCopy(logs_search_query_);
      for (const auto& entry : logs) {
        const std::string line = ToLowerCopy(entry.timestamp + " " + entry.message);
        if (line.find(q) != std::string::npos) {
          visible_logs.push_back(entry);
        }
      }
    }

    const bool relays_focused = active_pane_ == ActivePane::kRelays;
    const bool logs_focused = active_pane_ == ActivePane::kLogs;
    const std::string mode_label =
        mode_ == UiMode::kInsert
            ? "INSERT"
            : (mode_ == UiMode::kSearch ? "SEARCH" : (mode_ == UiMode::kCommand ? "COMMAND" : "NORMAL"));
    const std::string layout_label =
        compact_mode_ == CompactMode::kAuto
            ? (compact ? "compact(auto)" : "wide(auto)")
            : (compact_mode_ == CompactMode::kForceCompact ? "compact(forced)" : "wide(forced)");
    const std::string focus_label = relays_focused ? "Relays" : "Logs";

    if (logs_follow_) {
      if (visible_logs.size() > log_lines) {
        logs_scroll_lines_ = visible_logs.size() - log_lines;
      } else {
        logs_scroll_lines_ = 0;
      }
    }

    const std::string npub_line =
        compact ? TruncateWithEllipsis(header_context_.npub, tiny ? 40 : 70) : header_context_.npub;
    const std::string hex_line =
        compact ? TruncateWithEllipsis(header_context_.hex_pubkey, tiny ? 28 : 48) : header_context_.hex_pubkey;

    const auto mode_color = mode_ == UiMode::kInsert
                                ? Color::RGB(110, 170, 255)
                                : (mode_ == UiMode::kSearch ? Color::RGB(235, 198, 107)
                                                            : (mode_ == UiMode::kCommand ? Color::RGB(198, 160, 246)
                                                                                         : Color::RGB(152, 195, 121)));

    // Context-sensitive statusline content (always pinned to bottom).
    Element bottom_bar;
    if (mode_ == UiMode::kInsert) {
      bottom_bar = hbox({
                       text(" INSERT ") | bold | bgcolor(mode_color) | color(Color::Black),
                       text("  npub/nsec: "),
                       text(input_npub_ + "_"),
                   }) |
                   bgcolor(Color::RGB(34, 38, 49)) | color(Color::RGB(210, 218, 236));
    } else if (mode_ == UiMode::kSearch) {
      bottom_bar = hbox({
                       text(" SEARCH ") | bold | bgcolor(mode_color) | color(Color::Black),
                       text("  /"),
                       text(logs_search_query_ + "_"),
                   }) |
                   bgcolor(Color::RGB(34, 38, 49)) | color(Color::RGB(210, 218, 236));
    } else if (mode_ == UiMode::kCommand) {
      const std::string hint = ComputeCommandHint(command_line_);
      bottom_bar = vbox({
          hbox({
              text(" CMD ") | bold | bgcolor(mode_color) | color(Color::Black),
              text("  :"),
              text(command_line_ + "_"),
          }) | bgcolor(Color::RGB(34, 38, 49)) |
              color(Color::RGB(210, 218, 236)),
          hbox({
              text("  "),
              text(hint) | color(Color::RGB(110, 120, 145)),
          }) | bgcolor(Color::RGB(28, 32, 42)),
      });
    } else {
      bottom_bar = hbox({
                       text(" " + mode_label + " ") | bold | bgcolor(mode_color) | color(Color::Black),
                       text("  [" + focus_label + "]") | color(Color::RGB(210, 218, 236)),
                       text("  follow:") | color(Color::RGB(150, 160, 180)),
                       text(logs_follow_ ? "on" : "off") |
                           color(logs_follow_ ? Color::RGB(152, 195, 121) : Color::RGB(150, 160, 180)),
                       text("  " + layout_label) | color(Color::RGB(150, 160, 180)),
                       filler(),
                       text("h/l  j/k  gg/G  Ctrl-d/u  /  :") | color(Color::RGB(150, 160, 180)),
                   }) |
                   bgcolor(Color::RGB(34, 38, 49)) | color(Color::RGB(210, 218, 236));
    }

    auto main_body =
        vbox({
            hbox({
                text(" MONITOSTR ") | bold | bgcolor(Color::RGB(97, 175, 239)) | color(Color::Black),
                text(" relay monitor") | color(Color::RGB(150, 160, 180)),
                filler(),
                text(" connected:" + std::to_string(aggregate.connected_relays) + "  err:" +
                     std::to_string(aggregate.errored_relays) + "  events:" + std::to_string(aggregate.total_events)) |
                    color(Color::RGB(150, 160, 180)),
            }) | bgcolor(Color::RGB(30, 34, 45)),
            text(" npub " + npub_line) | color(Color::RGB(170, 180, 205)),
            text(" hex  " + hex_line) | color(Color::RGB(170, 180, 205)),
            separator(),
            RenderRelayTable(snapshot, selected_relay_, relays_focused, compact, nips_wrap_selected_row_, relay_rows),
            separator(),
            RenderLogs(visible_logs, logs_scroll_lines_, logs_focused, log_lines),
        }) |
        border | bgcolor(Color::RGB(24, 26, 33));

    return vbox({
        main_body | flex,
        bottom_bar,
    });
  });

  auto interactive = CatchEvent(renderer, [this](Event event) {
    constexpr std::size_t kLogPageLines = 8;

    auto next_page = [](std::size_t value, std::size_t delta) -> std::size_t { return value + delta; };
    auto prev_page = [](std::size_t value, std::size_t delta) -> std::size_t {
      return value > delta ? value - delta : 0;
    };

    if (mode_ == UiMode::kCommand) {
      if (event == Event::Escape) {
        monitostr::security::SecureClearString(command_line_);
        mode_ = UiMode::kNormal;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Return) {
        const std::string raw = TrimCopy(command_line_);
        const std::string cmd = ToLowerCopy(raw);
        if (cmd == "q" || cmd == "quit" || cmd == "qa" || cmd == "qall") {
          if (log_buffer_) {
            log_buffer_->Info("Command :" + command_line_ + " executed (exit)");
          }
          if (request_exit_) {
            request_exit_();
          }
        } else if (cmd == "clearlogs" || cmd == "cl") {
          if (log_buffer_) {
            log_buffer_->Clear();
            log_buffer_->Info("Logs cleared by command :" + command_line_);
          }
          logs_scroll_lines_ = 0;
          logs_follow_ = true;
        } else if (cmd == "set follow") {
          logs_follow_ = true;
          if (log_buffer_) {
            log_buffer_->Info("Logs follow mode enabled (:set follow)");
          }
        } else if (cmd == "set nofollow") {
          logs_follow_ = false;
          if (log_buffer_) {
            log_buffer_->Info("Logs follow mode disabled (:set nofollow)");
          }
        } else if (cmd == "set nipswrap") {
          nips_wrap_selected_row_ = true;
          if (log_buffer_) {
            log_buffer_->Info("Selected-row NIPs wrapping enabled (:set nipswrap)");
          }
        } else if (cmd == "set nonipswrap") {
          nips_wrap_selected_row_ = false;
          if (log_buffer_) {
            log_buffer_->Info("Selected-row NIPs wrapping disabled (:set nonipswrap)");
          }
        } else if (cmd == "set compact") {
          compact_mode_ = CompactMode::kForceCompact;
          if (log_buffer_) {
            log_buffer_->Info("Compact layout forced (:set compact)");
          }
        } else if (cmd == "set nocompact") {
          compact_mode_ = CompactMode::kForceWide;
          if (log_buffer_) {
            log_buffer_->Info("Wide layout forced (:set nocompact)");
          }
        } else if (cmd == "set autocompact") {
          compact_mode_ = CompactMode::kAuto;
          if (log_buffer_) {
            log_buffer_->Info("Layout mode returned to auto detection (:set autocompact)");
          }
        } else if (StartsWith(cmd, "filter")) {
          std::string pattern;
          if (raw.size() > 6) {
            pattern = TrimCopy(raw.substr(6));
          }
          if (pattern.empty() || ToLowerCopy(pattern) == "clear") {
            logs_search_query_.clear();
            logs_search_hit_ordinal_ = 0;
            if (log_buffer_) {
              log_buffer_->Info("Log filter cleared (:filter clear)");
            }
          } else {
            logs_search_query_ = pattern;
            logs_search_hit_ordinal_ = 0;
            logs_follow_ = false;
            if (log_buffer_) {
              log_buffer_->Info("Log filter set to: " + pattern);
            }
          }
        } else if (cmd == "copylogs" || cmd == "yanklogs") {
          if (log_buffer_) {
            const auto logs = log_buffer_->Snapshot();
            const std::string text = JoinLogsText(logs);
            if (CopyTextToClipboardMac(text)) {
              log_buffer_->Info("Copied " + std::to_string(logs.size()) + " log lines to clipboard");
            } else {
              log_buffer_->Error("Failed to copy logs to clipboard (pbcopy unavailable?)");
            }
          }
        } else if (StartsWith(cmd, "wlogs")) {
          std::string file_path;
          if (raw.size() > 5) {
            file_path = TrimCopy(raw.substr(5));
          }
          if (file_path.empty()) {
            if (log_buffer_) {
              log_buffer_->Warn("Usage: :wlogs <path>");
            }
          } else if (log_buffer_) {
            const auto logs = log_buffer_->Snapshot();
            const std::string text = JoinLogsText(logs);
            if (SaveTextToFile(file_path, text)) {
              log_buffer_->Info("Saved " + std::to_string(logs.size()) + " log lines to " + file_path);
            } else {
              log_buffer_->Error("Failed to save logs to " + file_path);
            }
          }
        } else if (StartsWith(cmd, "auth")) {
          std::string nsec_arg;
          if (raw.size() > 4) {
            nsec_arg = TrimCopy(raw.substr(4));
          }
          if (nsec_arg.empty()) {
            if (log_buffer_) {
              log_buffer_->Warn("Usage: :auth nsec1...");
            }
          } else if (on_auth_) {
            on_auth_(std::move(nsec_arg));
          } else if (log_buffer_) {
            log_buffer_->Warn("Auth handler not configured");
          }
        } else if (cmd == "help" || cmd == "h") {
          if (log_buffer_) {
            log_buffer_->Info(
                "Commands: :q :quit :qa :qall :clearlogs (:cl) :set follow :set nofollow "
                ":set nipswrap :set nonipswrap :set compact :set nocompact :set autocompact "
                ":filter <pattern> :filter clear "
                ":copylogs (:yanklogs) :wlogs <path> "
                ":auth nsec1... (NIP-42 auth) "
                ":help");
          }
        } else if (!cmd.empty()) {
          if (log_buffer_) {
            log_buffer_->Warn("Unknown command :" + command_line_);
          }
        }
        monitostr::security::SecureClearString(command_line_);
        mode_ = UiMode::kNormal;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Backspace) {
        if (!command_line_.empty()) {
          monitostr::security::SecurePopBack(command_line_);
        }
        return true;
      }
      if (event.is_character()) {
        command_line_ += event.character();
        return true;
      }
      return false;
    }

    if (mode_ == UiMode::kSearch) {
      if (event == Event::Escape) {
        mode_ = UiMode::kNormal;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Return) {
        mode_ = UiMode::kNormal;
        logs_search_hit_ordinal_ = 0;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Backspace) {
        if (!logs_search_query_.empty()) {
          logs_search_query_.pop_back();
        }
        return true;
      }
      if (event.is_character()) {
        logs_search_query_ += event.character();
        return true;
      }
      return false;
    }

    if (mode_ == UiMode::kInsert) {
      if (event == Event::Escape) {
        monitostr::security::SecureClearString(input_npub_);
        mode_ = UiMode::kNormal;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Return) {
        std::string submitted_input = TrimCopy(input_npub_);
        monitostr::security::SecureClearString(input_npub_);
        if (!submitted_input.empty()) {
          if (LooksLikeNsecInput(submitted_input) && on_auth_) {
            on_auth_(std::move(submitted_input));
          } else {
            on_submit_(std::move(submitted_input));
          }
        }
        mode_ = UiMode::kNormal;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Backspace) {
        if (!input_npub_.empty()) {
          monitostr::security::SecurePopBack(input_npub_);
        }
        return true;
      }
      if (event.is_character()) {
        input_npub_ += event.character();
        return true;
      }
      return false;
    }

    if (event == Event::Character('i')) {
      mode_ = UiMode::kInsert;
      pending_g_ = false;
      return true;
    }
    if (event == Event::Character(':')) {
      mode_ = UiMode::kCommand;
      monitostr::security::SecureClearString(command_line_);
      pending_g_ = false;
      return true;
    }
    if (event == Event::Character('/')) {
      active_pane_ = ActivePane::kLogs;
      mode_ = UiMode::kSearch;
      logs_follow_ = false;
      pending_g_ = false;
      return true;
    }
    if (event == Event::Character('h')) {
      active_pane_ = ActivePane::kRelays;
      pending_g_ = false;
      return true;
    }
    if (event == Event::Character('l')) {
      active_pane_ = ActivePane::kLogs;
      pending_g_ = false;
      return true;
    }
    if (event == Event::Character('f')) {
      logs_follow_ = !logs_follow_;
      pending_g_ = false;
      return true;
    }

    if (event == Event::Character('g')) {
      if (pending_g_) {
        selected_relay_ = 0;
        logs_scroll_lines_ = 0;
        pending_g_ = false;
      } else {
        pending_g_ = true;
      }
      return true;
    }
    pending_g_ = false;

    if (event == Event::Character('G')) {
      if (active_pane_ == ActivePane::kRelays) {
        const auto snapshot = shared_stats_->Snapshot();
        if (!snapshot.empty()) {
          selected_relay_ = snapshot.size() - 1;
        }
      } else {
        const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
        if (logs.size() > kLogPageLines) {
          logs_scroll_lines_ = logs.size() - kLogPageLines;
        }
      }
      return true;
    }

    if (event == Event::Character('0')) {
      if (active_pane_ == ActivePane::kRelays) {
        selected_relay_ = 0;
      } else {
        logs_scroll_lines_ = 0;
        logs_follow_ = false;
      }
      return true;
    }

    if (event == Event::Character('$')) {
      if (active_pane_ == ActivePane::kRelays) {
        const auto snapshot = shared_stats_->Snapshot();
        if (!snapshot.empty()) {
          selected_relay_ = snapshot.size() - 1;
        }
      } else {
        const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
        logs_scroll_lines_ = logs.size() > kLogPageLines ? logs.size() - kLogPageLines : 0;
        logs_follow_ = false;
      }
      return true;
    }

    if (event == Event::Character('n') || event == Event::Character('N')) {
      if (active_pane_ == ActivePane::kLogs && !logs_search_query_.empty()) {
        const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
        std::vector<std::size_t> matches;
        const std::string q = ToLowerCopy(logs_search_query_);
        for (std::size_t i = 0; i < logs.size(); ++i) {
          const std::string line = ToLowerCopy(logs[i].timestamp + " " + logs[i].message);
          if (line.find(q) != std::string::npos) {
            matches.push_back(i);
          }
        }
        if (!matches.empty()) {
          if (event == Event::Character('n')) {
            logs_search_hit_ordinal_ = (logs_search_hit_ordinal_ + 1) % matches.size();
          } else {
            logs_search_hit_ordinal_ = (logs_search_hit_ordinal_ + matches.size() - 1) % matches.size();
          }
          const std::size_t pos = matches[logs_search_hit_ordinal_];
          logs_scroll_lines_ = pos > 2 ? pos - 2 : 0;
          logs_follow_ = false;
        }
      }
      return true;
    }

    if (event == Event::Character('j')) {
      if (active_pane_ == ActivePane::kRelays) {
        const auto snapshot = shared_stats_->Snapshot();
        if (!snapshot.empty() && selected_relay_ + 1 < snapshot.size()) {
          ++selected_relay_;
        }
      } else {
        ++logs_scroll_lines_;
        logs_follow_ = false;
      }
      return true;
    }

    if (event == Event::Character('k')) {
      if (active_pane_ == ActivePane::kRelays) {
        if (selected_relay_ > 0) {
          --selected_relay_;
        }
      } else {
        if (logs_scroll_lines_ > 0) {
          --logs_scroll_lines_;
        }
        logs_follow_ = false;
      }
      return true;
    }

    if (event == Event::CtrlD || event == Event::PageDown || event == Event::Character('d')) {
      if (active_pane_ == ActivePane::kLogs) {
        logs_scroll_lines_ = next_page(logs_scroll_lines_, kLogPageLines);
        logs_follow_ = false;
        return true;
      }
      if (active_pane_ == ActivePane::kRelays) {
        const auto snapshot = shared_stats_->Snapshot();
        if (!snapshot.empty()) {
          selected_relay_ = std::min(selected_relay_ + 5, snapshot.size() - 1);
        }
        return true;
      }
    }

    if (event == Event::CtrlU || event == Event::PageUp || event == Event::Character('u')) {
      if (active_pane_ == ActivePane::kLogs) {
        logs_scroll_lines_ = prev_page(logs_scroll_lines_, kLogPageLines);
        logs_follow_ = false;
        return true;
      }
      if (active_pane_ == ActivePane::kRelays) {
        selected_relay_ = selected_relay_ > 5 ? selected_relay_ - 5 : 0;
        return true;
      }
    }

    if (event == Event::Escape) {
      mode_ = UiMode::kNormal;
      return true;
    }

    return false;
  });

  return interactive;
}

void App::Run() {
  using namespace std::chrono_literals;
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  request_exit_ = screen.ExitLoopClosure();
  auto root = BuildComponentTree();

  std::atomic<bool> running = true;
  std::thread ticker([&] {
    while (running.load()) {
      std::this_thread::sleep_for(250ms);
      screen.PostEvent(ftxui::Event::Custom);
    }
  });

  screen.Loop(root);
  running.store(false);
  ticker.join();
  request_exit_ = nullptr;
}

}  // namespace monitostr::ui
