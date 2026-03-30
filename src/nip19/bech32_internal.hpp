// Internal bech32 decoding helpers shared between nip19 modules.
// Not part of the public API — include only from src/nip19/*.cpp.
#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace monitostr::nip19::internal {

constexpr std::uint32_t kBech32Gen[5] = {
    0x3b6a57b2U, 0x26508e6dU, 0x1ea119faU, 0x3d4233ddU, 0x2a1462b3U,
};

inline int CharToValue(char c) {
  static constexpr const char* kCharset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
  const char* p = std::strchr(kCharset, c);
  if (p == nullptr) {
    return -1;
  }
  return static_cast<int>(p - kCharset);
}

inline std::uint32_t Polymod(const std::vector<std::uint8_t>& values) {
  std::uint32_t chk = 1;
  for (const std::uint8_t v : values) {
    const std::uint8_t top = static_cast<std::uint8_t>(chk >> 25U);
    chk = (chk & 0x1ffffffU) << 5U;
    chk ^= v;
    for (int i = 0; i < 5; ++i) {
      if (((top >> i) & 1U) != 0U) {
        chk ^= kBech32Gen[i];
      }
    }
  }
  return chk;
}

inline std::vector<std::uint8_t> HrpExpand(const std::string& hrp) {
  std::vector<std::uint8_t> out;
  out.reserve(hrp.size() * 2U + 1U);
  for (const char c : hrp) {
    out.push_back(static_cast<std::uint8_t>(c >> 5));
  }
  out.push_back(0);
  for (const char c : hrp) {
    out.push_back(static_cast<std::uint8_t>(c & 31));
  }
  return out;
}

inline bool VerifyChecksum(const std::string& hrp, const std::vector<std::uint8_t>& data) {
  std::vector<std::uint8_t> values = HrpExpand(hrp);
  values.insert(values.end(), data.begin(), data.end());
  return Polymod(values) == 1U;
}

inline bool ConvertBits(const std::vector<std::uint8_t>& in, int from_bits, int to_bits, bool pad,
                        std::vector<std::uint8_t>* out) {
  std::uint32_t acc = 0;
  int bits = 0;
  const std::uint32_t maxv = static_cast<std::uint32_t>((1 << to_bits) - 1);
  const std::uint32_t max_acc = static_cast<std::uint32_t>((1 << (from_bits + to_bits - 1)) - 1);

  for (const std::uint8_t value : in) {
    if ((value >> from_bits) != 0U) {
      return false;
    }
    acc = ((acc << from_bits) | value) & max_acc;
    bits += from_bits;
    while (bits >= to_bits) {
      bits -= to_bits;
      out->push_back(static_cast<std::uint8_t>((acc >> bits) & maxv));
    }
  }

  if (pad) {
    if (bits > 0) {
      out->push_back(static_cast<std::uint8_t>((acc << (to_bits - bits)) & maxv));
    }
  } else if (bits >= from_bits || ((acc << (to_bits - bits)) & maxv) != 0U) {
    return false;
  }

  return true;
}

inline std::string BytesToHex(const std::vector<std::uint8_t>& bytes) {
  std::ostringstream oss;
  oss << std::hex;
  for (std::uint8_t b : bytes) {
    oss.width(2);
    static_cast<void>(oss.fill('0'));
    oss << static_cast<int>(b);
  }
  return oss.str();
}

// Decode a bech32 string with the expected HRP into raw 32 bytes.
// On failure, returns empty vector and sets error_out.
inline std::vector<std::uint8_t> DecodeBech32_32Bytes(const std::string& input, const std::string& expected_hrp,
                                                      std::string& error_out) {
  if (input.empty()) {
    error_out = expected_hrp + " is empty";
    return {};
  }

  bool has_lower = false;
  bool has_upper = false;
  for (char c : input) {
    if (std::islower(static_cast<unsigned char>(c)) != 0) has_lower = true;
    if (std::isupper(static_cast<unsigned char>(c)) != 0) has_upper = true;
  }
  if (has_lower && has_upper) {
    error_out = "mixed-case bech32 string";
    return {};
  }

  std::string normalized = input;
  for (char& c : normalized) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  const std::size_t sep = normalized.rfind('1');
  if (sep == std::string::npos || sep == 0 || (sep + 7) > normalized.size()) {
    error_out = "invalid bech32 separator/checksum";
    return {};
  }

  const std::string hrp = normalized.substr(0, sep);
  if (hrp != expected_hrp) {
    error_out = "hrp is not " + expected_hrp;
    return {};
  }

  std::vector<std::uint8_t> data;
  data.reserve(normalized.size() - sep - 1U);
  for (std::size_t i = sep + 1; i < normalized.size(); ++i) {
    const int v = CharToValue(normalized[i]);
    if (v < 0) {
      error_out = "invalid bech32 character";
      return {};
    }
    data.push_back(static_cast<std::uint8_t>(v));
  }

  if (!VerifyChecksum(hrp, data)) {
    error_out = "invalid bech32 checksum";
    return {};
  }

  data.resize(data.size() - 6U);
  std::vector<std::uint8_t> bytes;
  if (!ConvertBits(data, 5, 8, false, &bytes)) {
    error_out = "failed bech32 bit conversion";
    return {};
  }
  if (bytes.size() != 32U) {
    error_out = expected_hrp + " payload is not 32 bytes";
    return {};
  }

  return bytes;
}

// Decodes a hex string into raw bytes.
// Returns empty vector if input is malformed.
inline std::vector<std::uint8_t> HexToBytes(const std::string& hex) {
  if (hex.size() % 2 != 0) return {};
  std::vector<std::uint8_t> out;
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    const int hi = nibble(hex[i]);
    const int lo = nibble(hex[i + 1]);
    if (hi < 0 || lo < 0) return {};
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return out;
}

// Computes a 6-symbol bech32 checksum for hrp + data.
inline std::vector<std::uint8_t> CreateChecksum(const std::string& hrp, const std::vector<std::uint8_t>& data) {
  std::vector<std::uint8_t> values = HrpExpand(hrp);
  values.insert(values.end(), data.begin(), data.end());
  for (int i = 0; i < 6; ++i) values.push_back(0);
  const std::uint32_t poly = Polymod(values) ^ 1U;
  std::vector<std::uint8_t> checksum(6);
  for (int i = 0; i < 6; ++i) {
    checksum[static_cast<std::size_t>(i)] =
        static_cast<std::uint8_t>((poly >> (5U * (5U - static_cast<unsigned>(i)))) & 0x1fU);
  }
  return checksum;
}

// Encodes exactly 32 raw bytes as a bech32 string with the given HRP.
// Returns empty string on failure.
inline std::string EncodeBech32_32Bytes(const std::string& hrp, const std::vector<std::uint8_t>& bytes_32) {
  if (bytes_32.size() != 32U) return {};
  std::vector<std::uint8_t> data5;
  data5.reserve(53);
  if (!ConvertBits(bytes_32, 8, 5, true, &data5)) return {};
  const auto checksum = CreateChecksum(hrp, data5);
  static constexpr const char* kCharset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
  std::string result;
  result.reserve(hrp.size() + 1 + data5.size() + 6);
  result += hrp;
  result += '1';
  for (const std::uint8_t v : data5) result += kCharset[v & 0x1fU];
  for (const std::uint8_t v : checksum) result += kCharset[v & 0x1fU];
  return result;
}

}  // namespace monitostr::nip19::internal
