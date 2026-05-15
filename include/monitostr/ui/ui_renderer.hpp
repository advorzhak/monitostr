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

#include <ftxui/dom/elements.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/model/relay_stats.hpp"
#include "monitostr/ui/command_processor.hpp"
#include "monitostr/ui/navigation_controller.hpp"

namespace monitostr::ui {

enum class RenderMode {
  kNormal,
  kInsert,
  kSearch,
  kCommand,
};

struct RenderContext {
  bool compact = false;
  bool tiny = false;
  std::size_t relay_rows = 0;
  std::size_t log_lines = 0;
  RenderMode mode = RenderMode::kNormal;
  ActivePane active_pane = ActivePane::kRelays;
  bool logs_follow = true;
  CompactMode compact_mode = CompactMode::kAuto;
  std::string npub_line;
  std::string hex_line;
  std::string input_npub;
  std::string logs_search_query;
  std::string command_line;
  std::size_t selected_relay = 0;
  std::size_t logs_scroll_lines = 0;
  std::size_t total_logs = 0;
  monitostr::model::AggregateStats aggregate;
  std::vector<monitostr::model::RelayStat> relay_stats;
  std::vector<monitostr::model::LogEntry> visible_logs;
};

struct LogStreamSummary {
  std::string meta;
  std::string empty_text;
};

struct SelectedRelaySummary {
  std::string relay_summary;
  std::string relay_status;
  std::string relay_latency;
  std::string relay_events;
  std::string relay_nips;
  std::string relay_error;
  std::string relay_latency_stats;
  std::string relay_uptime;
};

struct RelayListViewport {
  std::size_t start = 0;
  std::size_t end = 0;
  std::string meta;
};

std::string TruncateWithEllipsis(const std::string& value, std::size_t max_len);
std::string FormatNips(const std::vector<int>& nips);
RelayListViewport ComputeRelayListViewport(std::size_t total_relays, std::size_t selected_relay,
                                           std::size_t visible_rows);
LogStreamSummary SummarizeLogStream(const RenderContext& context);
SelectedRelaySummary SummarizeSelectedRelay(const RenderContext& context);
ftxui::Element RenderApp(const RenderContext& context);

}  // namespace monitostr::ui
