# Project Guidelines

## Code Style

- Use C++20 and keep implementation in existing source modules under src and headers under include.
- Prefer small, focused changes; do not reformat unrelated code.
- Use descriptive logging messages with relay URL context for network operations.

## Architecture

- Keep UI concerns in src/ui/app.cpp and avoid networking logic in the UI layer.
- Keep relay/network flow in src/net and update shared state through RelayStats and LogBuffer only.
- Prefer asynchronous, non-blocking behavior for network paths.

## Build and Test

- Install dependencies with Homebrew: brew bundle.
- Configure/build with CMake: cmake -S . -B build && cmake --build build -j4.
- Run tests through CTest: ctest --test-dir build --output-on-failure.

## Conventions

- Add tests for pure logic or deterministic behavior when changing decoding, state models, or orchestration.
- Keep macOS-specific behavior explicit (for example clipboard integration with pbcopy).
- For relay failures, include phase labels (resolve, connect, tls_handshake, ws_handshake, read, write) in logs.

## Documentation

- When user-facing behavior changes, update README.md in the same change.
- Treat command names, input flows, keybindings, and mode behavior as user-facing behavior that must be documented.
- When adding or changing agent customization files under .github/, keep their descriptions, scope, and expected output aligned with current repo behavior.
- If a change introduces a repeatable documentation maintenance workflow, prefer a focused prompt under .github/prompts/ instead of adding broad generic instructions.
