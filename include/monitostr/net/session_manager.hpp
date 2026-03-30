#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <memory>
#include <string>
#include <vector>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/model/relay_stats.hpp"
#include "monitostr/net/relay_session.hpp"
#include "monitostr/nostr/auth_key.hpp"

namespace monitostr::net {

class SessionManager {
 public:
  SessionManager(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
                 std::shared_ptr<monitostr::model::RelayStats> shared_stats,
                 std::shared_ptr<monitostr::model::LogBuffer> log_buffer);

  void Start(const std::vector<std::string>& relay_urls, const std::string& hex_pubkey,
             std::shared_ptr<const monitostr::nostr::AuthKey> auth_key = nullptr);
  void StopAll();

 private:
  boost::asio::io_context& io_context_;
  boost::asio::ssl::context& ssl_context_;
  std::shared_ptr<monitostr::model::RelayStats> shared_stats_;
  std::shared_ptr<monitostr::model::LogBuffer> log_buffer_;
  std::vector<std::shared_ptr<RelaySession>> sessions_;
};

}  // namespace monitostr::net
