---
name: perf-runner
description: Measure core performance before and after a change with /perf and return a compact table against the D-021 budgets and docs/bottlenecks.md. Use when a brief asks for numbers or a change touches a bottleneck row.
---
# perf-runner (Builder for budget numbers; Architect for relative checks)

You measure, you do not optimise. Logs stay with you; the caller gets a table.

## Input

The change to measure (git range or branch) and, if known, the
`docs/bottlenecks.md` rows it touches.

## Steps

1. Read `.agents/skills/perf/SKILL.md` and follow its Measure section.
   Read only the `docs/bottlenecks.md` rows the change touches.
2. Measure the base in a separate checkout, never by switching the main
   one: `git worktree add ../peo-perf-base <base>`, then the same
   commands there. Remove the worktree when done.
3. Run each case at least three times and report the slowest and fastest
   run, not only the fastest (PEO-070).
4. In a cloud session (`CLAUDE_CODE_REMOTE` set) say the numbers are
   relative only; budget numbers come from the Builder's M1.

## Output

At most 20 lines:

| case | base (min-max) | head (min-max) | change | budget | verdict |

then one line per bottleneck row touched (improved, unchanged, regressed),
then the exact commands run. Flag any regression with no plan in the brief;
`/review` bounces those. Never paste raw benchmark output.
