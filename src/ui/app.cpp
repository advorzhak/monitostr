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
#include <optional>
#include <thread>
#include <vector>

#include "monitostr/security/secure_memory.hpp"
#include "monitostr/ui/ui_renderer.hpp"
#include "monitostr/utils/string_utils.hpp"

namespace monitostr::ui {
namespace {

using monitostr::ToLowerCopy;
using monitostr::TrimCopy;

bool LooksLikeNsecInput(std::string_view value) {
  static constexpr std::string_view kPrefix = "nsec1";
  if (value.size() < kPrefix.size()) return false;
  for (std::size_t i = 0; i < kPrefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(value[i])) != kPrefix[i]) return false;
  }
  return true;
}

std::string FormatRelayReport(const monitostr::model::RelayStat& stat) {
  std::string report = "Selected relay: " + stat.relay_url + " | status=" + monitostr::model::ToString(stat.status);
  if (stat.latency_ms.has_value()) {
    report += " | latency=" + std::to_string(static_cast<int>(*stat.latency_ms)) + " ms";
  }
  report += " | events=" + std::to_string(stat.events_count);
  report += " | nips=" + (stat.supported_nips.empty() ? std::string("-") : FormatNips(stat.supported_nips));
  if (!stat.last_error.empty()) {
    report += " | error=" + stat.last_error;
  }
  return report;
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
    const std::size_t relay_rows = static_cast<std::size_t>(std::max(8, std::min(16, term.dimy - (compact ? 16 : 18))));
    const std::size_t log_lines = static_cast<std::size_t>(std::max(6, std::min(12, term.dimy / 3)));

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
    visible_logs.reserve(logs.size());

    if (logs_search_query_.empty()) {
      visible_logs = logs;
    } else {
      for (const auto& entry : logs) {
        const std::string line = entry.timestamp + " " + entry.message;
        if (std::search(
                line.begin(), line.end(), logs_search_query_.begin(), logs_search_query_.end(), [](char a, char b) {
                  return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                }) != line.end()) {
          visible_logs.push_back(entry);
        }
      }
    }

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

    return RenderApp({
        .compact = compact,
        .tiny = tiny,
        .relay_rows = relay_rows,
        .log_lines = log_lines,
        .mode =
            mode_ == UiMode::kInsert
                ? RenderMode::kInsert
                : (mode_ == UiMode::kSearch ? RenderMode::kSearch
                                            : (mode_ == UiMode::kCommand ? RenderMode::kCommand : RenderMode::kNormal)),
        .active_pane = active_pane_,
        .logs_follow = logs_follow_,
        .compact_mode = compact_mode_,
        .npub_line = npub_line,
        .hex_line = hex_line,
        .input_npub = input_npub_,
        .logs_search_query = logs_search_query_,
        .command_line = command_line_,
        .selected_relay = selected_relay_,
        .logs_scroll_lines = logs_scroll_lines_,
        .total_logs = logs.size(),
        .aggregate = aggregate,
        .relay_stats = snapshot,
        .visible_logs = visible_logs,
    });
  });

  auto interactive = CatchEvent(renderer, [this](Event event) {
    constexpr std::size_t kLogPageLines = 8;

    if (mode_ == UiMode::kCommand) {
      if (event == Event::Escape) {
        monitostr::security::SecureClearString(command_line_);
        mode_ = UiMode::kNormal;
        pending_g_ = false;
        return true;
      }
      if (event == Event::Return) {
        CommandState command_state{
            .logs_follow = logs_follow_,
            .compact_mode = compact_mode_,
            .logs_search_query = logs_search_query_,
            .logs_search_hit_ordinal = logs_search_hit_ordinal_,
            .logs_scroll_lines = logs_scroll_lines_,
        };
        ExecuteCommand(command_line_, command_state, log_buffer_,
                       {
                           .request_exit = request_exit_,
                           .on_auth = on_auth_,
                           .selected_relay_text = [this]() -> std::optional<std::string> {
                             const auto snapshot = shared_stats_->Snapshot();
                             if (snapshot.empty() || selected_relay_ >= snapshot.size()) {
                               return std::nullopt;
                             }
                             return snapshot[selected_relay_].relay_url;
                           },
                           .selected_relay_report = [this]() -> std::optional<std::string> {
                             const auto snapshot = shared_stats_->Snapshot();
                             if (snapshot.empty() || selected_relay_ >= snapshot.size()) {
                               return std::nullopt;
                             }
                             return FormatRelayReport(snapshot[selected_relay_]);
                           },
                       });
        logs_follow_ = command_state.logs_follow;
        compact_mode_ = command_state.compact_mode;
        logs_search_query_ = std::move(command_state.logs_search_query);
        logs_search_hit_ordinal_ = command_state.logs_search_hit_ordinal;
        logs_scroll_lines_ = command_state.logs_scroll_lines;
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

    auto navigation_state = NavigationState{
        .active_pane = active_pane_,
        .selected_relay = selected_relay_,
        .logs_scroll_lines = logs_scroll_lines_,
        .logs_follow = logs_follow_,
        .pending_g = pending_g_,
        .logs_search_query = logs_search_query_,
        .logs_search_hit_ordinal = logs_search_hit_ordinal_,
    };
    auto apply_navigation_state = [&]() {
      active_pane_ = navigation_state.active_pane;
      selected_relay_ = navigation_state.selected_relay;
      logs_scroll_lines_ = navigation_state.logs_scroll_lines;
      logs_follow_ = navigation_state.logs_follow;
      pending_g_ = navigation_state.pending_g;
      logs_search_query_ = navigation_state.logs_search_query;
      logs_search_hit_ordinal_ = navigation_state.logs_search_hit_ordinal;
    };

    if (mode_ == UiMode::kSearch) {
      if (event == Event::Escape) {
        mode_ = UiMode::kNormal;
        navigation_state.pending_g = false;
        apply_navigation_state();
        return true;
      }
      if (event == Event::Return) {
        mode_ = UiMode::kNormal;
        navigation_state.logs_search_hit_ordinal = 0;
        navigation_state.pending_g = false;
        apply_navigation_state();
        return true;
      }
      if (event == Event::Backspace) {
        if (!navigation_state.logs_search_query.empty()) {
          navigation_state.logs_search_query.pop_back();
        }
        apply_navigation_state();
        return true;
      }
      if (event.is_character()) {
        navigation_state.logs_search_query += event.character();
        apply_navigation_state();
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
      navigation_state.pending_g = false;
      apply_navigation_state();
      return true;
    }
    if (event == Event::Character(':')) {
      mode_ = UiMode::kCommand;
      monitostr::security::SecureClearString(command_line_);
      navigation_state.pending_g = false;
      apply_navigation_state();
      return true;
    }
    if (event == Event::Character('/')) {
      navigation_state.active_pane = ActivePane::kLogs;
      mode_ = UiMode::kSearch;
      navigation_state.logs_follow = false;
      navigation_state.pending_g = false;
      apply_navigation_state();
      return true;
    }
    if (event == Event::Character('h')) {
      navigation_state.active_pane = ActivePane::kRelays;
      navigation_state.pending_g = false;
      apply_navigation_state();
      return true;
    }
    if (event == Event::Character('l')) {
      navigation_state.active_pane = ActivePane::kLogs;
      navigation_state.pending_g = false;
      apply_navigation_state();
      return true;
    }
    if (event == Event::Character('f')) {
      navigation_state.logs_follow = !navigation_state.logs_follow;
      navigation_state.pending_g = false;
      apply_navigation_state();
      return true;
    }

    if (event == Event::Character('g')) {
      if (navigation_state.pending_g) {
        JumpToTop(navigation_state);
      } else {
        navigation_state.pending_g = true;
      }
      apply_navigation_state();
      return true;
    }
    navigation_state.pending_g = false;

    if (event == Event::Character('G')) {
      const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
      const auto snapshot = shared_stats_->Snapshot();
      JumpToBottom(navigation_state, snapshot.size(), logs.size(), kLogPageLines);
      apply_navigation_state();
      return true;
    }

    if (event == Event::Character('0')) {
      JumpToPaneStart(navigation_state);
      apply_navigation_state();
      return true;
    }

    if (event == Event::Character('$')) {
      const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
      const auto snapshot = shared_stats_->Snapshot();
      JumpToPaneEnd(navigation_state, snapshot.size(), logs.size(), kLogPageLines);
      apply_navigation_state();
      return true;
    }

    if (event == Event::Character('n') || event == Event::Character('N')) {
      const auto logs = log_buffer_ ? log_buffer_->Snapshot() : std::vector<monitostr::model::LogEntry>{};
      AdvanceLogMatch(navigation_state, logs, event == Event::Character('n'));
      apply_navigation_state();
      return true;
    }

    if (event == Event::Character('j')) {
      const auto snapshot = shared_stats_->Snapshot();
      MoveSelectionDown(navigation_state, snapshot.size());
      apply_navigation_state();
      return true;
    }

    if (event == Event::Character('k')) {
      MoveSelectionUp(navigation_state);
      apply_navigation_state();
      return true;
    }

    if (event == Event::CtrlD || event == Event::PageDown || event == Event::Character('d')) {
      const auto snapshot = shared_stats_->Snapshot();
      MovePageDown(navigation_state, snapshot.size(), kLogPageLines);
      apply_navigation_state();
      return true;
    }

    if (event == Event::CtrlU || event == Event::PageUp || event == Event::Character('u')) {
      MovePageUp(navigation_state, kLogPageLines);
      apply_navigation_state();
      return true;
    }

    if (event == Event::Escape) {
      mode_ = UiMode::kNormal;
      apply_navigation_state();
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
