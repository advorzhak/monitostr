#include "monitostr/nip19/nsec.hpp"
#include "bech32_internal.hpp"

namespace monitostr::nip19 {

NsecDecodeResult DecodeNsecToHex(const std::string& nsec) {
  NsecDecodeResult result;
  std::string error;
  const auto bytes = internal::DecodeBech32_32Bytes(nsec, "nsec", error);
  if (bytes.empty()) {
    result.error = std::move(error);
    return result;
  }
  result.ok = true;
  result.hex_privkey = internal::BytesToHex(bytes);
  return result;
}

}  // namespace monitostr::nip19
