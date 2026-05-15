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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>            // boost::beast::ssl_stream type
#include <boost/beast/websocket/ssl.hpp>  // async_handshake specialisation
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <memory>
#include <string>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/model/relay_stats.hpp"
#include "monitostr/net/relay_session_support.hpp"
#include "monitostr/net/relay_target.hpp"
#include "monitostr/nostr/auth_key.hpp"

namespace monitostr::net {

class RelaySession : public std::enable_shared_from_this<RelaySession> {
 public:
  RelaySession(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
               boost::asio::thread_pool& background_pool, std::shared_ptr<monitostr::model::RelayStats> shared_stats,
               std::shared_ptr<monitostr::model::LogBuffer> log_buffer, std::string relay_url, std::string hex_pubkey);

  void Start();
  void Stop();
  // Must be called before Start(), or after Start() to update a live session.
  // Thread-safe: dispatches the assignment onto the session strand.
  // Null disables NIP-42 auth.
  void SetAuthKey(std::shared_ptr<const monitostr::nostr::AuthKey> auth_key);

 private:
  using Tcp = boost::asio::ip::tcp;
  using Resolver = Tcp::resolver;
  using WsStream = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;

  void ParseRelayUrl();

  void StartResolve();
  void OnResolve(const boost::system::error_code& ec, const Resolver::results_type& results);
  void OnConnect(const boost::system::error_code& ec, const Resolver::results_type::endpoint_type& endpoint);
  void OnTlsHandshake(const boost::system::error_code& ec);
  void OnWsHandshake(const boost::system::error_code& ec);
  void OnWriteReq(const boost::system::error_code& ec, std::size_t bytes_transferred);
  void DoRead();
  void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred);

  std::string BuildReqMessage() const;
  void HandleMessage(const std::string& payload);
  void OnAuthChallenge(const std::string& challenge);
  void MarkError(const std::string& where, const boost::system::error_code& ec);
  bool ShouldReconnect(const std::string& where) const;
  void ScheduleReconnect(const std::string& reason);
  void ResetTransportForReconnect();

  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  Resolver resolver_;
  WsStream ws_;
  boost::asio::steady_timer reconnect_timer_;
  boost::beast::flat_buffer read_buffer_;

  boost::asio::ssl::context& ssl_context_;
  boost::asio::thread_pool& background_pool_;
  std::shared_ptr<monitostr::model::RelayStats> shared_stats_;
  std::shared_ptr<monitostr::model::LogBuffer> log_buffer_;
  std::string relay_url_;
  std::string hex_pubkey_;
  ParsedRelayUrl parsed_relay_;

  bool stopped_ = false;
  bool ws_connected_ = false;                                  // true only after a successful WebSocket handshake
  std::shared_ptr<const monitostr::nostr::AuthKey> auth_key_;  // null → NIP-42 auth disabled
  std::size_t reconnect_attempt_ = 0;
  std::chrono::steady_clock::time_point req_sent_at_{};
};

}  // namespace monitostr::net
