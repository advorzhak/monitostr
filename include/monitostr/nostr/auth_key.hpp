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

#include <array>
#include <memory>
#include <string>

namespace monitostr::nostr {

class AuthKey {
 public:
  static std::shared_ptr<AuthKey> FromHexPrivkey(std::string hex_privkey);

  AuthKey(const AuthKey&) = delete;
  AuthKey& operator=(const AuthKey&) = delete;

  ~AuthKey();

  const std::array<unsigned char, 32>& privkey_bytes() const { return privkey_bytes_; }
  const std::string& hex_pubkey() const { return hex_pubkey_; }

 private:
  AuthKey(std::array<unsigned char, 32> privkey_bytes, std::string hex_pubkey);

  std::array<unsigned char, 32> privkey_bytes_{};
  std::string hex_pubkey_;
};

}  // namespace monitostr::nostr
