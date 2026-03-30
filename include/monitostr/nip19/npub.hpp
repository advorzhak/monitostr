#pragma once

#include <optional>
#include <string>

namespace monitostr::nip19 {

struct DecodeResult {
  bool ok = false;
  std::string hex_pubkey;
  std::string error;
};

DecodeResult DecodeNpubToHex(const std::string& npub);

struct EncodeResult {
  bool ok = false;
  std::string npub;
  std::string error;
};

EncodeResult EncodeNpubFromHex(const std::string& hex_pubkey);

}  // namespace monitostr::nip19
