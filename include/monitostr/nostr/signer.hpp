#pragma once

#include <array>
#include <string>

namespace monitostr::nostr {

// Derives the x-only (BIP-340) public key from a 32-byte private key.
// hex_privkey: 64-char lowercase hex string.
// Returns 64-char lowercase hex pubkey, or empty string on failure.
std::string DeriveHexPubkeyFromPrivkey(const std::string& hex_privkey);
std::string DeriveHexPubkeyFromPrivkey(const std::array<unsigned char, 32>& privkey_bytes);

// Builds a complete NIP-42 kind-22242 AUTH event, signs it with hex_privkey,
// and returns its JSON representation suitable for use in ["AUTH", <event>].
// relay_url : full wss:// URL of the relay sending the challenge.
// challenge : challenge string received in the ["AUTH", "<challenge>"] message.
// hex_privkey: 64-char lowercase hex private key (from nsec decode).
// Returns the serialised event JSON string, or empty string on any failure.
std::string BuildAuthEvent(const std::string& relay_url, const std::string& challenge, const std::string& hex_privkey);
std::string BuildAuthEvent(const std::string& relay_url, const std::string& challenge,
                           const std::array<unsigned char, 32>& privkey_bytes);

}  // namespace monitostr::nostr
