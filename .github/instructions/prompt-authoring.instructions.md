---
applyTo: ".github/prompts/*.prompt.md"
description: "Use when creating or updating workspace prompt files; enforce consistent structure, clear outputs, and safe scope"
---

# Prompt Authoring Standards

These instructions apply to all prompt files in .github/prompts.

## Frontmatter

- Require these fields: name, description, argument-hint, agent.
- Start description with "Use when ..." and include concrete trigger phrases.
- Keep argument-hint short and input-focused.
- Use agent: "agent" unless a dedicated custom agent exists.

## Body Structure

Use this order:

1. Goal
2. Scope or Primary target (if applicable)
3. Requirements
4. Optional focus/style/checklist sections
5. Input
6. Output format

## Content Rules

- Keep prompts task-scoped; avoid broad, generic instructions.
- Preserve project architecture boundaries.
- Prefer minimal-risk changes and focused edits.
- Require validation steps when code changes are expected.
- Request file references in summaries for traceability.

## Output Contract

- Always include an explicit Output format section.
- Use numbered lists for expected response sections.
- Include residual risks, gaps, or missing evidence when relevant.

## Editing Discipline

- Do not duplicate rules already defined in workspace instructions unless needed for prompt clarity.
- Keep wording concise and consistent across prompt files.
- Avoid changing prompt intent when refactoring for style or structure.

## Examples

### Minimal Good Prompt

```md
---
description: "Use when adding deterministic tests for a recent C++ change in monitostr"
name: "Add Deterministic Tests"
argument-hint: "Describe changed files and behaviors to verify"
agent: "agent"
---

Goal:
Add deterministic tests for the described code changes.

Requirements:

- Prefer Catch2 tests under tests/.
- Avoid network-dependent assertions.
- Run configure/build/CTest and report failures with file references.

Input:
${input}

Output format:

1. Files added or updated
2. Scenarios covered
3. Build and CTest results
4. Remaining risks or gaps
```

### Anti-Pattern to Avoid

```md
Do testing stuff for this repo.
```

Why this is weak:

- Missing frontmatter fields, so discoverability is poor.
- No explicit scope, validation, or output contract.
- Ambiguous wording leads to inconsistent results.
