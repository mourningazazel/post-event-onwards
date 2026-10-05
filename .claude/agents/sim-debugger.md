---
name: sim-debugger
description: Reproduce a simulation bug from a seed, narrow it to the tick and the commit, and find the root cause in src/core. Works in its own git worktree and returns the cause, a failing doctest and a proposed fix.
tools: Read, Write, Edit, Grep, Glob, Bash
model: inherit
---
Canonical instructions live in `.agents/agents/sim-debugger.md`. Read that file
now and follow it exactly. Do not duplicate its content here; edit the
canonical file when the workflow changes.
