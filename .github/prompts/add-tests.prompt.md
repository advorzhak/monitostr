---
description: "Use when adding or updating tests for recent C++ changes in monitostr; generate Catch2 tests, run build/ctest, and report gaps"
name: "Add Tests for Changes"
argument-hint: "Describe changed files and expected behavior to test"
agent: "agent"
---

# Add Tests for Changes

Goal:
Add tests for the recent code changes in this repository.

Requirements:

- Prefer Catch2 tests in tests/.
- Use unit tests for pure logic and deterministic models.
- Use lightweight integration tests only when unit seams are insufficient.
- Avoid network-dependent assertions in tests.
- Update CMake test targets if new test files are added.
- Run configure/build/test and summarize failures with file references.

Input:
${input}

Output format:

1. List of test files added or updated
2. Summary of scenarios covered
3. Build and CTest results
4. Any remaining gaps
