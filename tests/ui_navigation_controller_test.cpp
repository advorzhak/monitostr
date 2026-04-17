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

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "monitostr/ui/navigation_controller.hpp"

namespace monitostr::ui {

TEST_CASE("FindLogMatches is case-insensitive", "[ui][navigation]") {
  const std::vector<monitostr::model::LogEntry> logs = {
      {.timestamp = "12:00:00", .message = "[INFO] Relay connected"},
      {.timestamp = "12:00:01", .message = "[WARN] Another line"},
  };

  const auto matches = FindLogMatches(logs, "relay");

  REQUIRE(matches.size() == 1);
  CHECK(matches.front() == 0);
}

TEST_CASE("AdvanceLogMatch updates log scroll position", "[ui][navigation]") {
  NavigationState state{
      .active_pane = ActivePane::kLogs,
      .logs_search_query = "relay",
  };
  const std::vector<monitostr::model::LogEntry> logs = {
      {.timestamp = "12:00:00", .message = "[INFO] start"},
      {.timestamp = "12:00:01", .message = "[INFO] relay one"},
      {.timestamp = "12:00:02", .message = "[INFO] relay two"},
      {.timestamp = "12:00:03", .message = "[INFO] relay three"},
  };

  AdvanceLogMatch(state, logs, true);

  CHECK(state.logs_search_hit_ordinal == 1);
  CHECK(state.logs_scroll_lines == 0);
  CHECK_FALSE(state.logs_follow);
}

TEST_CASE("MoveSelectionDown advances relay selection", "[ui][navigation]") {
  NavigationState state{.active_pane = ActivePane::kRelays};

  MoveSelectionDown(state, 3);

  CHECK(state.selected_relay == 1);
}

TEST_CASE("MovePageDown advances logs by page size", "[ui][navigation]") {
  NavigationState state{.active_pane = ActivePane::kLogs, .logs_scroll_lines = 2};

  MovePageDown(state, 0, 8);

  CHECK(state.logs_scroll_lines == 10);
  CHECK_FALSE(state.logs_follow);
}

TEST_CASE("JumpToPaneEnd moves to bottom of current pane", "[ui][navigation]") {
  NavigationState state{.active_pane = ActivePane::kLogs};

  JumpToPaneEnd(state, 0, 15, 8);

  CHECK(state.logs_scroll_lines == 7);
  CHECK_FALSE(state.logs_follow);
}

}  // namespace monitostr::ui
