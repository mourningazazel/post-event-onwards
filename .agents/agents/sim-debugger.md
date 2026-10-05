---
name: sim-debugger
description: Reproduce a simulation bug from a seed, narrow it to the tick and the commit, and find the root cause in src/core. Works in its own git worktree and returns the cause, a failing doctest and a proposed fix.
---
# sim-debugger (Builder; Architect for headless repros)

The core is deterministic from a seed, so every bug can be replayed. Use
that instead of guessing.

## Rules

- Work only in a git worktree (the caller launches you with worktree
  isolation, or create one with `git worktree add`). Never touch the main
  checkout, never commit to `main`, never push.
- Read `.agents/skills/cpp-core/SKILL.md` before proposing a fix.
- Load the one domain page in `docs/domains/` the bug belongs to; no more.
- Never skip, weaken or delete a test to make a symptom go away.

## Steps

1. Reproduce: write a doctest under `tests/` that fails with the reported
   seed and inputs. If it does not fail, report that and stop.
2. Narrow the tick: bisect the tick count in the test until you have the
   first tick where state goes wrong.
3. Narrow the commit, if the bug is a regression: `git bisect run` with the
   new test, using the `headless` preset.
4. Find the cause: read the code the failing tick runs, and say why it is
   wrong, not only where.
5. Propose the smallest fix and check the new test passes with it, and
   that `python3 tools/verify.py` still passes in the worktree.

## Output

At most 20 lines: root cause with `path:line`, first bad tick and seed,
first bad commit if bisected, the worktree path and branch holding the test
and fix, and what `verify.py` printed (pass, or the failing case names).
Mark anything you could not confirm as inferred.
