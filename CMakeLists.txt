cmake_minimum_required(VERSION 3.24)
project(monitostr VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(MONITOSTR_ENABLE_SANITIZERS "Enable AddressSanitizer, ThreadSanitizer, and UndefinedBehaviorSanitizer" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerSettings)
include(Dependencies)

if(MONITOSTR_ENABLE_SANITIZERS)
  set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
  add_compile_options(${SANITIZER_FLAGS})
  add_link_options(${SANITIZER_FLAGS})
  message(STATUS "Sanitizers enabled: address, undefined")
endif()

# Generate version info from git and build configuration
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE MONITOSTR_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(NOT MONITOSTR_GIT_HASH)
    set(MONITOSTR_GIT_HASH "unknown")
  endif()
else()
  set(MONITOSTR_GIT_HASH "unknown")
endif()

string(TIMESTAMP MONITOSTR_BUILD_DATE "%Y-%m-%d %H:%M:%S" UTC)
set(MONITOSTR_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}")
set(MONITOSTR_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
set(MONITOSTR_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/monitostr/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/monitostr/version.hpp"
  @ONLY
)

# Fix #16: all shared sources compile once into a static library that both
# the main executable and the test binary link against, eliminating duplication
# and build-time drift when new source files are added.
add_library(monitostr_core STATIC
  src/model_relay_stats.cpp
  src/nip19/npub.cpp
  src/nip19/nsec.cpp
  src/nostr/auth_key.cpp
  src/nostr/signer.cpp
  src/net/bootstrap_client.cpp
  src/net/relay_target.cpp
  src/net/relay_session_support.cpp
  src/net/relay_session.cpp
  src/net/session_manager.cpp
  src/ui/command_processor.cpp
  src/ui/navigation_controller.cpp
  src/ui/ui_renderer.cpp
)

target_include_directories(monitostr_core
  PUBLIC "${PROJECT_SOURCE_DIR}/include"
  PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(monitostr_core
  PUBLIC
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ftxui::screen
    ftxui::dom
    ftxui::component
    secp256k1::secp256k1
)

monitostr_apply_compiler_settings(monitostr_core)

add_executable(monitostr
  src/main.cpp
  src/ui/app.cpp
)

target_link_libraries(monitostr
  PRIVATE monitostr_core
)

monitostr_apply_compiler_settings(monitostr)

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG REQUIRED)

  add_executable(monitostr_tests
    tests/auth_key_test.cpp
    tests/nip19_decode_test.cpp
    tests/relay_target_test.cpp
    tests/relay_session_support_test.cpp
    tests/relay_stats_test.cpp
    tests/ui_command_processor_test.cpp
    tests/ui_navigation_controller_test.cpp
    tests/ui_renderer_test.cpp
    tests/relay_session_lifecycle_integration_test.cpp
  )

  target_link_libraries(monitostr_tests
    PRIVATE
      monitostr_core
      Catch2::Catch2WithMain
  )

  monitostr_apply_compiler_settings(monitostr_tests)

  include(Catch)
  catch_discover_tests(monitostr_tests)

  add_executable(monitostr_integration_tests
    tests/bootstrap_client_integration_test.cpp
  )

  target_link_libraries(monitostr_integration_tests
    PRIVATE
      monitostr_core
      Catch2::Catch2WithMain
  )

  monitostr_apply_compiler_settings(monitostr_integration_tests)
  catch_discover_tests(monitostr_integration_tests)
endif()
