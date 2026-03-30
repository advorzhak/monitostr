#include <string>

#include <catch2/catch_test_macros.hpp>

#include "monitostr/nip19/npub.hpp"

namespace monitostr::nip19 {

TEST_CASE("DecodeNpubToHex decodes valid all-zero test vector", "[nip19]") {
  const std::string npub = "npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqzqujme";

  const DecodeResult result = DecodeNpubToHex(npub);

  REQUIRE(result.ok);
  CHECK(result.error.empty());
  CHECK(result.hex_pubkey == "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST_CASE("DecodeNpubToHex rejects mixed-case input", "[nip19]") {
  const std::string mixed_case = "npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqzqujMe";

  const DecodeResult result = DecodeNpubToHex(mixed_case);

  REQUIRE_FALSE(result.ok);
  CHECK(result.error == "mixed-case bech32 string");
}

TEST_CASE("DecodeNpubToHex rejects invalid HRP", "[nip19]") {
  const DecodeResult result = DecodeNpubToHex("note1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp67l4f");

  REQUIRE_FALSE(result.ok);
  CHECK(result.error == "hrp is not npub");
}

TEST_CASE("DecodeNpubToHex rejects malformed separators", "[nip19]") {
  const DecodeResult result = DecodeNpubToHex("npub-no-separator");

  REQUIRE_FALSE(result.ok);
  CHECK(result.error == "invalid bech32 separator/checksum");
}

TEST_CASE("EncodeNpubFromHex round-trips with DecodeNpubToHex", "[nip19]") {
  const std::string hex = "0000000000000000000000000000000000000000000000000000000000000000";
  const EncodeResult enc = EncodeNpubFromHex(hex);

  REQUIRE(enc.ok);
  CHECK(enc.error.empty());

  const DecodeResult dec = DecodeNpubToHex(enc.npub);
  REQUIRE(dec.ok);
  CHECK(dec.hex_pubkey == hex);
}

TEST_CASE("EncodeNpubFromHex rejects invalid hex", "[nip19]") {
  const EncodeResult result = EncodeNpubFromHex("not-hex");

  REQUIRE_FALSE(result.ok);
  CHECK_FALSE(result.error.empty());
}

}  // namespace monitostr::nip19
