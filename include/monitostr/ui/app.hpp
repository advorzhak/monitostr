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

#include <ftxui/component/component.hpp>

#include <functional>
#include <memory>
#include <string>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/model/relay_stats.hpp"
// Fix #12: include ui_renderer.hpp so RenderMode is available here without
// duplicating the enum definition.
#include "monitostr/ui/ui_renderer.hpp"

namespace monitostr::ui {

class App {
 public:
  struct HeaderContext {
    std::string npub;
    std::string hex_pubkey;
  };

  using NpubSubmit = std::function<void(std::string npub)>;
  using NsecSubmit = std::function<void(std::string nsec)>;

  App(std::shared_ptr<monitostr::model::RelayStats> shared_stats,
      std::shared_ptr<monitostr::model::LogBuffer> log_buffer, NpubSubmit on_submit, NsecSubmit on_auth = nullptr);

  void SetHeaderContext(HeaderContext context);
  void Run();

 private:
  ftxui::Component BuildComponentTree();

  HeaderContext header_context_;
  std::shared_ptr<monitostr::model::RelayStats> shared_stats_;
  std::shared_ptr<monitostr::model::LogBuffer> log_buffer_;
  NpubSubmit on_submit_;
  NsecSubmit on_auth_;
  std::string input_npub_;
  // Fix #12: use RenderMode directly instead of a parallel App::UiMode enum.
  RenderMode mode_ = RenderMode::kNormal;
  ActivePane active_pane_ = ActivePane::kRelays;
  std::size_t selected_relay_ = 0;
  std::size_t logs_scroll_lines_ = 0;
  bool logs_follow_ = true;
  bool pending_g_ = false;
  std::string logs_search_query_;
  std::size_t logs_search_hit_ordinal_ = 0;
  std::string command_line_;
  std::function<void()> request_exit_;
  CompactMode compact_mode_ = CompactMode::kAuto;
};

}  // namespace monitostr::ui
