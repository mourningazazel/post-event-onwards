# AGENTS.md · shared policy

Rules every coding agent follows here. Harness deltas live in `CLAUDE.md`.
`docs/README.md` says where everything else is; load context on demand.

## Two roles, one queue

- **Architect** runs in Claude Code on the web (`CLAUDE_CODE_REMOTE` set).
  Owns direction (`docs/production/`), queue briefs, headless tests, tooling
  and review. Never claims to have run the game.
- **Builder** runs on the developer's machine. Owns implementation, running
  the game, manual test steps and real-hardware perf numbers.
- The contract between them is `docs/roles.md`. Work only moves through
  `WORK_QUEUE.json` via `tools/work_queue.py`; never edit the JSON by hand.
- Precedence when sources disagree: user → `docs/production/decisions.md` →
  topic docs → this file.
- Gameplay, abstraction and direction choices, and big rewrites, are the
  user's: add an entry to `docs/production/DECISIONS_NEEDED.md` (fixed A/B/C
  format), set the item `AwaitingUser`, move on. Technical method is yours.
- The enemies are **the Dead**. The word "zombie" is banned everywhere.

## Build and test

- `python3 tools/bootstrap.py` once per checkout (installs git hooks).
- `python3 tools/verify.py` before every commit (docs caps, clang-format,
  headless build, tests); `--frontend` on a machine with a display.
- Presets: `headless` (core + tests, no SDL), `dev`, `release`, `win-dev`.
- `src/core` is the simulation: no SDL, no I/O, no globals, deterministic
  from a seed. Every mechanic lands here first, with a doctest under `tests/`.
- `src/app` is the SDL3 frontend and stays thin. Logic in it is a bug.
- Unit tests stay headless; the suite runs in under 500 ms. What cannot be
  tested headlessly gets a `manual` step in its brief.
- Performance is a feature: no allocation in per-tick loops, flat arrays,
  measure with `headless-release` before optimising.

## Code

- C++20, warnings are errors, `.clang-format` is law (run `verify.py --fix`).
- No magic numbers: name constants (`kTickMs`), keep tunables in structs.
- Public headers under `src/core/include/peo/core/`; one concept per file.
- Comments say why; names say what.

## Git

- Commit explicit paths. Never `git add -A`, stash, or `--no-verify`.
- Message `[PEO-123] imperative summary`; one queue unit per commit.
- `main` is integration: fast-forward only, no force-push, no squash.
- Read queue state from `origin/main` before choosing work. On a queue
  conflict, take theirs and re-apply your change with the tool.

## Queue lifecycle

Record (`/work`, `/bug`) → Architect briefs (`/plan`) → Builder takes it
(`/start-work`, `InProgress`) → verify → commit + push → report note →
`Validation` → Architect reviews (`/review`) → complete or bounce to `Pending`.
Blocked items get `docs/production/handoffs/<id>.md` (`/handoff`).
Finished items are logged in `docs/COMPLETED_WORK/YYYY-MM-DD.md`.

## Never

- Skip, disable or weaken a test to get green.
- Commit build output, local settings or secrets (hooks block these).
- Promote `DEFERRED_WORK.json` items; only the user does.
- Widen scope beyond the brief; add a queue item.
