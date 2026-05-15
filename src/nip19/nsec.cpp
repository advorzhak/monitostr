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

#include "monitostr/nip19/nsec.hpp"

#include "monitostr/security/secure_memory.hpp"
#include "bech32_internal.hpp"

namespace monitostr::nip19 {

NsecDecodeResult DecodeNsecToHex(const std::string& nsec) {
  NsecDecodeResult result;
  std::string error;
  // Fix #5: use a mutable vector so we can zero it after the hex conversion.
  auto bytes = internal::DecodeBech32_32Bytes(nsec, "nsec", error);
  if (bytes.empty()) {
    result.error = std::move(error);
    return result;
  }
  result.ok = true;
  result.hex_privkey = internal::BytesToHex(bytes);
  // Fix #5: zero the raw private key bytes now that they are in hex form.
  monitostr::security::SecureZero(bytes.data(), bytes.size());
  return result;
}

}  // namespace monitostr::nip19
