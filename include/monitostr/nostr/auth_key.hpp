#pragma once

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
