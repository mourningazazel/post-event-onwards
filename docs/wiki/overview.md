# Overview

## The game

*Post Event: Onwards* is a turn-based survival roguelike. Nearly everyone turned at once into
**the Dead**: walking corpses that cannot see but never stop smelling. The player is one of a
very few who did not. The pillars (`docs/production/vision.md`):

1. **Scent is the game.** The Dead climb a scent field the player cannot help feeding.
2. **Hordes, not encounters.** Thousands of cheap units following one weighted rule; being
   swarmed is the main way to die, so performance is a pillar.
3. **Realism you can act on.** Items are physical things with materials, features and
   quality; skills run 1 to 100 with honest percentages.
4. **Onwards, always.** An endless, persistent world on one clock, days since the Event. Hard
   permadeath; the last character rises as one of the Dead that can see, and the next starts
   nearby.
5. **A mind under pressure.** Mood and Sanity as main mechanics.

Fixed scales: one tile is one metre and one z-level one storey (ADR-0013); a walking step is
3 game seconds (ADR-0016). The world waits for the player (D-002).

## The parts of the program

| Part | Path | What it is |
|---|---|---|
| Simulation | `src/core/` (`peo::core`, static library `peo_core`) | Everything that decides what happens. No SDL, I/O, globals, threads or clocks; deterministic from a seed |
| GPU backend | `src/gpu/` (`peo_gpu`, optional) | The scent update on SDL_GPU compute, bit-identical to the CPU |
| Frontend | `src/app/` (executable `peo`) | SDL3 app: keys to actions, a thread pool, the speculation thread, drawing |
| Tests | `tests/` | Doctest suites (`peo_core_tests`), the siege runner (`peo_siege`), the map imager (`peo_geo`), GPU tests, content expectations |
| Content | `content/` | The game's catalogue in TOML: items, materials, buildings, rooms, loot |
| Tools | `tools/` | `verify.py`, the queue tool, docs validation, git hooks, the content tools |
| Docs | `docs/` | Vision, decisions, ADRs, design corpus, owner's notes, this wiki |
| Agent setup | `AGENTS.md`, `CLAUDE.md`, `.agents/`, `.claude/` | Rules, skills and subagents for the AI developers |
| Work queue | `WORK_QUEUE.json`, `DEFERRED_WORK.json` | Planned work, edited only through `tools/work_queue.py` |

## Systems and their state

| System | State | Page |
|---|---|---|
| World and the turn | Built: `World`, actions, the substep clock, the occupancy log | [Architecture](architecture.md), [Time and turns](time-and-turns.md) |
| Computing ahead | Built: `speculate` / `commit`, bit-identical to a live step | [Time and turns](time-and-turns.md) |
| Scent | Built: one human channel, geodesic, with wind | [Scent](scent.md) |
| The Dead | Built: calm state, slots, weighted draw, one per tile. Designed: triggers, sound, trips, trample, other states | [The Dead](the-dead.md) |
| Stages | Built: random-wall stages in an endless sequence | [World generation](world-generation.md) |
| Geography | Built: terrain, seas, lakes, settlement sites. Not yet used by play. Designed: rivers, towns, buildings, one continuous map | [World generation](world-generation.md) |
| Sight, memory, Look | Built | [Sight and memory](sight-and-memory.md) |
| Saves | Built: the container and the World's sections, in memory. Designed: files, fallbacks, compression | [Persistence](persistence.md) |
| Revisits | Built: attrition curve, fates, thinning, store and resume. Not yet called by play | [Persistence](persistence.md) |
| Content | Data and Python tools built. Designed: the C++ loader and everything that reads it | [Content](content.md) |
| Threads and GPU | Built: lent executor, GPU field backend | [Architecture](architecture.md) |
| Frontend | Built: camera, HUD, input pacing, look-ahead strip, tilesets | [Frontend](frontend.md) |
| Characters, skills, Mood and Sanity, items in play, sound, death and succession, replays | Designed | `docs/design/`, `docs/domains/` |

## Ids used across the project

| Id | Means | Where |
|---|---|---|
| `PEO-123` | A work-queue item | `tools/work_queue.py show PEO-123`; brief in `docs/production/briefs/`, notes in `docs/production/notes/` |
| `D-038` | An owner's decision, with its chosen option ("D-038 B") | `docs/production/decisions.md`, older ones in `decisions-archive/` |
| `ADR-0012` | An accepted architecture decision | `docs/adr/` |
| `N014` | An owner's note, verbatim | `docs/design/notes/` |
| `R8` | One of the fifteen framework rules | `docs/SYSTEMS.md` §1 |

Code comments cite these ids, so the reason for a line is usually one lookup away.
