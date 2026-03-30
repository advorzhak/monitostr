#include <chrono>
#include <future>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <catch2/catch_test_macros.hpp>

#include "monitostr/net/bootstrap_client.hpp"

namespace monitostr::net {

TEST_CASE("BootstrapClient returns error for empty pubkey without network dependency", "[integration][bootstrap]") {
  boost::asio::io_context io_context;
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);

  BootstrapClient bootstrap(io_context, ssl_context);

  std::promise<BootstrapResult> completion;
  auto future = completion.get_future();

  bootstrap.ResolveRelaysForPubkey("", [&](BootstrapResult result) { completion.set_value(std::move(result)); });

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
         std::chrono::steady_clock::now() < deadline) {
    io_context.restart();
    io_context.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  REQUIRE(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);

  const auto result = future.get();
  REQUIRE_FALSE(result.ok);
  CHECK(result.error == "empty hex pubkey");
  CHECK(result.relay_urls.empty());
}

}  // namespace monitostr::net
