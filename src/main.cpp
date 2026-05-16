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
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ssl/context.hpp>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "monitostr/model/log_buffer.hpp"
#include "monitostr/model/relay_stats.hpp"
#include "monitostr/net/bootstrap_client.hpp"
#include "monitostr/net/session_manager.hpp"
#include "monitostr/nip19/npub.hpp"
#include "monitostr/nip19/nsec.hpp"
#include "monitostr/nostr/auth_key.hpp"
#include "monitostr/nostr/signer.hpp"
#include "monitostr/security/secure_memory.hpp"
#include "monitostr/ui/app.hpp"
#include "monitostr/version.hpp"

namespace {

void PrintVersion() {
  std::cout << "monitostr " << monitostr::kVersion << "\n"
            << "commit:    " << monitostr::kGitHash << "\n"
            << "built:     " << monitostr::kBuildDate << " UTC\n"
            << "compiler:  " << monitostr::kCompilerId << " " << monitostr::kCompilerVersion << "\n"
            << "build:     " << (monitostr::kBuildType[0] ? monitostr::kBuildType : "unknown") << "\n";
}

void ConfigureTlsContext(boost::asio::ssl::context& ctx) {
  ctx.set_default_verify_paths();
  ctx.set_verify_mode(boost::asio::ssl::verify_peer);
}

}  // namespace

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--version" || arg == "-v" || arg == "-V") {
      PrintVersion();
      return 0;
    }
  }
  boost::asio::io_context io_context;
  auto work_guard = boost::asio::make_work_guard(io_context);
  boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_client);
  ConfigureTlsContext(ssl_context);

  auto shared_stats = std::make_shared<monitostr::model::RelayStats>();
  auto log_buffer = std::make_shared<monitostr::model::LogBuffer>();
  log_buffer->Info("Application started");

  monitostr::net::BootstrapClient bootstrap(io_context);
  bootstrap.SetLogBuffer(log_buffer);
  monitostr::net::SessionManager session_manager(io_context, ssl_context, shared_stats, log_buffer);

  std::thread io_thread([&io_context] { io_context.run(); });

  // auth_key is written only on the UI thread; it is read by the bootstrap
  // completion callback (which runs on the io thread) by value-capturing a
  // snapshot at the moment start_monitoring is called — eliminating the race.
  std::shared_ptr<const monitostr::nostr::AuthKey> auth_key;

  monitostr::ui::App app(
      shared_stats, log_buffer,
      // NpubSubmit callback
      [&](std::string_view npub) {
        log_buffer->Info("New npub submitted");
        auto decode = monitostr::nip19::DecodeNpubToHex(npub);
        if (!decode.ok) {
          log_buffer->Error("Invalid npub: " + decode.error);
          std::cerr << "Invalid npub: " << decode.error << std::endl;
          return;
        }
        app.SetHeaderContext({.npub = std::string(npub), .hex_pubkey = decode.hex_pubkey});
        log_buffer->Info("npub decoded successfully");
        shared_stats->Reset();
        log_buffer->Info("Relay stats reset");
        const std::string hex_pubkey = decode.hex_pubkey;

        // Fix #3: capture auth_key by value here (UI thread) so that the
        // bootstrap completion callback (io thread) reads from its own copy and
        // never races with a concurrent NsecSubmit write to auth_key.
        auto auth_key_snapshot = auth_key;
        bootstrap.ResolveRelaysForPubkey(
            hex_pubkey, [&, hex_pubkey, auth_key_snapshot](const monitostr::net::BootstrapResult& result) {
              if (!result.ok) {
                log_buffer->Error("Bootstrap failed: " + result.error);
                std::cerr << "Bootstrap failed: " << result.error << std::endl;
                return;
              }
              log_buffer->Info("Bootstrap returned " + std::to_string(result.relay_urls.size()) + " relays");
              session_manager.Start(result.relay_urls, hex_pubkey, auth_key_snapshot);
            });
      },
      // NsecSubmit callback (:auth nsec1... or nsec entered in Insert mode)
      [&](std::string_view nsec) {
        auto decode = monitostr::nip19::DecodeNsecToHex(nsec);
        if (!decode.ok) {
          log_buffer->Error("Invalid nsec: " + decode.error);
          return;
        }
        auto next_auth_key = monitostr::nostr::AuthKey::FromHexPrivkey(std::move(decode.hex_privkey));
        monitostr::security::SecureClearString(decode.hex_privkey);
        if (!next_auth_key) {
          log_buffer->Error("nsec key derivation failed");
          return;
        }
        auth_key = next_auth_key;
        const std::string& derived_pubkey = auth_key->hex_pubkey();
        log_buffer->Info("NIP-42 auth key set (pubkey: " + derived_pubkey.substr(0, 16) + "...)");

        if (session_manager.HasActiveSessions()) {
          // Sessions already running — push the new key to them without restarting.
          // Each session will use it on the next AUTH challenge from a relay.
          session_manager.UpdateAuthKey(auth_key);
          return;
        }

        // No sessions yet — derive npub and start monitoring automatically.
        const auto npub_result = monitostr::nip19::EncodeNpubFromHex(derived_pubkey);
        if (!npub_result.ok) {
          log_buffer->Error("Failed to encode npub from nsec: " + npub_result.error);
          return;
        }
        log_buffer->Info("npub derived from nsec, starting monitoring");
        auto npub_decode = monitostr::nip19::DecodeNpubToHex(npub_result.npub);
        if (!npub_decode.ok) {
          log_buffer->Error("Invalid npub from nsec: " + npub_decode.error);
          return;
        }
        app.SetHeaderContext({.npub = npub_result.npub, .hex_pubkey = npub_decode.hex_pubkey});
        shared_stats->Reset();
        log_buffer->Info("Relay stats reset");
        const std::string hex_pubkey = npub_decode.hex_pubkey;
        auto auth_key_snapshot = auth_key;
        bootstrap.ResolveRelaysForPubkey(
            hex_pubkey, [&, hex_pubkey, auth_key_snapshot](const monitostr::net::BootstrapResult& result) {
              if (!result.ok) {
                log_buffer->Error("Bootstrap failed: " + result.error);
                std::cerr << "Bootstrap failed: " << result.error << std::endl;
                return;
              }
              log_buffer->Info("Bootstrap returned " + std::to_string(result.relay_urls.size()) + " relays");
              session_manager.Start(result.relay_urls, hex_pubkey, auth_key_snapshot);
            });
      });

  app.SetHeaderContext({.npub = "", .hex_pubkey = ""});
  app.Run();

  session_manager.StopAll();
  session_manager.Shutdown();
  bootstrap.Shutdown();
  log_buffer->Info("Application stopping");
  work_guard.reset();
  io_context.stop();
  io_thread.join();
  return 0;
}
