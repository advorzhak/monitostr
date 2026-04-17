cmake_minimum_required(VERSION 3.24)
project(monitostr VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerSettings)
include(Dependencies)

add_executable(monitostr
  src/main.cpp
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
  src/ui/app.cpp
)

target_include_directories(monitostr
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(monitostr
  PRIVATE
    Boost::headers   # header-only: Asio, Beast, System (since 1.77)
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ftxui::screen
    ftxui::dom
    ftxui::component
    secp256k1::secp256k1
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
    src/model_relay_stats.cpp
    src/nip19/npub.cpp
    src/net/relay_target.cpp
    src/net/relay_session_support.cpp
    src/net/relay_session.cpp
    src/nostr/auth_key.cpp
    src/nostr/signer.cpp
    src/ui/command_processor.cpp
    src/ui/navigation_controller.cpp
    src/ui/ui_renderer.cpp
  )

  target_include_directories(monitostr_tests
    PRIVATE
      "${PROJECT_SOURCE_DIR}/include"
  )

  target_link_libraries(monitostr_tests
    PRIVATE
      Catch2::Catch2WithMain
      OpenSSL::Crypto
      OpenSSL::SSL
      secp256k1::secp256k1
      nlohmann_json::nlohmann_json
      ftxui::screen
      ftxui::dom
      ftxui::component
  )

  monitostr_apply_compiler_settings(monitostr_tests)

  include(Catch)
  catch_discover_tests(monitostr_tests)

  add_executable(monitostr_integration_tests
    tests/bootstrap_client_integration_test.cpp
    src/net/bootstrap_client.cpp
    src/net/relay_target.cpp
  )

  target_include_directories(monitostr_integration_tests
    PRIVATE
      "${PROJECT_SOURCE_DIR}/include"
  )

  target_link_libraries(monitostr_integration_tests
    PRIVATE
      Boost::headers
      OpenSSL::SSL
      OpenSSL::Crypto
      nlohmann_json::nlohmann_json
      Catch2::Catch2WithMain
  )

  monitostr_apply_compiler_settings(monitostr_integration_tests)
  catch_discover_tests(monitostr_integration_tests)
endif()
