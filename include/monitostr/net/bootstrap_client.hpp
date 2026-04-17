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

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

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
  boost::asio::thread_pool worker_pool_{1};
  std::shared_ptr<monitostr::model::LogBuffer> log_buffer_;
};

}  // namespace monitostr::net
