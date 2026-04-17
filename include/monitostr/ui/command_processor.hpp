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

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "monitostr/model/log_buffer.hpp"

namespace monitostr::ui {

enum class CompactMode {
  kAuto,
  kForceCompact,
  kForceWide,
};

struct CommandState {
  bool logs_follow = true;
  bool nips_wrap_selected_row = true;
  CompactMode compact_mode = CompactMode::kAuto;
  std::string logs_search_query;
  std::size_t logs_search_hit_ordinal = 0;
  std::size_t logs_scroll_lines = 0;
};

struct CommandCallbacks {
  std::function<void()> request_exit;
  std::function<void(std::string)> on_auth;
  std::function<bool(const std::string&)> copy_text;
  std::function<std::optional<std::string>()> selected_relay_text;
  std::function<std::optional<std::string>()> selected_relay_report;
};

std::string ComputeCommandHint(const std::string& input);
void ExecuteCommand(const std::string& raw_command, CommandState& state,
                    const std::shared_ptr<monitostr::model::LogBuffer>& log_buffer, const CommandCallbacks& callbacks);

}  // namespace monitostr::ui
