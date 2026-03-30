# Prepend the Homebrew prefix so find_package locates keg-only packages
# (openssl@3) and non-default prefix packages (ftxui, nlohmann-json) reliably.
if(APPLE)
  execute_process(
    COMMAND brew --prefix
    OUTPUT_VARIABLE HOMEBREW_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  # openssl is keg-only and needs its own prefix hint.
  execute_process(
    COMMAND brew --prefix openssl@3
    OUTPUT_VARIABLE HOMEBREW_OPENSSL_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  list(PREPEND CMAKE_PREFIX_PATH
    "${HOMEBREW_PREFIX}"
    "${HOMEBREW_OPENSSL_PREFIX}"
  )
  # Propagate to child scopes / cache so find_* helpers pick it up.
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "" FORCE)
endif()

# Boost 1.74+ ships BoostConfig.cmake with Boost::headers for all header-only
# libs including Asio, Beast, and System (header-only since 1.77).
# CMP0167: the legacy FindBoost module is gone; we use BoostConfig directly.
cmake_policy(SET CMP0167 NEW)
find_package(Boost 1.74 REQUIRED)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ftxui REQUIRED)

# secp256k1 ships a pkg-config file; find it via the Homebrew prefix.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SECP256K1 REQUIRED libsecp256k1)
add_library(secp256k1::secp256k1 INTERFACE IMPORTED)
target_include_directories(secp256k1::secp256k1 INTERFACE ${SECP256K1_INCLUDE_DIRS})
target_link_libraries(secp256k1::secp256k1 INTERFACE ${SECP256K1_LINK_LIBRARIES})
