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

#include <chrono>
#include <cstdint>
#include <future>
#include <nlohmann/json.hpp>
#include <thread>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <catch2/catch_test_macros.hpp>

#include "monitostr/net/bootstrap_client.hpp"

namespace monitostr::net {
namespace {

class LocalBootstrapRelayServer {
 public:
  explicit LocalBootstrapRelayServer(std::string event_payload)
      : event_payload_(std::move(event_payload)), server_thread_([this] { Run(); }) {
    port_ = port_ready_.get_future().get();
  }

  ~LocalBootstrapRelayServer() {
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  [[nodiscard]] std::uint16_t port() const { return port_; }
  [[nodiscard]] std::string received_request() { return request_payload_.get_future().get(); }

 private:
  void Run() {
    namespace websocket = boost::beast::websocket;
    bool port_reported = false;
    bool request_reported = false;

    try {
      boost::asio::io_context io_context;
      boost::asio::ip::tcp::acceptor acceptor(io_context, {boost::asio::ip::make_address("127.0.0.1"), 0});
      port_ready_.set_value(acceptor.local_endpoint().port());
      port_reported = true;

      boost::asio::ip::tcp::socket socket(io_context);
      acceptor.accept(socket);

      websocket::stream<boost::asio::ip::tcp::socket> ws(std::move(socket));
      ws.accept();

      boost::beast::flat_buffer buffer;
      ws.read(buffer);
      const std::string request = boost::beast::buffers_to_string(buffer.data());
      request_payload_.set_value(request);
      request_reported = true;

      ws.write(boost::asio::buffer(event_payload_));
      ws.write(boost::asio::buffer(R"(["EOSE","bootstrap-10002"])"));

      boost::beast::flat_buffer close_buffer;
      boost::system::error_code ignored;
      ws.read(close_buffer, ignored);
      ws.close(boost::beast::websocket::close_code::normal, ignored);
    } catch (...) {
      if (!port_reported) {
        port_ready_.set_exception(std::current_exception());
      }
      if (!request_reported) {
        request_payload_.set_exception(std::current_exception());
      }
    }
  }

  std::string event_payload_;
  std::thread server_thread_;
  std::promise<std::uint16_t> port_ready_;
  std::promise<std::string> request_payload_;
  std::uint16_t port_ = 0;
};

BootstrapResult AwaitBootstrapResult(boost::asio::io_context& io_context, std::future<BootstrapResult>& future) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
         std::chrono::steady_clock::now() < deadline) {
    io_context.restart();
    io_context.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  REQUIRE(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
  return future.get();
}

}  // namespace

TEST_CASE("BootstrapClient returns error for empty pubkey without network dependency", "[integration][bootstrap]") {
  boost::asio::io_context io_context;
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);

  BootstrapClient bootstrap(io_context);

  std::promise<BootstrapResult> completion;
  auto future = completion.get_future();

  bootstrap.ResolveRelaysForPubkey("", [&](BootstrapResult result) { completion.set_value(std::move(result)); });

  const auto result = AwaitBootstrapResult(io_context, future);
  REQUIRE_FALSE(result.ok);
  CHECK(result.error == "empty hex pubkey");
  CHECK(result.relay_urls.empty());
}

TEST_CASE("BootstrapClient resolves relay list from a local websocket seed relay", "[integration][bootstrap]") {
  const auto event = nlohmann::json::array(
      {"EVENT",
       "bootstrap-10002",
       {{"kind", 10002},
        {"pubkey", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
        {"tags", nlohmann::json::array({
                     nlohmann::json::array({"r", "wss://relay.one.example"}),
                     nlohmann::json::array({"r", "ws://relay.insecure.example"}),
                     nlohmann::json::array({"r", "wss://relay.one.example"}),
                 })},
        {"content", R"({"wss://relay.two.example":{"read":true},"ws://relay.three.example":{"write":true}})"}}});
  LocalBootstrapRelayServer server(event.dump());

  boost::asio::io_context io_context;
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();

  BootstrapClient bootstrap(io_context);
  bootstrap.SetLogBuffer(log_buffer);

  std::promise<BootstrapResult> completion;
  auto future = completion.get_future();

  const std::string seed_relay_url = "ws://127.0.0.1:" + std::to_string(server.port()) + "/nostr";
  const std::string hex_pubkey = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
  bootstrap.ResolveRelaysForPubkey(
      hex_pubkey, [&](BootstrapResult result) { completion.set_value(std::move(result)); }, seed_relay_url);

  const auto result = AwaitBootstrapResult(io_context, future);

  REQUIRE(result.ok);
  CHECK(result.source_event_kind == "10002");
  CHECK(result.error.empty());
  CHECK(result.relay_urls == std::vector<std::string>{"wss://relay.one.example", "wss://relay.two.example"});

  const auto request = nlohmann::json::parse(server.received_request());
  REQUIRE(request.is_array());
  REQUIRE(request.size() == 3);
  CHECK(request[0] == "REQ");
  CHECK(request[1] == "bootstrap-10002");
  REQUIRE(request[2].is_object());
  CHECK(request[2]["authors"] == nlohmann::json::array({hex_pubkey}));
  CHECK(request[2]["kinds"] == nlohmann::json::array({10002}));
  CHECK(request[2]["limit"] == 20);

  const auto logs = log_buffer->Snapshot();
  CHECK_FALSE(logs.empty());
  CHECK(logs.back().message.find("relays discovered: 2") != std::string::npos);
}

}  // namespace monitostr::net
