#pragma once

#include <string>

namespace monitostr::nip19 {

struct NsecDecodeResult {
  bool ok = false;
  std::string hex_privkey;  // 32-byte private key as 64-char lowercase hex
  std::string error;
};

NsecDecodeResult DecodeNsecToHex(const std::string& nsec);

}  // namespace monitostr::nip19
