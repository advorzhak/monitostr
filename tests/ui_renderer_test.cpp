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

#include <chrono>

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
  CHECK(summary.relay_events == "0");
  CHECK(summary.relay_latency_stats == "-");
  CHECK(summary.relay_uptime == "-");
}

TEST_CASE("SummarizeSelectedRelay returns placeholders when nothing is selected", "[ui][renderer]") {
  const auto summary = SummarizeSelectedRelay({});

  CHECK(summary.relay_summary == "No relay selected");
  CHECK(summary.relay_status == "idle");
  CHECK(summary.relay_latency == "waiting");
  CHECK(summary.relay_nips == "-");
  CHECK(summary.relay_error == "none");
  CHECK(summary.relay_events == "0");
  CHECK(summary.relay_latency_stats == "-");
  CHECK(summary.relay_uptime == "-");
}

TEST_CASE("SummarizeSelectedRelay computes latency stats and uptime", "[ui][renderer]") {
  auto past_time = std::chrono::steady_clock::now() - std::chrono::seconds(125);
  RenderContext context{
      .compact = false,
      .selected_relay = 0,
      .relay_stats =
          {
              {
                  .relay_url = "wss://relay.example",
                  .status = monitostr::model::RelayStatus::kSubscribed,
                  .latency_ms = 42.0,
                  .events_count = 150,
                  .supported_nips = {1, 11},
                  .latency_history_ms = {30.0, 40.0, 50.0},
                  .connected_at = past_time,
              },
          },
  };

  const auto summary = SummarizeSelectedRelay(context);

  CHECK(summary.relay_events == "150");
  CHECK(summary.relay_latency_stats == "avg: 40 min: 30 max: 50");
  CHECK(summary.relay_uptime.find("2m") != std::string::npos);
}

TEST_CASE("ComputeRelayListViewport centers the selected relay when possible", "[ui][renderer]") {
  const auto viewport = ComputeRelayListViewport(20, 10, 6);

  CHECK(viewport.start == 7);
  CHECK(viewport.end == 13);
  CHECK(viewport.meta == "8-13 of 20");
}

TEST_CASE("ComputeRelayListViewport clamps near the end of the list", "[ui][renderer]") {
  const auto viewport = ComputeRelayListViewport(8, 7, 5);

  CHECK(viewport.start == 3);
  CHECK(viewport.end == 8);
  CHECK(viewport.meta == "4-8 of 8");
}

TEST_CASE("ComputeRelayListViewport reports zero state", "[ui][renderer]") {
  const auto viewport = ComputeRelayListViewport(0, 0, 5);

  CHECK(viewport.start == 0);
  CHECK(viewport.end == 0);
  CHECK(viewport.meta == "0 discovered");
}

TEST_CASE("ComputeRelayListViewport falls back to total when visible_rows is zero", "[ui][renderer]") {
  const auto viewport = ComputeRelayListViewport(4, 2, 0);

  CHECK(viewport.start == 0);
  CHECK(viewport.end == 4);
  CHECK(viewport.meta == "1-4 of 4");
}

TEST_CASE("ComputeRelayListViewport keeps the start at zero for early selections", "[ui][renderer]") {
  const auto viewport = ComputeRelayListViewport(10, 0, 5);

  CHECK(viewport.start == 0);
  CHECK(viewport.end == 5);
  CHECK(viewport.meta == "1-5 of 10");
}

TEST_CASE("ComputeRelayListViewport clamps an out-of-range selection", "[ui][renderer]") {
  const auto viewport = ComputeRelayListViewport(5, 100, 3);

  CHECK(viewport.start == 2);
  CHECK(viewport.end == 5);
  CHECK(viewport.meta == "3-5 of 5");
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
  CHECK(output.find("1-1 of 1") != std::string::npos);
  CHECK(output.find("relayinfo") != std::string::npos);
  CHECK(output.find("npub1example") != std::string::npos);
}

}  // namespace monitostr::ui
