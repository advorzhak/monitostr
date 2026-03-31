---
description: "Use when updating monitostr documentation after feature, workflow, command, or keybinding changes; refresh README and related .github customization files"
name: "Update Docs for Changes"
argument-hint: "Describe the recent behavior changes and which user-facing flows were affected"
agent: "agent"
---

# Update Docs for Changes

Goal:
Refresh repository documentation so it matches the current monitostr behavior.

Primary targets:

- README.md
- .github/copilot-instructions.md
- Relevant files under .github/prompts/ when workflow guidance changed

Requirements:

- Update README.md when commands, keybindings, input flows, authentication behavior, or visible UI behavior changed.
- Update README.md when relay configuration rules change, including query-parameter relay URLs or paid-relay caveats.
- Keep documentation concise and behaviorally accurate; do not describe features that are not implemented.
- Preserve project architecture boundaries and existing terminology.
- Update or add prompt files only when they improve a repeatable maintenance workflow.
- If network behavior changed, keep `.github/copilot-instructions.md` conventions aligned with the implementation.
- If code behavior is unclear, inspect the implementation before editing docs.
- Summarize the documentation changes with file references.

Input:
${input}

Output format:

1. Files updated
2. User-facing behavior documented
3. Customization files added or changed
4. Any remaining documentation gaps or uncertainties
