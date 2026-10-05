@AGENTS.md

# CLAUDE.md · Claude Code deltas

Only what differs from `AGENTS.md`. Where they conflict, this file wins.

## Which role am I?

Run `python3 tools/bootstrap.py`. It prints `ARCHITECT (cloud)` when
`CLAUDE_CODE_REMOTE` is set, otherwise `BUILDER (local)`. Then read the
matching section of `docs/roles.md` before touching anything.

- Architect sessions land on a `claude/*` branch the harness assigns; open a
  pull request for the work, drive CI green and merge it yourself. Gameplay,
  abstraction, direction and big-rewrite calls still stop in
  `DECISIONS_NEEDED.md`. Use the `headless` preset only; there is no display.
- Builder sessions commit to `main` directly (fast-forward). Run the game with
  `cmake --build --preset dev && ./build/dev/bin/peo` and follow the brief's
  `manual` steps before reporting.

## Skills

Canonical skill text lives in `.agents/skills/<name>/SKILL.md`. The files in
`.claude/skills/` are wrappers that point there; edit the canonical file.
Available: `/work`, `/bug`, `/plan`, `/start-work`, `/review`, `/handoff`,
`/burndown`, `/status`, `/decisions`, `/cpp-core`, `/perf`.

Subagents follow the same pattern: canonical `.agents/agents/<name>.md`,
wrappers in `.claude/agents/`. Hand one a read-heavy job so its logs stay out
of your context: `core-checker` (diff vs `/cpp-core`), `perf-runner`,
`sim-debugger`, `docs-drift`. Small tasks are cheaper done directly.

## Working style

- Prefer editing files with the Edit tool over shell heredocs; keep shell
  edits few and reviewable.
- Reference code as `path:line`.
- When a task is unclear, add a `research` queue item or a decision proposal
  in `docs/production/decisions.md` rather than guessing silently.
- Keep replies short; the queue item's `notes` are the record, not chat.
