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

#include "monitostr/nostr/auth_key.hpp"

#include <openssl/crypto.h>

#include <array>
#include <utility>

#include "monitostr/nostr/signer.hpp"
#include "monitostr/security/secure_memory.hpp"

namespace monitostr::nostr {
namespace {

bool HexToBytes32(const std::string& hex, std::array<unsigned char, 32>& out) {
  if (hex.size() != 64U) {
    return false;
  }

  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };

  for (std::size_t index = 0; index < out.size(); ++index) {
    const int hi = nibble(hex[index * 2]);
    const int lo = nibble(hex[index * 2 + 1]);
    if (hi < 0 || lo < 0) {
      monitostr::security::SecureZero(out);
      return false;
    }
    out[index] = static_cast<unsigned char>((hi << 4) | lo);
  }

  return true;
}

}  // namespace

std::shared_ptr<AuthKey> AuthKey::FromHexPrivkey(std::string hex_privkey) {
  std::array<unsigned char, 32> privkey_bytes{};
  if (!HexToBytes32(hex_privkey, privkey_bytes)) {
    monitostr::security::SecureClearString(hex_privkey);
    return nullptr;
  }

  const std::string hex_pubkey = DeriveHexPubkeyFromPrivkey(privkey_bytes);
  monitostr::security::SecureClearString(hex_privkey);
  if (hex_pubkey.empty()) {
    monitostr::security::SecureZero(privkey_bytes);
    return nullptr;
  }

  return std::shared_ptr<AuthKey>(new AuthKey(std::move(privkey_bytes), hex_pubkey));
}

AuthKey::AuthKey(std::array<unsigned char, 32> privkey_bytes, std::string hex_pubkey)
    : privkey_bytes_(std::move(privkey_bytes)), hex_pubkey_(std::move(hex_pubkey)) {}

AuthKey::~AuthKey() { monitostr::security::SecureZero(privkey_bytes_); }

}  // namespace monitostr::nostr
