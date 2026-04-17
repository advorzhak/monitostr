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

#include <cstddef>
#include <string>
#include <vector>

#include "monitostr/model/log_buffer.hpp"

namespace monitostr::ui {

enum class ActivePane {
  kRelays,
  kLogs,
};

struct NavigationState {
  ActivePane active_pane = ActivePane::kRelays;
  std::size_t selected_relay = 0;
  std::size_t logs_scroll_lines = 0;
  bool logs_follow = true;
  bool pending_g = false;
  std::string logs_search_query;
  std::size_t logs_search_hit_ordinal = 0;
};

std::vector<std::size_t> FindLogMatches(const std::vector<monitostr::model::LogEntry>& logs, const std::string& query);
void JumpToTop(NavigationState& state);
void JumpToBottom(NavigationState& state, std::size_t relay_count, std::size_t log_count, std::size_t log_page_lines);
void JumpToPaneStart(NavigationState& state);
void JumpToPaneEnd(NavigationState& state, std::size_t relay_count, std::size_t log_count, std::size_t log_page_lines);
void MoveSelectionDown(NavigationState& state, std::size_t relay_count);
void MoveSelectionUp(NavigationState& state);
void MovePageDown(NavigationState& state, std::size_t relay_count, std::size_t log_page_lines);
void MovePageUp(NavigationState& state, std::size_t log_page_lines);
void AdvanceLogMatch(NavigationState& state, const std::vector<monitostr::model::LogEntry>& logs, bool forward);

}  // namespace monitostr::ui
