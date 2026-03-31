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

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "monitostr/nostr/auth_key.hpp"

namespace monitostr::nostr {

TEST_CASE("AuthKey derives x-only pubkey from valid private key hex", "[nostr][auth]") {
  auto auth_key = AuthKey::FromHexPrivkey("0000000000000000000000000000000000000000000000000000000000000001");

  REQUIRE(auth_key);
  CHECK(auth_key->hex_pubkey() == "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
}

TEST_CASE("AuthKey rejects malformed private key hex", "[nostr][auth]") {
  auto auth_key = AuthKey::FromHexPrivkey("not-hex");

  CHECK_FALSE(auth_key);
}

}  // namespace monitostr::nostr
