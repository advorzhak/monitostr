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

#include <optional>
#include <string>
#include <string_view>

namespace monitostr::nip19 {

struct DecodeResult {
  bool ok = false;
  std::string hex_pubkey;
  std::string error;
};

[[nodiscard]] DecodeResult DecodeNpubToHex(std::string_view npub);

struct EncodeResult {
  bool ok = false;
  std::string npub;
  std::string error;
};

[[nodiscard]] EncodeResult EncodeNpubFromHex(std::string_view hex_pubkey);

}  // namespace monitostr::nip19
