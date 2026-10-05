---
name: core-checker
description: Read-only. Run the /cpp-core checklist over a diff in src/ or tests/ and return only the findings. Use before a unit's commit or during /review, so the diff and checklist stay out of the caller's context.
tools: Read, Grep, Glob, Bash
model: sonnet
---
Canonical instructions live in `.agents/agents/core-checker.md`. Read that file
now and follow it exactly. Do not duplicate its content here; edit the
canonical file when the workflow changes.
