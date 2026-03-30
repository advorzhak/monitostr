#pragma once

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/io_context.hpp>

#include <functional>
#include <string>
#include <vector>

#include "monitostr/model/log_buffer.hpp"

namespace monitostr::net {

struct BootstrapResult {
  bool ok = false;
  std::vector<std::string> relay_urls;
  std::string source_event_kind;
  std::string error;
};

class BootstrapClient {
 public:
  using Completion = std::function<void(BootstrapResult)>;

  BootstrapClient(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context);

  void SetLogBuffer(std::shared_ptr<monitostr::model::LogBuffer> log_buffer);

  void ResolveRelaysForPubkey(const std::string& hex_pubkey, Completion completion,
                              const std::string& seed_relay_url = "wss://relay.damus.io");

 private:
  boost::asio::io_context& io_context_;
  [[maybe_unused]] boost::asio::ssl::context& ssl_context_;
  std::shared_ptr<monitostr::model::LogBuffer> log_buffer_;
};

}  // namespace monitostr::net
