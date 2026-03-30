---
description: "Use when applying visual polish to monitostr TUI in NeoVim style while preserving behavior and keybindings"
name: "NeoVim TUI Polish"
argument-hint: "Describe visual changes you want in the relay/log panes and statusline"
agent: "agent"
---

# NeoVim TUI Polish

Goal:
Apply a focused visual polish pass to this repository's terminal UI.

Primary target:

- src/ui/app.cpp

Requirements:

- Keep behavior and keybindings unchanged unless explicitly requested.
- Keep panel block sizes stable; prefer viewport scrolling over resizing blocks.
- Keep statusline always visible at the bottom.
- Maintain readability in both compact and wide layouts.
- Preserve asynchronous networking architecture; no net logic in UI layer.
- Make small, focused edits and avoid unrelated refactors.

Style goals:

- NeoVim-like hierarchy and contrast
- Clear active pane indication
- Mode-emphasized statusline
- Muted base palette with selective accent colors

Execution checklist:

1. Inspect current UI rendering and mode/status sections.
2. Propose concise visual deltas.
3. Implement minimal code changes in src/ui/app.cpp.
4. Build with CMake and report compile/test outcomes.
5. Summarize changed sections with file references.

User request:
${input}

Output format:

1. Visual deltas applied (concise)
2. Files/sections changed
3. Build and test outcomes
4. Any intentional trade-offs
