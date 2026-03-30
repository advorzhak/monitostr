---
description: "Use when diagnosing monitostr relay connectivity failures from logs; classify root cause and propose low-risk fixes"
name: "Diagnose Relay Connectivity"
argument-hint: "Paste logs and describe observed failure"
agent: "agent"
---

# Diagnose Relay Connectivity

Goal:
Analyze the provided monitostr logs and diagnose relay connectivity issues.

Focus on:

- TLS/SNI/hostname verification failures
- WebSocket handshake declines vs transport failures
- Recoverable vs non-recoverable errors
- Reconnect behavior and backoff quality

Instructions:

- Identify likely root causes with confidence level.
- Separate expected relay policy declines from true defects.
- Propose concrete code changes with minimal risk.
- Suggest specific tests to validate the fix.
- If logs are insufficient, state exactly which missing log lines are needed.

Input:
${input}

Output format:

1. Most likely root cause(s) with confidence level
2. Classification: relay policy decline vs product defect
3. Suggested code-level fix(es), ordered by risk
4. Test plan to validate and prevent regressions
5. Missing evidence needed (if diagnosis is inconclusive)
