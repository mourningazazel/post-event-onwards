# 15 · Roadmap

What is designed but not built, grouped by system, from `WORK_QUEUE.json` on 2026-10-12
(`python3 tools/work_queue.py list` for the live view). Severity S1 is most urgent, S4 least;
effort S to XL. "Design" and "research" items produce a decision or a brief before code.

`docs/production/vision.md` "Near-term direction" gives the owner's order: the playable loop
(done), the Dead's full movement rule with swarm scenarios, content loaded into the game,
buildings in world generation, skills with Mood and Sanity, then replays and the headless
playtest harness.

## The Dead and the senses

| Item | What | |
|---|---|---|
| PEO-010 | Triggers, sound events and speeds | design, L |
| PEO-011 | Trips, contests and trample | design, M |
| PEO-022 | Shared tile occupancy with capacity, trip, knock-out and trample | design, M |
| PEO-084 | Crowd pressure field: barricades fail from crowd depth (after PEO-022) | design, M |
| PEO-102 | Whether one of the Dead may step onto the player's tile (D-043) | design, S |
| PEO-003 | Blood scent layer on contact with the Dead | feature, M |
| PEO-038 | Soak: a capped per-tile scent memory charged by contact (D-013) | feature, M |
| PEO-039 | Rot: section counts of the Dead mask human scent (D-012) | feature, M |
| PEO-045 | Ambient sound and the noise floor (D-018) | feature, M |
| PEO-047 | Scent only near the player: reach radius, sealed buildings skipped (D-019) | feature, L |
| PEO-049 | Z-level scent routes near the player | feature, M |
| PEO-051 | A 6x6 section aggregate layer for rot, reach and sealed buildings | design, M |
| PEO-018 | Ambient crow text | feature, S |

## World and map

| Item | What | |
|---|---|---|
| PEO-118 | One continuous map: no stage jumps (ADR-0007, D-041) | design, XL |
| PEO-111 | River courses on the geography square | design, L |
| PEO-112 | Settlement sites know their rivers | design, M |
| PEO-114 | Settlement layout generator: outline, streets, blocks, lots | design, XL |
| PEO-115 | Town layouts fitted to rivers and shores | design, L |
| PEO-014 | Building census, category retail and persona houses | research, L |
| PEO-113 | Shore and river buildings, rooms and items | research, M |
| PEO-064 | Room slot that places one item from a list (a garage's vehicle) | feature, S |
| PEO-005 | Cave stage generator | feature, L |
| PEO-017 | Maps as rare knowledge items (after PEO-005) | feature, M |
| PEO-083 | Big stages: 1024 to 2048 square with far-reaching scent | research, M |

## Items and content

| Item | What | |
|---|---|---|
| PEO-008 | Content loader: registry, materials, items, modifiers into runtime tables | feature, L |
| PEO-013 | Quality on every item: degradation curves and broken text | design, M |
| PEO-050 | Items as seeds: instantiate on change, resolve on access (D-023) | design, L |
| PEO-053 | Contents generator in C++ (D-023, D-026, D-027) | design, L |
| PEO-019 | Lock kinds, tier bands and security-weighted placement | design, M |
| PEO-020 | Generated books by library section | design, L |
| PEO-015 | Drugs, paraphernalia and pharmacy stock | research, M |
| PEO-116 | Generic parent archetypes, a junk archetype, remap records (D-046) | chore, M |
| PEO-092 | Content migration on load (ADR-0017), briefed for the Builder | feature, M |

## Characters

| Item | What | |
|---|---|---|
| PEO-012 | Skills 1-100: success percentage, failure outcomes, tool gates | design, M |
| PEO-016 | Mood and Sanity data model | design, M |
| PEO-046 | Random character generation: background, physique, job, where you were | design, L |
| PEO-021 | Death: the player rises as a Dead that can see; respawn 1-2 miles away | design, M |

## Time, saves and revisits

| Item | What | |
|---|---|---|
| PEO-082 | Fast time passing: sleep, wait and search at full detail | design, L |
| PEO-107 | Save fallbacks: two good copies and a crash flag | feature, M |
| PEO-108 | Save sections zstd-compressed | feature, M |
| PEO-109 | Player-made things age on revisit | design, M |
| PEO-110 | Revisit catch-up as a streaming job | design, M |
| PEO-006 | Replay: seed plus input log reproduces a run headlessly | feature, L |

## Presentation

| Item | What | |
|---|---|---|
| PEO-121 | What can be made out at range: labels within 20 ft (D-049), briefed for the Builder | design, M |

## Performance

| Item | What | |
|---|---|---|
| PEO-122 | Unit-tier test time headroom on g++-13 (S1) | perf, S |
| PEO-089 | Desire field rebuilds only cells that can change (bottleneck B1) | perf, M |
| PEO-100 | Direction bytes have no reader: measure, then drop or compute on demand | perf, S |
| PEO-074 | Perf levers: swap the speculated horde, copy only dirty cells, static emitter layer | research, M |
| PEO-070 | `peo_bench`: per-axis sweeps and steady-state sampling | perf, M |
| PEO-071 | Run `peo_bench` on the M1 Air | perf, S |
| PEO-072 | Run `peo_bench` on one real PC per tier (yours to do) | perf, S |
| PEO-073 | Auto-config: calibrate horde and active area per machine (D-034) | design, L |

## Tooling

| Item | What | |
|---|---|---|
| PEO-059 | Tests fail to build under clang 22 / libstdc++ 16 | bug, S |
| PEO-068 | Run clang-tidy in `verify --full` and CI | chore, M |
| PEO-069 | Lint and type-check `tools/` with ruff and mypy | chore, S |
| PEO-106 | Warn before `WORK_QUEUE.json` hits its word cap | chore, S |

## Deferred

PEO-098, the engine split and a scripting API (N023, `docs/domains/engine-api.md`). Only you
can promote it.
