# Post Event: Onwards

A performance-heavy, open-source roguelike of super-realistic survival among the Dead:

- ASCII-first grid presentation, with swappable fonts and tilesets planned
- blind, scent-driven hordes of the Dead
- an endless, staged, persistent world with a global days-since-event clock
- turn-based, with the next turn computed while you think
- Linux and Windows, C++20 and SDL3, MIT licensed

**Status: first playable loop.** A small deterministic core (scent field, staged world, the
Dead) with a thin SDL3 frontend, plus the full design corpus and the authored content data.

## Build and run

```sh
python3 tools/bootstrap.py          # once: git hooks, toolchain report
cmake --preset dev                  # or: release · headless · win-dev
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/peo [world-seed]    # arrows/WASD move · Shift+S scent view · N next stage · Esc quit
python3 tools/verify.py             # the gate: docs, content lint+tests, format, build, tests
```

`headless` builds only the simulation and tests (no SDL); CI and cloud agent sessions use it.
SDL3 is found from the system or fetched from source.

## Layout

| Path | What |
|------|------|
| `src/core/` | The simulation library. No SDL, deterministic, fully unit-tested. |
| `src/app/` | SDL3 frontend: window, input, drawing. Thin by rule. |
| `tests/` | doctest suite for `core` (< 1 s) and content expectations/chains under `tests/content/`. |
| `content/` | All game data: registry, materials, substances, item archetypes, modifiers, world data. Schema in `content/README.md`. |
| `tools/` | `verify.py` (the gate), `work_queue.py`, `validate_docs.py`, `content/lint.py`, `content/test.py`, git hooks. |
| `docs/` | Read-by-task index in `docs/README.md`. Direction in `docs/production/`, decisions in `docs/adr/`, design in `docs/design/`. |
| `WORK_QUEUE.json` | The only backlog. Managed with `tools/work_queue.py`. |

## How this repo is developed

AI agents do most day-to-day work under two roles described in `docs/roles.md`: an
**Architect** (Claude Code on the web) that plans, briefs, tests headlessly and reviews, and a
**Builder** (Claude Code on the developer's machine) that implements and runs the game. The
owner decides gameplay and direction through `docs/production/DECISIONS_NEEDED.md`.
`AGENTS.md` holds the shared rules.

## License

[MIT](LICENSE). Dependency licenses and policy: `docs/LICENSING.md`.
