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

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/thread_pool.hpp>

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
                 std::shared_ptr<monitostr::model::LogBuffer> log_buffer, std::size_t background_threads = 2);

  void Start(const std::vector<std::string>& relay_urls, const std::string& hex_pubkey,
             std::shared_ptr<const monitostr::nostr::AuthKey> auth_key = nullptr);
  void UpdateAuthKey(std::shared_ptr<const monitostr::nostr::AuthKey> auth_key);
  bool HasActiveSessions() const;
  void StopAll();
  void Shutdown();

 private:
  boost::asio::io_context& io_context_;
  boost::asio::ssl::context& ssl_context_;
  boost::asio::thread_pool background_pool_;
  std::shared_ptr<monitostr::model::RelayStats> shared_stats_;
  std::shared_ptr<monitostr::model::LogBuffer> log_buffer_;
  std::vector<std::shared_ptr<RelaySession>> sessions_;
};

}  // namespace monitostr::net
