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

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/model/relay_stats.hpp"
#include "monitostr/net/relay_session.hpp"
#include "monitostr/nostr/auth_key.hpp"

namespace monitostr::net {
namespace {

constexpr std::string_view kLocalhostCertPem = R"(-----BEGIN CERTIFICATE-----
MIIDHzCCAgegAwIBAgIUKDwyND8XsBeAnqsuxj9mrubgsZgwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI2MDQxNzEyMjMwMloXDTI3MDQx
NzEyMjMwMlowFDESMBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAiVGRlwQvsC9UH4XdakhurhSHYA+B5/UAsoADfopAgOdo
VqgmWKVQdYY3MqmnT1ucpQWmqDiIriqnBzHzxG7lVf1Gfrrv+qU0Y02xaq8lmRky
dqkiKcBMTkmk+ZcK56O9+TQeEwSSRGLFXDyCCDlHfTCM86lOd3ZAWiw29mF3abal
qVKhws+oLeut8Jwz0vmrl0w/vYqy0yXRMJS7Zg7QNjg8y9B5aTKH+EEoTW+UIdTb
cx8NGSDX7kSdn9+DttjrEJ4sjvb2p1TgpMYhp32oVzgDH1gJh3YrbCsmcuDZYbHr
ilZmgy6EXGIkFZ//GwGqpAck0dUn9LSHGi6soUO8zwIDAQABo2kwZzAdBgNVHQ4E
FgQUFJMyildOcREo8NqbElfHb39ObsswHwYDVR0jBBgwFoAUFJMyildOcREo8Nqb
ElfHb39ObsswDwYDVR0TAQH/BAUwAwEB/zAUBgNVHREEDTALgglsb2NhbGhvc3Qw
DQYJKoZIhvcNAQELBQADggEBABI6iKa0Jnpkrr30ppf6/wFFSJHrChDidNAYVX0w
wKlgG+tHqVNfxvjY5MkvHDaGEAiJFHUo3M4beTJtTh2ibx40ehJ4SeQZJwbNMgzK
6XG1vF6QUcYxqDHarG97LhUGR47skoUXkG1M/rP7n306ir1eDTwtJlbSGO8o8+oF
gT4s0u5AwAUJ4jm2EB96jwqW5O5otu+yReCuFCY65j6nzxahSsMAT1fgjrJbDE1P
ZlmwuaVObALGWYAvYsYRPMMkhDkoORwYS0wow5g8d4aaEQlu/rL6Cw1PDWzlvGAe
oHP2liJENDoHxvVIWkJSUWOVijsBbYeglFj0p1isQfeS11g=
-----END CERTIFICATE-----
)";

constexpr std::string_view kLocalhostKeyPem = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCJUZGXBC+wL1Qf
hd1qSG6uFIdgD4Hn9QCygAN+ikCA52hWqCZYpVB1hjcyqadPW5ylBaaoOIiuKqcH
MfPEbuVV/UZ+uu/6pTRjTbFqryWZGTJ2qSIpwExOSaT5lwrno735NB4TBJJEYsVc
PIIIOUd9MIzzqU53dkBaLDb2YXdptqWpUqHCz6gt663wnDPS+auXTD+9irLTJdEw
lLtmDtA2ODzL0HlpMof4QShNb5Qh1NtzHw0ZINfuRJ2f34O22OsQniyO9vanVOCk
xiGnfahXOAMfWAmHditsKyZy4NlhseuKVmaDLoRcYiQVn/8bAaqkByTR1Sf0tIca
LqyhQ7zPAgMBAAECggEAOpLbqnIzsVCO7p9YUgSxiXniQPx8aigc8wcd9nUcjJ+o
5wX0zW0+w9LLasXpQzWNvOqLK0hqLPiupKgPFiRDmQlfb6Fnyh98GDvEyQAK5qJT
3z/H/c0z/1T0lS5zjVlQKKOtjGkvxxHmo7B/uuhL8T+rcCgw+04qpwwjKbdri2Co
kZFSEpSuJB7NTO0HXA1CesJ9PXhOkdyBmxbfTqvZjrJMjYo9gnQl0GEKk72A8CW5
qDWMiCritQCz0k6KUZ9gJQGVUHXPUoHw54qoCBqineJo6mszXezRiE0YTH/VGJtG
05DE60wBfATwSBCsu5TcAPKOCss/UCjiWhMI5vLdsQKBgQC+p5VeduYZjCCWOsZ6
Zqb3rm3zl8n7LljA6pcTMQzD5gvN3VOFOEacKQsPMKX2+UQISbSF3Ndisw03Y0vu
mwhB+U1FoF4RyuFWq4qZLCdXzHHvaou7yJXwRVKmjtxMM1ujIjYpU/zk7pF0qcyK
Z4DJ50X3qM3lSvIlus04tA2oGQKBgQC4Yi3CAGtnqxkTHZ1ZAJZyFbaZcBIXWRg7
26YSBJJ4WWfdG5v/YmvgoeH1bWj8cjt6YTPCWakefeXquBvecOahtOI9L9NUmVVC
n1rfz4RCvvsXjdKGDewhuFXjhQjCLlfA1I4pfrWyABN6U1bLr8ewI8y3DUTj0Hwg
v3oKiEBJJwKBgQCPfTkh+4J6P8hWyK2Qeam4R4NhDiBp9xu9NdG2E/hzh7PioGy3
mv8pkIpOSGLgWgIl6rL+/JLuKawMv6LqVawFW0gY2vVxlqu5uyl41o5Vuf0WH65D
ClgumT33NRYAE7coNBtnR1rSQesfTpwunm+DhZhXKKitpZZRZyIFRx3wIQKBgEF8
9X2bQoqelZhZg2JyN/0rQyC6UmKijV3tRM3Pe+ps7tv0i3KolWJCfQv7oTdQp1lv
HsAfslaXJss4OwXFmdTDdzt+OhQpJcQ070Tg4rwGMw6Jm8VrOnw7iZ32yUaWySo/
FIMecxBWwJRI92H++/DOtk4p01cK3JuRBpqpDBQvAoGAaYHXW4C+0I/PSbUhX172
bmTZFJysa4rt42YocfwWEVKE6fg48QNX0gC5yplkXLiV2JKoi/yEWmYF/dPiU3J4
7Ns8OfzuFczt9/kY4MUHJWjie6w4jt0lnNNfSrXnJ6916TTTeAzjhbTDaY6leKjG
oQe458QIqfgotIRQXeidhFM=
-----END PRIVATE KEY-----
)";

void ConfigureClientTlsContext(boost::asio::ssl::context& ssl_context) {
  ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);
  ssl_context.add_certificate_authority(boost::asio::buffer(kLocalhostCertPem.data(), kLocalhostCertPem.size()));
}

boost::asio::ssl::context MakeServerTlsContext() {
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_server);
  ssl_context.use_certificate_chain(boost::asio::buffer(kLocalhostCertPem.data(), kLocalhostCertPem.size()));
  ssl_context.use_private_key(boost::asio::buffer(kLocalhostKeyPem.data(), kLocalhostKeyPem.size()),
                              boost::asio::ssl::context::file_format::pem);
  return ssl_context;
}

struct RelayServerSnapshot {
  bool saw_ws_upgrade = false;
  bool saw_nip11_request = false;
  std::string ws_target;
  std::string nip11_target;
  std::string monitor_request;
  std::optional<std::string> auth_message;
};

class LocalTlsRelayServer {
 public:
  explicit LocalTlsRelayServer(bool issue_auth_challenge)
      : issue_auth_challenge_(issue_auth_challenge), server_thread_([this] { Run(); }) {
    WaitForPort();
  }

  ~LocalTlsRelayServer() {
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  [[nodiscard]] std::uint16_t port() const { return port_; }

  [[nodiscard]] RelayServerSnapshot Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
  }

  void RethrowIfFailed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (failure_) {
      std::rethrow_exception(failure_);
    }
  }

 private:
  void WaitForPort() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (port_ == 0 && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(port_ != 0);
  }

  void RecordFailure(std::exception_ptr failure) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!failure_) {
      failure_ = std::move(failure);
    }
  }

  void Run() {
    namespace http = boost::beast::http;
    namespace websocket = boost::beast::websocket;

    try {
      boost::asio::io_context io_context;
      auto ssl_context = MakeServerTlsContext();
      boost::asio::ip::tcp::acceptor acceptor(io_context, {boost::asio::ip::make_address("127.0.0.1"), 0});
      port_ = acceptor.local_endpoint().port();

      boost::asio::ip::tcp::socket socket(io_context);
      acceptor.accept(socket);

      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> tls_stream(std::move(socket), ssl_context);
      tls_stream.handshake(boost::asio::ssl::stream_base::server);

      boost::beast::flat_buffer buffer;
      http::request<http::string_body> request;
      http::read(tls_stream, buffer, request);
      if (!websocket::is_upgrade(request)) {
        throw std::runtime_error("expected websocket upgrade request");
      }

      {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.saw_ws_upgrade = true;
        snapshot_.ws_target = request.target();
      }

      websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ws(std::move(tls_stream));
      ws.accept(request);

      boost::beast::flat_buffer ws_buffer;
      ws.read(ws_buffer);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.monitor_request = boost::beast::buffers_to_string(ws_buffer.data());
      }

      if (issue_auth_challenge_) {
        ws.write(boost::asio::buffer(R"(["AUTH","relay-challenge"])"));
        ws_buffer.consume(ws_buffer.size());
        ws.read(ws_buffer);
        {
          std::lock_guard<std::mutex> lock(mutex_);
          snapshot_.auth_message = boost::beast::buffers_to_string(ws_buffer.data());
        }
      }

      ws.write(boost::asio::buffer(R"(["EVENT","monitostr-sub",{"kind":1,"content":"hello"}])"));
      ws.write(boost::asio::buffer(R"(["EOSE","monitostr-sub"])"));

      boost::asio::ip::tcp::socket nip11_socket(io_context);
      acceptor.accept(nip11_socket);

      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> nip11_stream(std::move(nip11_socket), ssl_context);
      nip11_stream.handshake(boost::asio::ssl::stream_base::server);

      boost::beast::flat_buffer nip11_buffer;
      http::request<http::string_body> nip11_request;
      http::read(nip11_stream, nip11_buffer, nip11_request);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.saw_nip11_request = true;
        snapshot_.nip11_target = nip11_request.target();
      }

      http::response<http::string_body> response{http::status::ok, nip11_request.version()};
      response.set(http::field::server, "monitostr-test");
      response.set(http::field::content_type, "application/nostr+json");
      response.body() = R"({"supported_nips":[11,42,11,1]})";
      response.prepare_payload();
      http::write(nip11_stream, response);

      boost::system::error_code ignored;
      nip11_stream.shutdown(ignored);
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      ws.close(boost::beast::websocket::close_code::normal, ignored);
    } catch (...) {
      RecordFailure(std::current_exception());
    }
  }

  bool issue_auth_challenge_;
  std::thread server_thread_;
  mutable std::mutex mutex_;
  RelayServerSnapshot snapshot_;
  std::exception_ptr failure_;
  std::uint16_t port_ = 0;
};

template <typename Predicate>
bool PumpUntil(boost::asio::io_context& io_context, Predicate&& predicate, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    io_context.restart();
    io_context.poll();
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  io_context.restart();
  io_context.poll();
  return predicate();
}

}  // namespace

TEST_CASE("RelaySession marks invalid relay URLs as errors without network activity", "[integration][relay_session]") {
  boost::asio::io_context io_context;
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);
  boost::asio::thread_pool background_pool(1);
  auto stats = std::make_shared<monitostr::model::RelayStats>();
  auto logs = std::make_shared<monitostr::model::LogBuffer>();

  auto session =
      std::make_shared<RelaySession>(io_context, ssl_context, background_pool, stats, logs, "relay.example", "pubkey");
  session->Start();
  io_context.poll();

  const auto snapshot = stats->Snapshot();
  REQUIRE(snapshot.size() == 1);
  CHECK(snapshot.front().status == monitostr::model::RelayStatus::kError);
  CHECK(snapshot.front().last_error == "invalid relay URL");
}

TEST_CASE("RelaySession rejects ws relay URLs before attempting a connection", "[integration][relay_session]") {
  boost::asio::io_context io_context;
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);
  boost::asio::thread_pool background_pool(1);
  auto stats = std::make_shared<monitostr::model::RelayStats>();
  auto logs = std::make_shared<monitostr::model::LogBuffer>();

  auto session = std::make_shared<RelaySession>(io_context, ssl_context, background_pool, stats, logs,
                                                "ws://relay.example", "pubkey");
  session->Start();
  io_context.poll();

  const auto snapshot = stats->Snapshot();
  REQUIRE(snapshot.size() == 1);
  CHECK(snapshot.front().status == monitostr::model::RelayStatus::kError);
  CHECK(snapshot.front().last_error == "unsupported ws:// relay URL");
}

TEST_CASE("RelaySession completes subscribe, auth, and NIP-11 flows against a local TLS relay",
          "[integration][relay_session]") {
  LocalTlsRelayServer server(/*issue_auth_challenge=*/true);

  boost::asio::io_context io_context;
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);
  ConfigureClientTlsContext(ssl_context);
  boost::asio::thread_pool background_pool(1);
  auto stats = std::make_shared<monitostr::model::RelayStats>();
  auto logs = std::make_shared<monitostr::model::LogBuffer>();

  auto session = std::make_shared<RelaySession>(io_context, ssl_context, background_pool, stats, logs,
                                                "wss://localhost:" + std::to_string(server.port()) + "/nostr",
                                                "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
  auto auth_key =
      monitostr::nostr::AuthKey::FromHexPrivkey("0000000000000000000000000000000000000000000000000000000000000001");
  REQUIRE(auth_key);
  session->SetAuthKey(auth_key);
  session->Start();

  REQUIRE(PumpUntil(
      io_context,
      [&]() {
        const auto snapshot = stats->Snapshot();
        return snapshot.size() == 1 && snapshot.front().events_count == 1 && snapshot.front().latency_ms.has_value() &&
               snapshot.front().supported_nips == std::vector<int>({1, 11, 42});
      },
      std::chrono::milliseconds(5000)));

  session->Stop();
  PumpUntil(
      io_context,
      [&]() {
        const auto snapshot = stats->Snapshot();
        return snapshot.size() == 1 && snapshot.front().status == monitostr::model::RelayStatus::kDisconnected;
      },
      std::chrono::milliseconds(1000));

  server.RethrowIfFailed();
  const auto server_snapshot = server.Snapshot();
  REQUIRE(server_snapshot.saw_ws_upgrade);
  CHECK(server_snapshot.ws_target == "/nostr");
  REQUIRE(server_snapshot.saw_nip11_request);
  CHECK(server_snapshot.nip11_target == "/");
  REQUIRE_FALSE(server_snapshot.monitor_request.empty());
  REQUIRE(server_snapshot.auth_message.has_value());

  const auto req = nlohmann::json::parse(server_snapshot.monitor_request);
  CHECK(req[0] == "REQ");
  CHECK(req[1] == "monitostr-sub");
  CHECK(req[2]["limit"] == 1);

  const auto auth = nlohmann::json::parse(*server_snapshot.auth_message);
  CHECK(auth[0] == "AUTH");
  REQUIRE(auth[1].is_object());
  CHECK(auth[1]["kind"] == 22242);

  const auto snapshot = stats->Snapshot();
  REQUIRE(snapshot.size() == 1);
  CHECK(snapshot.front().events_count == 1);
  CHECK(snapshot.front().supported_nips == std::vector<int>({1, 11, 42}));
  CHECK(snapshot.front().latency_ms.has_value());

  const auto log_entries = logs->Snapshot();
  bool saw_auth_sent = false;
  bool saw_nip11_loaded = false;
  for (const auto& entry : log_entries) {
    if (entry.message.find("NIP-42 AUTH sent") != std::string::npos) {
      saw_auth_sent = true;
    }
    if (entry.message.find("NIP-11 capabilities loaded") != std::string::npos) {
      saw_nip11_loaded = true;
    }
  }
  CHECK(saw_auth_sent);
  CHECK(saw_nip11_loaded);
}

}  // namespace monitostr::net
