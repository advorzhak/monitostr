---
name: gplv3-compliance
description: "Use when adding new source files or updating existing files to ensure GPL-3.0 copyright headers are present and correct"
argument-hint: "List the files that need GPL-3.0 headers added or updated"
agent: agent
---

# GPL-3.0 Compliance: Copyright Headers

## Goal

Ensure all C++ source and header files (.cpp, .hpp) contain the proper GPL-3.0 copyright notice and license header, as mandated by the project's GPL-3.0 license.

## Scope

- Add or update copyright headers in all .cpp and .hpp files in src/ and include/ directories.
- Update existing files only if they are missing the header.
- Preserve all existing code; add headers without altering functionality.

## Requirements

- Use the exact copyright header format specified below.
- For header files (.hpp), place the copyright header **after** the `#pragma once` line.
- For source files (.cpp), place the copyright header **before** any #include directives.
- Verify the project LICENSE file exists at repository root and references GPL-3.0 v3.
- Run `cmake -S . -B build && cmake --build build -j4` after changes to confirm no build regressions.

## Copyright Header Template

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

## Input

${input}

## Output Format

1. **Files Updated** — List each file with header status: "added", "already present", or "skipped".
2. **Header Placement** — Confirm correct placement (after #pragma once for headers, before includes for sources).
3. **Build Verification** — Report cmake configure, build, and any compiler warnings/errors.
4. **Test Status** — Run `ctest --test-dir build --output-on-failure` and report pass/fail.
5. **Gaps or Notes** — Flag any files that could not be updated and why (e.g., auto-generated, external library).

## Validation

- Confirm at least one file was successfully updated with the header.
- No build errors or warnings should be introduced by header additions.
- All tests should pass after headers are added.
