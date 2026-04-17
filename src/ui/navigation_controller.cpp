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

#include "monitostr/ui/navigation_controller.hpp"

#include <algorithm>
#include <cctype>

namespace monitostr::ui {
namespace {

std::string ToLowerCopy(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

}  // namespace

std::vector<std::size_t> FindLogMatches(const std::vector<monitostr::model::LogEntry>& logs, const std::string& query) {
  std::vector<std::size_t> matches;
  if (query.empty()) {
    return matches;
  }

  const std::string lowered_query = ToLowerCopy(query);
  for (std::size_t i = 0; i < logs.size(); ++i) {
    const std::string line = ToLowerCopy(logs[i].timestamp + " " + logs[i].message);
    if (line.find(lowered_query) != std::string::npos) {
      matches.push_back(i);
    }
  }
  return matches;
}

void JumpToTop(NavigationState& state) {
  state.selected_relay = 0;
  state.logs_scroll_lines = 0;
  state.pending_g = false;
}

void JumpToBottom(NavigationState& state, std::size_t relay_count, std::size_t log_count, std::size_t log_page_lines) {
  if (state.active_pane == ActivePane::kRelays) {
    if (relay_count > 0) {
      state.selected_relay = relay_count - 1;
    }
    return;
  }

  if (log_count > log_page_lines) {
    state.logs_scroll_lines = log_count - log_page_lines;
  }
}

void JumpToPaneStart(NavigationState& state) {
  if (state.active_pane == ActivePane::kRelays) {
    state.selected_relay = 0;
  } else {
    state.logs_scroll_lines = 0;
    state.logs_follow = false;
  }
}

void JumpToPaneEnd(NavigationState& state, std::size_t relay_count, std::size_t log_count, std::size_t log_page_lines) {
  if (state.active_pane == ActivePane::kRelays) {
    if (relay_count > 0) {
      state.selected_relay = relay_count - 1;
    }
  } else {
    state.logs_scroll_lines = log_count > log_page_lines ? log_count - log_page_lines : 0;
    state.logs_follow = false;
  }
}

void MoveSelectionDown(NavigationState& state, std::size_t relay_count) {
  if (state.active_pane == ActivePane::kRelays) {
    if (relay_count > 0 && state.selected_relay + 1 < relay_count) {
      ++state.selected_relay;
    }
  } else {
    ++state.logs_scroll_lines;
    state.logs_follow = false;
  }
}

void MoveSelectionUp(NavigationState& state) {
  if (state.active_pane == ActivePane::kRelays) {
    if (state.selected_relay > 0) {
      --state.selected_relay;
    }
  } else {
    if (state.logs_scroll_lines > 0) {
      --state.logs_scroll_lines;
    }
    state.logs_follow = false;
  }
}

void MovePageDown(NavigationState& state, std::size_t relay_count, std::size_t log_page_lines) {
  if (state.active_pane == ActivePane::kLogs) {
    state.logs_scroll_lines += log_page_lines;
    state.logs_follow = false;
    return;
  }

  if (relay_count > 0) {
    state.selected_relay = std::min(state.selected_relay + 5, relay_count - 1);
  }
}

void MovePageUp(NavigationState& state, std::size_t log_page_lines) {
  if (state.active_pane == ActivePane::kLogs) {
    state.logs_scroll_lines = state.logs_scroll_lines > log_page_lines ? state.logs_scroll_lines - log_page_lines : 0;
    state.logs_follow = false;
    return;
  }

  state.selected_relay = state.selected_relay > 5 ? state.selected_relay - 5 : 0;
}

void AdvanceLogMatch(NavigationState& state, const std::vector<monitostr::model::LogEntry>& logs, bool forward) {
  if (state.active_pane != ActivePane::kLogs || state.logs_search_query.empty()) {
    return;
  }

  const auto matches = FindLogMatches(logs, state.logs_search_query);
  if (matches.empty()) {
    return;
  }

  if (forward) {
    state.logs_search_hit_ordinal = (state.logs_search_hit_ordinal + 1) % matches.size();
  } else {
    state.logs_search_hit_ordinal = (state.logs_search_hit_ordinal + matches.size() - 1) % matches.size();
  }

  const std::size_t pos = matches[state.logs_search_hit_ordinal];
  state.logs_scroll_lines = pos > 2 ? pos - 2 : 0;
  state.logs_follow = false;
}

}  // namespace monitostr::ui
