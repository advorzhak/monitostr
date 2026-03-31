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

#include <string>

namespace monitostr::nip19 {

struct NsecDecodeResult {
  bool ok = false;
  std::string hex_privkey;  // 32-byte private key as 64-char lowercase hex
  std::string error;
};

NsecDecodeResult DecodeNsecToHex(const std::string& nsec);

}  // namespace monitostr::nip19
