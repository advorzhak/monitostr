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

#include <memory>
#include <string>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/ui/command_processor.hpp"

namespace monitostr::ui {

TEST_CASE("ComputeCommandHint suggests matching commands", "[ui][command]") {
  CHECK(ComputeCommandHint("set n").find(":set nofollow") != std::string::npos);
  CHECK(ComputeCommandHint("").find("copyrelay") != std::string::npos);
  CHECK(ComputeCommandHint("ri").find(":ri") != std::string::npos);
}

TEST_CASE("ExecuteCommand updates filter state", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;

  ExecuteCommand("filter paid", state, log_buffer, {});

  CHECK(state.logs_search_query == "paid");
  CHECK(state.logs_search_hit_ordinal == 0);
  CHECK_FALSE(state.logs_follow);
}

TEST_CASE("ExecuteCommand clears filters and restores follow on clearlogs", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;
  state.logs_follow = false;
  state.logs_scroll_lines = 8;
  state.logs_search_query = "relay";
  log_buffer->Info("one");

  ExecuteCommand("clearlogs", state, log_buffer, {});

  CHECK(state.logs_follow);
  CHECK(state.logs_scroll_lines == 0);
  CHECK(log_buffer->Snapshot().size() == 1);
  CHECK(log_buffer->Snapshot().front().message.find("Logs cleared by command") != std::string::npos);
}

TEST_CASE("ExecuteCommand toggles compact mode", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;

  ExecuteCommand("set compact", state, log_buffer, {});
  CHECK(state.compact_mode == CompactMode::kForceCompact);

  ExecuteCommand("set autocompact", state, log_buffer, {});
  CHECK(state.compact_mode == CompactMode::kAuto);
}

TEST_CASE("ExecuteCommand routes auth command to callback", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;
  std::string received;

  ExecuteCommand("auth nsec1example", state, log_buffer,
                 {.on_auth = [&](std::string value) { received = std::move(value); }});

  CHECK(received == "nsec1example");
}

TEST_CASE("ExecuteCommand copies the selected relay through callbacks", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;
  std::string copied_text;

  ExecuteCommand("copyrelay", state, log_buffer,
                 {
                     .copy_text =
                         [&](const std::string& text) {
                           copied_text = text;
                           return true;
                         },
                     .selected_relay_text = [&]() { return std::optional<std::string>{"wss://relay.example"}; },
                 });

  CHECK(copied_text == "wss://relay.example");
  const auto logs = log_buffer->Snapshot();
  REQUIRE_FALSE(logs.empty());
  CHECK(logs.back().message.find("Copied selected relay URL") != std::string::npos);
}

TEST_CASE("ExecuteCommand logs selected relay details through callbacks", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;

  ExecuteCommand("relayinfo", state, log_buffer,
                 {
                     .selected_relay_report =
                         [&]() {
                           return std::optional<std::string>{
                               "Selected relay: wss://relay.example | status=Subscribed | events=4 | nips=1,11"};
                         },
                 });

  const auto logs = log_buffer->Snapshot();
  REQUIRE_FALSE(logs.empty());
  CHECK(logs.back().message.find("Selected relay: wss://relay.example") != std::string::npos);
}

TEST_CASE("ExecuteCommand requests exit for quit aliases", "[ui][command]") {
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  CommandState state;
  bool exit_requested = false;

  ExecuteCommand("q", state, log_buffer, {.request_exit = [&]() { exit_requested = true; }});

  CHECK(exit_requested);
}

}  // namespace monitostr::ui
