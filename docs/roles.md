# Roles · Architect and Builder

Two Claude Code sessions share this repo. They never do each other's job, and
everything they hand each other goes through the queue so either can pick up
after a context reset.

## Architect · Claude Code on the web

Detect: `CLAUDE_CODE_REMOTE` is set; no display; branch `claude/*`.

Owns:
- **Direction.** `docs/production/vision.md` (pillars), `docs/architecture.md`
  (systems and boundaries), `docs/production/decisions.md` (why).
- **Abstraction.** Public interfaces in `src/core/include/peo/core/`. When the
  Builder needs a new seam, the Architect designs the header and the tests
  that pin its behaviour, then briefs the implementation.
- **Briefs.** Every queue item the Builder picks up has a full `brief` (see
  below). Units are sized so one commit finishes one unit.
- **Headless tests and tooling.** Writes doctests that encode acceptance
  criteria before the Builder starts; maintains `tools/`, CI, presets.
- **Review.** For each `Validation` item: read the diff on `origin/main` for
  commits tagged `[PEO-xxx]`, run `tools/verify.py`, check the brief's
  acceptance list and the vision/architecture docs. Then either `complete`
  the item or set it back to `Pending` with a note that says exactly what to
  change. Reviews are about direction and correctness, not style; the
  formatter owns style.

Never: claims to have run the game, tunes feel without Builder numbers,
edits the queue by hand, pushes to `main` directly.

## Builder · Claude Code on the developer's machine

Detect: `CLAUDE_CODE_REMOTE` unset; has a display and the real toolchain.

Owns:
- **Implementation** of briefed units, in `src/core` first, `src/app` last.
- **Running the game.** Executes every `manual` step in the brief, records
  what happened (frame time, feel, bugs seen) in the item's notes.
- **Perf numbers.** `headless-release` benchmarks and in-game frame times on
  real hardware. Numbers go into the report note.
- **Bugs found while working.** Recorded with `/bug`, not fixed on the side
  unless they block the unit.

Loop (`/start-work`): `git fetch origin main` → `work_queue.py next --owner
builder` → `set InProgress` → one unit → `verify.py --frontend` → commit
explicit paths `[PEO-xxx] …` → push → repeat units → report note → `set
Validation --owner architect`.

Never: skips `manual` steps, reinterprets the brief (ask via a note and
`AwaitingUser`), takes an item that is not `Pending` with `owner: builder`.

## User

Sets intent, promotes deferred items, answers `AwaitingUser`, merges
Architect branches into `main`, and is the only one who may relax a rule.

## Handoff formats

Both are stored on the queue item, so they survive session resets.

**Brief** (Architect → Builder), `brief` object on the item:

| Field | Content |
|-------|---------|
| `goal` | One sentence: what is true when this is done. |
| `context` | Files and doc anchors to read first (`src/core/include/peo/core/scent.hpp`, `docs/architecture.md#scent`). |
| `units` | Ordered, commit-sized steps. Each starts with the layer it touches. |
| `acceptance` | Observable criteria. Prefer "test X passes" and "in game, Y happens". |
| `tests` | Headless tests that must pass (written by Architect, may be red until the unit lands). |
| `manual` | Exact in-game steps for the Builder and what to look for. |
| `out_of_scope` | What not to touch, and where it went instead. |

**Report** (Builder → Architect), a note with `--by builder` when moving to
`Validation`:

```
commits: <sha> <sha>
tests: verify.py --frontend passed on <os>/<compiler>
manual: <step> → <what happened>; <step> → <what happened>
perf: <numbers, if the brief asked>
open: <questions or deviations from the brief, or "none">
```

**Verdict** (Architect → Builder), a note with `--by architect`: `complete`
with commit shas, or `Pending` with a numbered list of concrete changes.

## Branching

- `main` is the integration branch and holds the queue of record.
- Builder pushes fast-forward commits to `main`.
- Architect works on its harness-assigned `claude/*` branch; the user merges
  it (or opens a PR) when asked. Architect queue edits are small, so merge
  conflicts are resolved by taking `origin/main` and re-running the same
  `work_queue.py` command.
- Subagents and experiments use `git worktree`, never the main checkout.
