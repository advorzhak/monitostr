#pragma once

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
