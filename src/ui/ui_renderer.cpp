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

#include "monitostr/ui/ui_renderer.hpp"

#include <algorithm>
#include "monitostr/ui/command_processor.hpp"
namespace monitostr::ui {
namespace {

using monitostr::model::RelayStat;

struct Palette {
  ftxui::Color bg = ftxui::Color::RGB(18, 22, 30);
  ftxui::Color panel = ftxui::Color::RGB(27, 33, 44);
  ftxui::Color panel_alt = ftxui::Color::RGB(33, 39, 52);
  ftxui::Color panel_focus = ftxui::Color::RGB(39, 48, 64);
  ftxui::Color border = ftxui::Color::RGB(71, 84, 105);
  ftxui::Color border_focus = ftxui::Color::RGB(111, 168, 220);
  ftxui::Color text = ftxui::Color::RGB(222, 230, 242);
  ftxui::Color text_muted = ftxui::Color::RGB(144, 157, 178);
  ftxui::Color accent = ftxui::Color::RGB(88, 166, 255);
  ftxui::Color accent_warm = ftxui::Color::RGB(244, 196, 99);
  ftxui::Color success = ftxui::Color::RGB(125, 211, 166);
  ftxui::Color warning = ftxui::Color::RGB(244, 179, 80);
  ftxui::Color danger = ftxui::Color::RGB(240, 113, 120);
};

const Palette& UiPalette() {
  static const Palette palette;
  return palette;
}

std::string StripLevelPrefix(const std::string& message) {
  if (message.size() > 8 && message[0] == '[') {
    const std::size_t close = message.find("] ");
    if (close != std::string::npos) {
      return message.substr(close + 2);
    }
  }
  return message;
}

ftxui::Color StatusColor(monitostr::model::RelayStatus status) {
  const auto& palette = UiPalette();
  switch (status) {
    case monitostr::model::RelayStatus::kSubscribed:
      return palette.success;
    case monitostr::model::RelayStatus::kError:
      return palette.danger;
    case monitostr::model::RelayStatus::kResolving:
    case monitostr::model::RelayStatus::kConnecting:
    case monitostr::model::RelayStatus::kTlsHandshake:
    case monitostr::model::RelayStatus::kWsHandshake:
      return palette.accent_warm;
    case monitostr::model::RelayStatus::kDisconnected:
      return palette.text_muted;
  }
  return palette.text_muted;
}

ftxui::Color LogLevelColor(monitostr::model::LogLevel level) {
  const auto& palette = UiPalette();
  switch (level) {
    case monitostr::model::LogLevel::kInfo:
      return palette.accent;
    case monitostr::model::LogLevel::kWarn:
      return palette.warning;
    case monitostr::model::LogLevel::kError:
      return palette.danger;
  }
  return palette.text_muted;
}

ftxui::Element RenderChip(const std::string& label, ftxui::Color fg, ftxui::Color bg) {
  using namespace ftxui;
  return text(" " + label + " ") | color(fg) | bgcolor(bg) | bold;
}

ftxui::Element RenderMetricCard(const std::string& label, const std::string& value, ftxui::Color accent) {
  using namespace ftxui;
  const auto& palette = UiPalette();
  return vbox({
             text(label) | color(palette.text_muted),
             text(value) | bold | color(accent),
         }) |
         bgcolor(palette.panel_alt) | color(palette.text) | border;
}

ftxui::Element RenderInfoLine(const std::string& label, const std::string& value, ftxui::Color value_color) {
  using namespace ftxui;
  const auto& palette = UiPalette();
  return hbox({
             text(label) | color(palette.text_muted),
             text(value) | color(value_color),
         }) |
         bgcolor(palette.panel_alt);
}

ftxui::Element RenderPanelTitle(const std::string& title, const std::string& meta, bool focused) {
  using namespace ftxui;
  const auto& palette = UiPalette();
  return hbox({
      text(focused ? ">" : " ") | color(focused ? palette.accent : palette.text_muted),
      text(" " + title) | bold | color(focused ? palette.text : palette.text_muted),
      filler(),
      text(meta) | color(palette.text_muted),
  });
}

ftxui::Element RenderVerticalScrollBar(std::size_t total_items, std::size_t first_visible, std::size_t visible_items,
                                       std::size_t height, bool focused) {
  using namespace ftxui;
  const auto& palette = UiPalette();

  if (height == 0) {
    return text("");
  }

  Elements cells;
  cells.reserve(height);

  const Color track_color = focused ? palette.border_focus : palette.border;
  const Color thumb_color = focused ? palette.accent : palette.text_muted;

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

ftxui::Element RenderLogs(const RenderContext& context, bool focused) {
  using namespace ftxui;
  const auto& palette = UiPalette();

  const std::size_t total = context.visible_logs.size();
  std::size_t start = 0;
  if (total > context.log_lines) {
    const std::size_t max_start = total - context.log_lines;
    start = (context.logs_scroll_lines > max_start) ? max_start : context.logs_scroll_lines;
  }

  Elements lines;
  for (std::size_t i = start; i < context.visible_logs.size() && lines.size() < context.log_lines; ++i) {
    const auto& entry = context.visible_logs[i];
    const std::string level_label = entry.level == monitostr::model::LogLevel::kInfo
                                        ? "info"
                                        : (entry.level == monitostr::model::LogLevel::kWarn ? "warn" : "error");
    lines.push_back(hbox({
                        text(entry.timestamp) | color(palette.text_muted),
                        text(" "),
                        RenderChip(level_label, LogLevelColor(entry.level), palette.panel_alt),
                        text(" "),
                        text(StripLevelPrefix(entry.message)) | color(palette.text),
                    }) |
                    bgcolor((i % 2 == 0) ? palette.panel : palette.panel_alt));
  }

  const LogStreamSummary summary = SummarizeLogStream(context);
  if (lines.empty()) {
    lines.push_back(text(summary.empty_text) | color(palette.text_muted));
  }

  auto title = RenderPanelTitle("LOG STREAM", summary.meta, focused);
  auto content = hbox({
      vbox(std::move(lines)) | size(HEIGHT, EQUAL, static_cast<int>(context.log_lines)) | flex,
      separator(),
      RenderVerticalScrollBar(total, start, context.log_lines, context.log_lines, focused),
  });

  return vbox({title, separator(), content}) | border | color(focused ? palette.border_focus : palette.border) |
         bgcolor(palette.panel);
}

ftxui::Element RenderSparkline(const RelayStat& stat) {
  const std::deque<double>& history = stat.latency_history_ms;

  double max_val = 1.0;
  for (double v : history) {
    if (v > max_val) {
      max_val = v;
    }
  }

  auto graph_fn = [&history, max_val](int width, int height) -> std::vector<int> {
    const int n = static_cast<int>(history.size());
    std::vector<int> result(static_cast<std::size_t>(width), 0);
    for (int col = 0; col < width; ++col) {
      const int src = n - width + col;
      if (src >= 0 && src < n) {
        result[static_cast<std::size_t>(col)] =
            static_cast<int>(history[static_cast<std::size_t>(src)] / max_val * static_cast<double>(height));
      }
    }
    return result;
  };

  return ftxui::graph(graph_fn) | ftxui::color(UiPalette().accent) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 24) |
         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 2);
}

ftxui::Element RenderRelayTable(const RenderContext& context, bool focused) {
  using namespace ftxui;
  const auto& palette = UiPalette();

  const int index_w = context.compact ? 4 : 5;
  const int relay_w = context.compact ? 21 : 31;
  const int status_w = context.compact ? 9 : 12;
  const int latency_w = context.compact ? 7 : 10;
  const int events_w = context.compact ? 6 : 8;
  const int nips_w = context.compact ? 12 : 16;
  const int spark_w = context.compact ? 0 : 24;
  const bool show_sparkline = !context.compact;
  const RelayListViewport viewport =
      ComputeRelayListViewport(context.relay_stats.size(), context.selected_relay, context.relay_rows);

  std::vector<Element> rows;
  rows.push_back(RenderPanelTitle("RELAYS", viewport.meta, focused));
  rows.push_back(separator());
  rows.push_back(hbox({
                     text("#") | bold | size(WIDTH, EQUAL, index_w),
                     separator(),
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
                 bold | color(palette.text_muted));
  rows.push_back(separator());

  if (context.relay_stats.empty()) {
    rows.push_back(text("Waiting for relay discovery...") | color(palette.text_muted));
    rows.push_back(separator());
  }

  for (std::size_t idx = viewport.start; idx < viewport.end; ++idx) {
    const auto& stat = context.relay_stats[idx];
    std::string latency = "-";
    if (stat.latency_ms.has_value()) {
      latency = std::to_string(static_cast<int>(*stat.latency_ms)) + " ms";
    }
    const std::string nips_full = FormatNips(stat.supported_nips);
    const bool is_selected = idx == context.selected_relay;
    const std::string nips_compact = TruncateWithEllipsis(nips_full, static_cast<std::size_t>(nips_w - 1));
    std::string ordinal = std::to_string(idx + 1);
    if (ordinal.size() > static_cast<std::size_t>(index_w)) {
      ordinal.erase(static_cast<std::size_t>(index_w) - 1);
      ordinal += "+";
    }

    Elements cols = {
        text(ordinal) | color(is_selected ? palette.accent : palette.text_muted) | size(WIDTH, EQUAL, index_w),
        separator(),
        text(TruncateWithEllipsis(stat.relay_url, static_cast<std::size_t>(relay_w - 1))) | size(WIDTH, EQUAL, relay_w),
        separator(),
        RenderChip(monitostr::model::ToString(stat.status), StatusColor(stat.status), palette.panel_alt) |
            size(WIDTH, EQUAL, status_w),
        separator(),
        text(latency) | color(stat.latency_ms.has_value() ? palette.text : palette.text_muted) |
            size(WIDTH, EQUAL, latency_w),
        separator(),
        text(std::to_string(stat.events_count)) | color(stat.events_count > 0 ? palette.accent : palette.text_muted) |
            size(WIDTH, EQUAL, events_w),
        separator(),
        text(nips_compact) | size(WIDTH, EQUAL, nips_w),
    };

    if (show_sparkline) {
      cols.push_back(separator());
      cols.push_back(RenderSparkline(stat) | size(WIDTH, EQUAL, spark_w));
    }

    auto row = hbox(std::move(cols));
    if (is_selected) {
      row = row | bgcolor(palette.panel_focus) | color(palette.text);
    } else {
      row = row | bgcolor((idx % 2 == 0) ? palette.panel : palette.panel_alt) | color(palette.text);
    }
    rows.push_back(row);
    rows.push_back(separator());
  }

  const std::size_t shown_rows = viewport.end > viewport.start ? viewport.end - viewport.start : 0;
  const std::size_t relay_panel_height = 4 + shown_rows * 2;
  auto body = hbox({
                  vbox(std::move(rows)) | flex,
                  separator(),
                  RenderVerticalScrollBar(context.relay_stats.size(), viewport.start, context.relay_rows,
                                          relay_panel_height, focused),
              }) |
              border;
  return body | color(focused ? palette.border_focus : palette.border) | bgcolor(palette.panel);
}

std::string ModeLabel(RenderMode mode) {
  switch (mode) {
    case RenderMode::kInsert:
      return "INSERT";
    case RenderMode::kSearch:
      return "SEARCH";
    case RenderMode::kCommand:
      return "COMMAND";
    case RenderMode::kNormal:
      return "NORMAL";
  }
  return "NORMAL";
}

ftxui::Color ModeColor(RenderMode mode) {
  const auto& palette = UiPalette();
  switch (mode) {
    case RenderMode::kInsert:
      return palette.accent;
    case RenderMode::kSearch:
      return palette.accent_warm;
    case RenderMode::kCommand:
      return ftxui::Color::RGB(174, 138, 255);
    case RenderMode::kNormal:
      return palette.success;
  }
  return palette.success;
}

}  // namespace

std::string TruncateWithEllipsis(const std::string& value, std::size_t max_len) {
  if (value.size() <= max_len) {
    return value;
  }
  if (max_len <= 3) {
    return value.substr(0, max_len);
  }
  return value.substr(0, max_len - 3) + "...";
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

RelayListViewport ComputeRelayListViewport(std::size_t total_relays, std::size_t selected_relay,
                                           std::size_t visible_rows) {
  RelayListViewport viewport;
  if (total_relays == 0) {
    viewport.meta = "0 discovered";
    return viewport;
  }

  const std::size_t clamped_visible = visible_rows == 0 ? total_relays : std::min(visible_rows, total_relays);
  const std::size_t clamped_selected = std::min(selected_relay, total_relays - 1);
  const std::size_t preferred_offset = clamped_visible > 2 ? clamped_visible / 2 : 0;
  std::size_t start = clamped_selected > preferred_offset ? clamped_selected - preferred_offset : 0;
  if (start + clamped_visible > total_relays) {
    start = total_relays - clamped_visible;
  }

  viewport.start = start;
  viewport.end = start + clamped_visible;
  viewport.meta =
      std::to_string(start + 1) + "-" + std::to_string(viewport.end) + " of " + std::to_string(total_relays);
  return viewport;
}

LogStreamSummary SummarizeLogStream(const RenderContext& context) {
  const std::size_t visible_logs = context.visible_logs.size();
  LogStreamSummary summary{
      .meta = std::to_string(context.total_logs) + " lines",
      .empty_text = "No logs yet.",
  };
  if (!context.logs_search_query.empty()) {
    summary.meta = std::to_string(visible_logs) + "/" + std::to_string(context.total_logs) + " visible";
    summary.empty_text = "No logs match the current search.";
  }
  return summary;
}

SelectedRelaySummary SummarizeSelectedRelay(const RenderContext& context) {
  const RelayStat* selected_relay = context.relay_stats.empty() || context.selected_relay >= context.relay_stats.size()
                                        ? nullptr
                                        : &context.relay_stats[context.selected_relay];
  return {
      .relay_summary = selected_relay == nullptr
                           ? "No relay selected"
                           : TruncateWithEllipsis(selected_relay->relay_url, context.compact ? 52 : 84),
      .relay_status = selected_relay == nullptr ? "idle" : monitostr::model::ToString(selected_relay->status),
      .relay_latency = (selected_relay != nullptr && selected_relay->latency_ms.has_value())
                           ? (std::to_string(static_cast<int>(*selected_relay->latency_ms)) + " ms")
                           : "waiting",
      .relay_nips = selected_relay == nullptr
                        ? "-"
                        : TruncateWithEllipsis(FormatNips(selected_relay->supported_nips), context.compact ? 24 : 40),
      .relay_error = selected_relay == nullptr || selected_relay->last_error.empty()
                         ? "none"
                         : TruncateWithEllipsis(selected_relay->last_error, context.compact ? 44 : 88),
  };
}

ftxui::Element RenderApp(const RenderContext& context) {
  using namespace ftxui;
  const auto& palette = UiPalette();

  const bool relays_focused = context.active_pane == ActivePane::kRelays;
  const bool logs_focused = context.active_pane == ActivePane::kLogs;
  const std::string mode_label = ModeLabel(context.mode);
  const std::string layout_label =
      context.compact_mode == CompactMode::kAuto
          ? (context.compact ? "compact(auto)" : "wide(auto)")
          : (context.compact_mode == CompactMode::kForceCompact ? "compact(forced)" : "wide(forced)");
  const std::string focus_label = relays_focused ? "Relays" : "Logs";
  const SelectedRelaySummary relay = SummarizeSelectedRelay(context);

  Element bottom_bar;
  const auto mode_color = ModeColor(context.mode);
  if (context.mode == RenderMode::kInsert) {
    bottom_bar = hbox({
                     text(" INSERT ") | bold | bgcolor(mode_color) | color(Color::Black),
                     text("  npub/nsec: "),
                     text(context.input_npub + "_"),
                 }) |
                 bgcolor(palette.panel) | color(palette.text);
  } else if (context.mode == RenderMode::kSearch) {
    bottom_bar =
        hbox({
            text(" SEARCH ") | bold | bgcolor(mode_color) | color(Color::Black),
            text("  /"),
            text(context.logs_search_query + "_"),
            filler(),
            text(std::to_string(context.visible_logs.size()) + "/" + std::to_string(context.total_logs) + " matches") |
                color(palette.text_muted),
        }) |
        bgcolor(palette.panel) | color(palette.text);
  } else if (context.mode == RenderMode::kCommand) {
    const std::string hint = ComputeCommandHint(context.command_line);
    const std::string resolved_hint = hint.empty() ? "No matching commands" : hint;
    bottom_bar = vbox({
        hbox({
            text(" CMD ") | bold | bgcolor(mode_color) | color(Color::Black),
            text("  :"),
            text(context.command_line + "_"),
        }) | bgcolor(palette.panel) |
            color(palette.text),
        hbox({
            text("  "),
            text(resolved_hint) | color(palette.text_muted),
        }) | bgcolor(palette.panel_alt),
    });
  } else {
    bottom_bar = hbox({
                     text(" " + mode_label + " ") | bold | bgcolor(mode_color) | color(Color::Black),
                     text("  [" + focus_label + "]") | color(palette.text),
                     text("  follow:") | color(palette.text_muted),
                     text(context.logs_follow ? "on" : "off") |
                         color(context.logs_follow ? palette.success : palette.text_muted),
                     text("  " + layout_label) | color(palette.text_muted),
                     filler(),
                     text("h/l  j/k  gg/G  Ctrl-d/u  /  :") | color(palette.text_muted),
                 }) |
                 bgcolor(palette.panel) | color(palette.text);
  }

  auto main_body =
      vbox({
          hbox({
              text(" MONITOSTR ") | bold | bgcolor(palette.accent) | color(Color::Black),
              text(" relay monitor") | color(palette.text_muted),
              filler(),
              RenderChip("connected " + std::to_string(context.aggregate.connected_relays), palette.success,
                         palette.panel_alt),
              text(" "),
              RenderChip("errors " + std::to_string(context.aggregate.errored_relays), palette.danger,
                         palette.panel_alt),
              text(" "),
              RenderChip("events " + std::to_string(context.aggregate.total_events), palette.accent, palette.panel_alt),
          }) | bgcolor(palette.panel),
          hbox({
              vbox({
                  text(" npub") | color(palette.text_muted),
                  text(" " + (context.npub_line.empty() ? std::string("waiting for input") : context.npub_line)) |
                      color(palette.text),
              }) | flex,
              text("  "),
              vbox({
                  text(" pubkey") | color(palette.text_muted),
                  text(" " + (context.hex_line.empty() ? std::string("not derived yet") : context.hex_line)) |
                      color(palette.text),
              }) | flex,
          }) | bgcolor(palette.panel_alt) |
              border,
          hbox({
              RenderMetricCard("Selected relay", relay.relay_summary, palette.text) | flex,
              text(" "),
              RenderMetricCard(
                  "Status", relay.relay_status,
                  StatusColor(context.relay_stats.empty() || context.selected_relay >= context.relay_stats.size()
                                  ? monitostr::model::RelayStatus::kDisconnected
                                  : context.relay_stats[context.selected_relay].status)),
              text(" "),
              RenderMetricCard("Latency", relay.relay_latency, palette.accent_warm),
              text(" "),
              RenderMetricCard("NIPs", relay.relay_nips, palette.accent),
          }),
          RenderInfoLine(" relay error: ", relay.relay_error,
                         relay.relay_error == "none" ? palette.text_muted : palette.danger),
          separator(),
          RenderRelayTable(context, relays_focused),
          separator(),
          RenderLogs(context, logs_focused),
      }) |
      border | color(palette.border) | bgcolor(palette.bg);

  return vbox({
      main_body | flex,
      bottom_bar,
  });
}

}  // namespace monitostr::ui
