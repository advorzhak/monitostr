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

#include "monitostr/nip19/npub.hpp"
#include "bech32_internal.hpp"

namespace monitostr::nip19 {

DecodeResult DecodeNpubToHex(const std::string& npub) {
  DecodeResult result;
  std::string error;
  const auto bytes = internal::DecodeBech32_32Bytes(npub, "npub", error);
  if (bytes.empty()) {
    result.error = std::move(error);
    return result;
  }
  result.ok = true;
  result.hex_pubkey = internal::BytesToHex(bytes);
  return result;
}

EncodeResult EncodeNpubFromHex(const std::string& hex_pubkey) {
  EncodeResult result;
  const auto bytes = internal::HexToBytes(hex_pubkey);
  if (bytes.size() != 32U) {
    result.error = "hex_pubkey must be 64 lowercase hex chars (32 bytes)";
    return result;
  }
  const std::string encoded = internal::EncodeBech32_32Bytes("npub", bytes);
  if (encoded.empty()) {
    result.error = "bech32 encoding failed";
    return result;
  }
  result.ok = true;
  result.npub = encoded;
  return result;
}

}  // namespace monitostr::nip19
