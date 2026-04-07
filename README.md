# monitostr

NeoVim-inspired terminal UI for monitoring Nostr relay connectivity, latency, event flow, and relay capabilities for a given `npub` or `nsec`.

## Features

- NeoVim-style interaction model with Normal/Insert/Search/Command modes
- Split panes: relay table + logs, each with visible vertical scrollbars
- Always-visible bottom statusline
- Insert mode accepts either `npub1...` or `nsec1...`; when `nsec` is provided the app derives the matching `npub` automatically
- Relay discovery from Nostr metadata:
  - NIP-65 (`kind:10002`) relay list
  - fallback NIP-02 (`kind:3`) contacts relay hints
- TLS with certificate verification, hostname verification, and SNI
- NIP-11 capability fetch (`supported_nips`) per relay
- NIP-42 relay auth support (challenge signing via `nsec`)
- Auto-reconnect with exponential backoff and jitter for recoverable errors
- Log utilities:
  - filter/search
  - copy all logs to clipboard (`pbcopy` on macOS)
  - write logs to file
- Live command hints while typing in Command mode

## Requirements

- macOS (current setup uses Homebrew)
- CMake >= 3.24
- C++20 compiler
- Homebrew packages:
  - `boost`
  - `catch2`
  - `secp256k1`
  - `openssl@3`
  - `nlohmann-json`
  - `ftxui`

You can install dependencies from the repo `Brewfile`:

```bash
brew bundle
```

## Build

```bash
cmake -S . -B build
cmake --build build -j4
```

## Run

```bash
./build/monitostr
```

## Optional Dev Shortcuts (zsh)

If you want short commands for configure/build/run/test flows, source the provided shortcuts file:

```bash
source ~/dev/advorzhak/monitostr/scripts/dev_aliases.zsh
```

Add that line to your `~/.zshrc` to load shortcuts automatically.

Available shortcuts:

- `mroot`: `cd` to repo root
- `mcfg`: configure Debug build
- `mbld`: build Debug
- `mrun`: run Debug binary
- `mrelcfg`: configure Release build
- `mrelbld`: build Release
- `mtest`: run tests from Debug build dir
- `mtesti`: run integration tests only
- `mclean`: remove build directories

These are implemented as zsh functions (not aliases), so extra arguments are passed through, for example:

- `mtest -R relay_stats`
- `mbld --target monitostr_tests`

Also included:

- `with_gnu <command> [args...]`: run a command with GNU coreutils in `PATH` for that one call only

## Usage

1. Launch the app.
2. Press `i` to enter Insert mode.
3. Paste/type either your `npub1...` or `nsec1...`.
4. Press `Enter` to submit.
5. If you entered `nsec1...`, the app derives the matching `npub` automatically and also keeps the key available for NIP-42 AUTH signing.
6. (Optional, for paid relays requiring NIP-42) enter Command mode with `:` and run `:auth nsec1...` to set or replace the signing key without retyping the public input.
7. Monitor relay status, latency, events, and supported NIPs.

## Keybindings

### Global / Navigation

- `h` / `l`: focus Relays / Logs pane
- `j` / `k`: move down / up in focused pane
- `Ctrl-d` / `Ctrl-u`: page down / page up in focused pane
- `g` then `g`: go to top
- `G`: go to bottom
- `0`: jump to start in focused pane
- `$`: jump to end in focused pane
- `f`: toggle log follow mode
- `Esc`: return to Normal mode

### Modes

- `i`: Insert mode (enter `npub` or `nsec`)
- `/`: Search mode (filter logs)
- `:`: Command mode (Ex-style commands with live hints)

### Search

- In Search mode, type query and press `Enter`
- `n`: next match
- `N`: previous match

## Command Mode (`:`)

- Live hint row shows matching commands while you type
- `:q`, `:quit`, `:qa`, `:qall` - quit
- `:clearlogs`, `:cl` - clear logs
- `:set follow` / `:set nofollow` - logs follow behavior
- `:set nipswrap` / `:set nonipswrap` - wrap selected-row NIPs
- `:set compact` / `:set nocompact` / `:set autocompact` - layout mode
- `:filter <pattern>` - set log filter
- `:filter clear` - clear log filter
- `:copylogs`, `:yanklogs` - copy all logs to clipboard
- `:wlogs <path>` - write logs to file
- `:auth nsec1...` - set NIP-42 signing key for relay AUTH challenges
- `:help`, `:h` - print command help into logs

## Notes

- Discovery may return many relays; connection failures or policy declines on some relays are expected in practice.
- Clipboard copy uses `pbcopy` and is macOS-specific.
- Entering `nsec1...` in Insert mode starts monitoring immediately after deriving the matching `npub`.
- `:auth` keeps the key in process memory only for the current run (not persisted).
- The app uses best-effort secret handling for NIP-42 keys: transient input buffers are cleared after submission and the long-lived private key is stored as binary key bytes instead of repeated hex strings.

## Paid Relay Auth Notes

- `filter.nostr.wine` requires NIP-42 auth (`AUTH` challenge after WebSocket upgrade) and active paid time.
- `nostr.wine` and `cellar.nostr.wine` advertise payment requirements in NIP-11, but not mandatory NIP-42 for basic connection.
- Some paid relays publish meaningful query-parameter variants such as `wss://filter.nostr.wine?global=all`; keep the full relay URL unchanged when sharing or debugging relay lists.
- Relay URLs that include query parameters are preserved during bootstrap and monitoring connects.
- Some relays can reject writes/subscriptions based on account status even when the socket connection succeeds.
- Bearer-token HTTP auth is relay-specific and not a standard Nostr mechanism. This app currently implements NIP-42 challenge signing.

## Project Layout

- `src/main.cpp`: app bootstrap, io context, session wiring
- `src/ui/app.cpp`: TUI rendering + key handling
- `src/net/bootstrap_client.cpp`: relay discovery (NIP-65/NIP-02)
- `src/net/relay_session.cpp`: per-relay websocket session, TLS, NIP-11, reconnect
- `src/net/session_manager.cpp`: multi-relay orchestration
- `src/model_relay_stats.cpp`: shared relay metrics model

## License

Copyright (C) 2026 advorzhak

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for the full text.
