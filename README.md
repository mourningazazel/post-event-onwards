# Post-Event Onwards

Performance-heavy open-source ASCII roguelike (C++20 / SDL3): scent-driven
hordes and an endless, staged world.

## Build

```sh
python3 tools/bootstrap.py          # once: git hooks, toolchain report
cmake --preset dev                  # or: release · headless · win-dev
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/peo [world-seed]    # arrows/WASD move · Shift+S scent view · N next stage · Esc quit
```

`headless` builds only the simulation and tests (no SDL) and is what CI and
cloud agent sessions use. SDL3 is found from the system or fetched from source.

## Layout

| Path | What |
|------|------|
| `src/core/` | The simulation library. No SDL, deterministic, fully unit-tested. |
| `src/app/` | SDL3 frontend: window, input, drawing. Thin by rule. |
| `tests/` | doctest suite for `core` (< 500 ms). |
| `tools/` | `verify.py` (the gate), `work_queue.py`, `validate_docs.py`, git hooks. |
| `docs/` | Read-by-task index in `docs/README.md`. Direction lives in `docs/production/`. |
| `WORK_QUEUE.json` | The only backlog. Managed with `tools/work_queue.py`. |

## How this repo is developed

AI agents do most day-to-day work under two roles described in
`docs/roles.md`: an **Architect** (Claude Code on the web) that plans, briefs,
tests headlessly and reviews, and a **Builder** (Claude Code on the developer's
machine) that implements and runs the game. `AGENTS.md` holds the shared rules.
