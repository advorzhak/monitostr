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

#include "monitostr/net/session_manager.hpp"

namespace monitostr::net {

SessionManager::SessionManager(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
                               std::shared_ptr<monitostr::model::RelayStats> shared_stats,
                               std::shared_ptr<monitostr::model::LogBuffer> log_buffer)
    : io_context_(io_context),
      ssl_context_(ssl_context),
      shared_stats_(std::move(shared_stats)),
      log_buffer_(std::move(log_buffer)) {}

void SessionManager::Start(const std::vector<std::string>& relay_urls, const std::string& hex_pubkey,
                           std::shared_ptr<const monitostr::nostr::AuthKey> auth_key) {
  StopAll();
  if (log_buffer_) {
    log_buffer_->Info("Starting relay sessions for " + std::to_string(relay_urls.size()) + " relays");
  }
  sessions_.reserve(relay_urls.size());
  for (const auto& relay_url : relay_urls) {
    auto session =
        std::make_shared<RelaySession>(io_context_, ssl_context_, shared_stats_, log_buffer_, relay_url, hex_pubkey);
    session->SetAuthKey(auth_key);
    sessions_.push_back(session);
    session->Start();
  }
}

void SessionManager::UpdateAuthKey(std::shared_ptr<const monitostr::nostr::AuthKey> auth_key) {
  for (const auto& session : sessions_) {
    session->SetAuthKey(auth_key);
  }
  if (log_buffer_) {
    log_buffer_->Info("NIP-42 auth key updated on " + std::to_string(sessions_.size()) + " running sessions");
  }
}

bool SessionManager::HasActiveSessions() const { return !sessions_.empty(); }

void SessionManager::StopAll() {
  if (!sessions_.empty() && log_buffer_) {
    log_buffer_->Info("Stopping " + std::to_string(sessions_.size()) + " relay sessions");
  }
  for (const auto& session : sessions_) {
    session->Stop();
  }
  sessions_.clear();
}

}  // namespace monitostr::net
