#include "monitostr/nostr/signer.hpp"

#include <nlohmann/json.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "monitostr/security/secure_memory.hpp"

namespace monitostr::nostr {
namespace {

// Converts a 64-char lowercase hex string to 32 bytes.
// Returns false if the string is malformed.
bool HexToBytes32(const std::string& hex, unsigned char out[32]) {
  if (hex.size() != 64U) return false;
  for (std::size_t i = 0; i < 32; ++i) {
    const char hi = hex[i * 2];
    const char lo = hex[i * 2 + 1];
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    const int h = nibble(hi);
    const int l = nibble(lo);
    if (h < 0 || l < 0) return false;
    out[i] = static_cast<unsigned char>((h << 4) | l);
  }
  return true;
}

// Converts raw bytes to lowercase hex string.
std::string BytesToHex(const unsigned char* data, std::size_t len) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < len; ++i) {
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

// Computes SHA-256 of msg and writes 32 bytes into out.
void Sha256(const std::string& msg, unsigned char out[32]) {
  SHA256(reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), out);
}

// Builds the canonical NIP-01 serialisation string for event ID computation.
std::string SerialiseForId(const std::string& pubkey_hex, std::int64_t created_at, int kind, const nlohmann::json& tags,
                           const std::string& content) {
  nlohmann::json arr = nlohmann::json::array({
      0,
      pubkey_hex,
      created_at,
      kind,
      tags,
      content,
  });
  return arr.dump();
}

}  // namespace

std::string DeriveHexPubkeyFromPrivkey(const std::string& hex_privkey) {
  unsigned char privkey_bytes[32];
  if (!HexToBytes32(hex_privkey, privkey_bytes)) return {};

  std::array<unsigned char, 32> privkey_array{};
  std::copy(std::begin(privkey_bytes), std::end(privkey_bytes), privkey_array.begin());
  const std::string result = DeriveHexPubkeyFromPrivkey(privkey_array);
  monitostr::security::SecureZero(privkey_array);
  monitostr::security::SecureZero(privkey_bytes, sizeof(privkey_bytes));
  return result;
}

std::string DeriveHexPubkeyFromPrivkey(const std::array<unsigned char, 32>& privkey_bytes) {
  secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
  if (ctx == nullptr) return {};

  secp256k1_keypair keypair;
  if (secp256k1_keypair_create(ctx, &keypair, privkey_bytes.data()) != 1) {
    secp256k1_context_destroy(ctx);
    return {};
  }

  secp256k1_xonly_pubkey xonly;
  if (secp256k1_keypair_xonly_pub(ctx, &xonly, nullptr, &keypair) != 1) {
    secp256k1_context_destroy(ctx);
    return {};
  }

  unsigned char pubkey_bytes[32];
  secp256k1_xonly_pubkey_serialize(ctx, pubkey_bytes, &xonly);
  secp256k1_context_destroy(ctx);

  return BytesToHex(pubkey_bytes, 32);
}

std::string BuildAuthEvent(const std::string& relay_url, const std::string& challenge, const std::string& hex_privkey) {
  unsigned char privkey_bytes[32];
  if (!HexToBytes32(hex_privkey, privkey_bytes)) return {};

  std::array<unsigned char, 32> privkey_array{};
  std::copy(std::begin(privkey_bytes), std::end(privkey_bytes), privkey_array.begin());
  const std::string result = BuildAuthEvent(relay_url, challenge, privkey_array);
  monitostr::security::SecureZero(privkey_array);
  monitostr::security::SecureZero(privkey_bytes, sizeof(privkey_bytes));
  return result;
}

std::string BuildAuthEvent(const std::string& relay_url, const std::string& challenge,
                           const std::array<unsigned char, 32>& privkey_bytes) {
  secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
  if (ctx == nullptr) return {};

  // Randomise context to protect against side-channel nonce leakage.
  unsigned char ctx_seed[32];
  if (RAND_bytes(ctx_seed, 32) != 1) {
    secp256k1_context_destroy(ctx);
    return {};
  }
  (void)secp256k1_context_randomize(ctx, ctx_seed);

  secp256k1_keypair keypair;
  if (secp256k1_keypair_create(ctx, &keypair, privkey_bytes.data()) != 1) {
    monitostr::security::SecureZero(ctx_seed, sizeof(ctx_seed));
    secp256k1_context_destroy(ctx);
    return {};
  }

  // Derive x-only public key.
  secp256k1_xonly_pubkey xonly;
  if (secp256k1_keypair_xonly_pub(ctx, &xonly, nullptr, &keypair) != 1) {
    secp256k1_context_destroy(ctx);
    return {};
  }
  unsigned char pubkey_bytes[32];
  secp256k1_xonly_pubkey_serialize(ctx, pubkey_bytes, &xonly);
  const std::string pubkey_hex = BytesToHex(pubkey_bytes, 32);

  // Construct NIP-42 event fields.
  const std::int64_t created_at =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  constexpr int kKind = 22242;
  const nlohmann::json tags = nlohmann::json::array({
      nlohmann::json::array({"relay", relay_url}),
      nlohmann::json::array({"challenge", challenge}),
  });
  const std::string content;

  // Compute event ID = SHA256(canonical serialisation).
  const std::string serialised = SerialiseForId(pubkey_hex, created_at, kKind, tags, content);
  unsigned char event_id[32];
  Sha256(serialised, event_id);
  const std::string event_id_hex = BytesToHex(event_id, 32);

  // Sign the event ID with Schnorr.
  unsigned char aux_rand[32];
  if (RAND_bytes(aux_rand, 32) != 1) {
    monitostr::security::SecureZero(ctx_seed, sizeof(ctx_seed));
    secp256k1_context_destroy(ctx);
    return {};
  }
  unsigned char sig[64];
  if (secp256k1_schnorrsig_sign32(ctx, sig, event_id, &keypair, aux_rand) != 1) {
    monitostr::security::SecureZero(ctx_seed, sizeof(ctx_seed));
    monitostr::security::SecureZero(aux_rand, sizeof(aux_rand));
    monitostr::security::SecureZero(event_id, sizeof(event_id));
    secp256k1_context_destroy(ctx);
    return {};
  }
  monitostr::security::SecureZero(ctx_seed, sizeof(ctx_seed));
  monitostr::security::SecureZero(aux_rand, sizeof(aux_rand));
  monitostr::security::SecureZero(event_id, sizeof(event_id));
  secp256k1_context_destroy(ctx);

  const std::string sig_hex = BytesToHex(sig, 64);

  // Assemble the final event JSON.
  nlohmann::json event = {
      {"id", event_id_hex}, {"pubkey", pubkey_hex}, {"created_at", created_at}, {"kind", kKind},
      {"tags", tags},       {"content", content},   {"sig", sig_hex},
  };
  return event.dump();
}

}  // namespace monitostr::nostr
