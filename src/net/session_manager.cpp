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
