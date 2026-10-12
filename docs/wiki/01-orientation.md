# 1 · Orientation

## The game, as designed

`docs/production/vision.md` is the source; the short version:

- **Scent is the game.** The Dead cannot see; they climb a scent field the player feeds by
  existing. Hiding is managing what you leave behind.
- **Hordes, not encounters.** Thousands of cheap units following one weighted rule. Being
  swarmed is the main way to die, so performance is a pillar, not a polish task.
- **Realism you can act on.** Items are physical things (materials, features, quality);
  skills run 1 to 100 with honest percentages.
- **Onwards, always.** An endless, persistent world on one clock (days since the Event).
  Hard permadeath; your last character rises as one of the Dead and the next one starts
  nearby.
- **A mind under pressure.** Mood and Sanity as main mechanics.

Scale is fixed by ADR-0013: one tile is one metre, one z-level is one storey. Time is fixed by
ADR-0016: a walking step is 3 game seconds. The word "zombie" is banned everywhere (code,
docs, UI, commits), except inside the owner's verbatim notes.

## What exists today

The playable build is a **prototype loop**, not the game yet:

| Built | Where |
|---|---|
| A deterministic `World`: player, stage, scent, the Dead, a game clock | `src/core/include/peo/core/world.hpp` |
| The geodesic scent field, with wind, threads and an optional GPU backend | `scent_wave.hpp`, `wind.hpp`, `src/gpu/` |
| The Dead's weighted-draw movement in staggered slots | `dead.hpp`, `desire.hpp`, `world.cpp` |
| Computing the next update while the player thinks, committed exactly on input | `World::speculate` / `World::commit` |
| Random-wall stages in an endless sequence | `stage.hpp` |
| The real map's bottom layer: elevation, seas, forest, lakes, settlement sites (not used by play yet) | `geography.hpp`, `settlement_site.hpp`, `noise.hpp` |
| Sight (shadowcasting), map memory, a Look cursor | `sight.hpp`, `memory.hpp`, `look.hpp` |
| A save container and full world save/restore (in memory; no files yet) | `save.hpp`, `world_save.cpp` |
| Revisit maths: an attrition curve that thins a stored horde | `aftermath.hpp`, `revisit.hpp` |
| SDL3 frontend: centred camera, HUD, input pacing, look-ahead strip, DF tilesets | `src/app/` |
| A large content catalogue in TOML, checked by Python tools (not loaded by C++ yet) | `content/`, `tools/content/` |
| Headless tests, the siege suite, a map image tool, CI | `tests/`, `.github/workflows/` |

Not built yet, but designed: buildings and towns on the map, items in the game, characters
and skills, sound, triggers, trips and trample, rot and soak, death and succession, file
saves, replays. [Roadmap](15-roadmap.md) maps each to its queue items.

## The repo, top to bottom

```
AGENTS.md, CLAUDE.md     rules every coding agent follows (short, word-capped)
WORK_QUEUE.json          the work queue; edit only through tools/work_queue.py
DEFERRED_WORK.json       items parked until the owner promotes them
CMakeLists.txt, CMakePresets.json, cmake/   the build
src/core/                peo::core, the simulation (static library, no SDL)
  include/peo/core/      public headers: one concept per file
  src/                   implementation; wave_kernels* are the scent row kernels
src/gpu/                 peo_gpu: the scent update on SDL_GPU compute (optional)
src/app/                 peo, the SDL3 frontend: main.cpp plus SDL-free helpers
tests/core/              doctest suites, one per header (unit and scenario tiers)
tests/siege/             peo_siege: hordes round three buildings for 48 game hours
tests/geo/               peo_geo: draws geography squares as images
tests/gpu/               GPU backend tests (bit-identical to the CPU)
tests/content/           expectations, action chains and pinned rolls for content
content/                 game data in TOML (items, materials, buildings, loot, ...)
tools/                   verify, queue, docs validation, git hooks, content tools
docs/                    everything written; docs/README.md indexes it
.agents/skills/, .agents/agents/   canonical skill and subagent text
.claude/skills/, .claude/agents/   thin wrappers that point at .agents/
third_party/             vendored doctest
```

## The code in numbers

About 16,600 lines of C++ and GLSL (`wc -l` on 2026-10-12): core headers and sources about
6,500, the frontend about 1,500, the GPU backend about 500, tests about 8,100. The biggest
files are `src/app/main.cpp` (1,011), `src/core/src/scent_wave.cpp` (838),
`src/core/src/world.cpp` (647), `src/core/src/world_save.cpp` (568) and
`tests/core/test_world.cpp` (1,214).

## Ids you will see everywhere

| Id | Means | Where |
|---|---|---|
| `PEO-123` | A work-queue item | `tools/work_queue.py show PEO-123`; brief in `docs/production/briefs/`, notes in `docs/production/notes/` |
| `D-038` | A decision the owner made (lettered options, e.g. "D-038 B") | `docs/production/decisions.md`, older ones in `decisions-archive/` |
| `ADR-0012` | An accepted architecture decision | `docs/adr/` |
| `N014` | An owner's note, verbatim | `docs/design/notes/` |
| `R8` | One of the 15 framework rules | `docs/SYSTEMS.md` §1 |

Code comments cite these ids constantly ("(D-031, ADR-0016)"), so a comment is usually one
lookup away from its reason.
