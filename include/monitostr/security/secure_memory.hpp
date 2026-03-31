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

#include <openssl/crypto.h>

#include <array>
#include <cstddef>
#include <string>

namespace monitostr::security {

inline void SecureZero(void* data, std::size_t size) {
  if (data != nullptr && size > 0) {
    OPENSSL_cleanse(data, size);
  }
}

template <typename T, std::size_t N>
inline void SecureZero(std::array<T, N>& data) {
  SecureZero(data.data(), data.size() * sizeof(T));
}

inline void SecureClearString(std::string& value) {
  if (!value.empty()) {
    SecureZero(value.data(), value.size());
    value.clear();
    value.shrink_to_fit();
  }
}

inline void SecurePopBack(std::string& value) {
  if (!value.empty()) {
    SecureZero(value.data() + value.size() - 1, 1);
    value.pop_back();
  }
}

}  // namespace monitostr::security
