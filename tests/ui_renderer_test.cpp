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

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include "monitostr/model/relay_stats.hpp"
#include "monitostr/ui/ui_renderer.hpp"

namespace monitostr::ui {

TEST_CASE("SummarizeLogStream reports total lines without active search", "[ui][renderer]") {
  RenderContext context{
      .total_logs = 12,
  };

  const auto summary = SummarizeLogStream(context);

  CHECK(summary.meta == "12 lines");
  CHECK(summary.empty_text == "No logs yet.");
}

TEST_CASE("SummarizeLogStream reports visible matches during search", "[ui][renderer]") {
  RenderContext context{
      .logs_search_query = "paid",
      .total_logs = 12,
      .visible_logs =
          {
              {.timestamp = "12:00:00", .message = "[INFO] one"},
              {.timestamp = "12:00:01", .message = "[INFO] two"},
          },
  };

  const auto summary = SummarizeLogStream(context);

  CHECK(summary.meta == "2/12 visible");
  CHECK(summary.empty_text == "No logs match the current search.");
}

TEST_CASE("SummarizeSelectedRelay surfaces relay details and errors", "[ui][renderer]") {
  RenderContext context{
      .compact = false,
      .selected_relay = 0,
      .relay_stats =
          {
              {
                  .relay_url = "wss://relay.example",
                  .status = monitostr::model::RelayStatus::kError,
                  .latency_ms = 321.0,
                  .supported_nips = {1, 11, 65},
                  .last_error = "tls_handshake: certificate verify failed",
              },
          },
  };

  const auto summary = SummarizeSelectedRelay(context);

  CHECK(summary.relay_summary == "wss://relay.example");
  CHECK(summary.relay_status == "Error");
  CHECK(summary.relay_latency == "321 ms");
  CHECK(summary.relay_nips == "1,11,65");
  CHECK(summary.relay_error.find("certificate verify failed") != std::string::npos);
}

TEST_CASE("SummarizeSelectedRelay returns placeholders when nothing is selected", "[ui][renderer]") {
  const auto summary = SummarizeSelectedRelay({});

  CHECK(summary.relay_summary == "No relay selected");
  CHECK(summary.relay_status == "idle");
  CHECK(summary.relay_latency == "waiting");
  CHECK(summary.relay_nips == "-");
  CHECK(summary.relay_error == "none");
}

TEST_CASE("RenderApp includes key panels and mode labels in the rendered frame", "[ui][renderer]") {
  RenderContext context{
      .compact = true,
      .relay_rows = 4,
      .log_lines = 4,
      .mode = RenderMode::kCommand,
      .npub_line = "npub1example",
      .hex_line = "abcdef",
      .logs_search_query = "warn",
      .command_line = "relayinfo",
      .total_logs = 2,
      .relay_stats = {{
          .relay_url = "wss://relay.example",
          .status = monitostr::model::RelayStatus::kSubscribed,
          .events_count = 4,
          .supported_nips = {1, 11},
      }},
      .visible_logs =
          {
              {.timestamp = "12:00:00", .message = "[INFO] boot"},
              {.timestamp = "12:00:01", .message = "[WARN] auth"},
          },
  };

  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(32));
  ftxui::Render(screen, RenderApp(context));
  const auto output = screen.ToString();

  CHECK(output.find("RELAYS") != std::string::npos);
  CHECK(output.find("LOG STREAM") != std::string::npos);
  CHECK(output.find("relayinfo") != std::string::npos);
  CHECK(output.find("npub1example") != std::string::npos);
}

}  // namespace monitostr::ui
