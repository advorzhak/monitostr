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

## License

- This project is licensed under GNU General Public License v3.0 (GPL-3.0). See [LICENSE](LICENSE) for full text.
- All source files (.cpp, .hpp) must include the GPL-3.0 copyright header at the top (after #pragma once for headers).
- Copyright header format:
  ```cpp
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
  ```
- When creating new files, add this header before any other comments or includes.
- See .github/prompts/gplv3-compliance.prompt.md for automated header maintenance.
