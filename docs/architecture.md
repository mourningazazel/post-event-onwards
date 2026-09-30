# Architecture

One rule above all: **the simulation never knows about SDL.** `peo::core` is a static library
with no I/O, no globals and no clocks; given a seed and a list of actions it produces the same
world on every platform. The frontend feeds it actions and draws its state.

## Target system map

`docs/SYSTEMS.md` holds the target framework: the fifteen engineering rules R1–R15 and the
layered system map (Foundation → Engine → World → Simulation → Game, plus Tools). It was
written before any code and is the destination. This repo grows toward it **one system at a
time through the work queue**, never by a framework-first rewrite. Rules that already hold in
the current code are marked below; the rest apply as each system lands.

| Rule | Status now |
|---|---|
| R8 determinism (seeded streams, no wall clock, no `std` distributions) | holds (`rng.hpp`, `stage.hpp`) |
| R11 every system runs headless | holds (`headless` preset) |
| R12 no singals, explicit context | holds |
| R6 content is data (registry, TOML) | data exists in `content/`; loading is queued |
| R1/R2 one CMake target per system, acyclic | one `core` target today; split as systems appear |
| R3 commands and events | `Action` is the first command (PEO-002) |
| R4 batch-first APIs | the Dead's poll and slot buckets are one pass over the horde; keep it that way |
| R9/R10 budgets, metrics, profiling | not yet |
| R15 playable headless with a rule checker | designed in `docs/design/playtest-harness.md`; queued |

The stack decisions in `docs/GAMEPLAN.md` §2 (SDL_GPU grid renderer, ECS, jobs, noise,
profiling) are targets; the prototype uses `SDL_Renderer` debug text so the loop is playable
today. The renderer reads only a committed world snapshot, so swapping it later is contained.

## Current code

```
 src/app (SDL3)  ──Action──▶  peo::core  ──state──▶  src/app draws
                              │
                              ├─ rng.hpp    xoshiro256**, seeded, no std::rand
                              ├─ grid.hpp   dense row-major Grid<T>; Grid<bool> is bytes
                              ├─ scent_wave.hpp  ScentWave: deposit / update / patch_deposit / strongest_neighbour
                              ├─ stage.hpp  StageSpec, stage_seed, generate_stage
                              └─ dead.hpp   Dead, plan_slots, decide_move
```

### Scent (`scent_wave.hpp`)

The signature mechanic, and D-024's transport: an integer **geodesic field**. A cell holds the
best `strength - distance_cost x route - age_cost x age` over the deposits that reached it, the
route 8-connected along open tiles and never past a wall corner; three numbers set it
(`WaveParams`: strength, distance and age costs of 8, a 60-cell reach standing still). Values
are stored with the age folded in, so ageing is a moving line, not a rewrite, and each update
only the cells that changed carry their value on, `speed` cells a round: the work is bounded by
reach, not the map. A round offers the values its cells had when it started, so its result is a
max over offers and independent of order; that makes the **min-plus patch** exact:
`speculate()` runs the update with no deposit and `commit()` adds the player's with
`patch_deposit`. `strongest_neighbour` keeps ScentField's contract and is still the whole AI.
`ScentField` (diffusion) stays in the tree, unused by World, until PEO-030 is accepted.

Where it is going (`docs/design/scent-mobs.md`): wind outdoors (PEO-048), z-level routes
(PEO-049), a reach radius round the player with sealed buildings skipped (PEO-047), rot and
soak on their own values (PEO-039, PEO-038), several channels with a dominance matrix, and
aggregates the movement rule reads.

### Stages (`stage.hpp`)

Today: an endless sequence of random-wall stages from `stage_seed(world, index)`. Target: the
hierarchical generation levels of `docs/design/world-generation.md` (macro → region →
settlement → structure → tile → contents), baseline + Event + aftermath on the global clock
(ADR-0009, ADR-0011), the building census and persona houses (`world-catalog.md` §10). The
`generate_stage` signature is the seam; generators are plug-ins behind it.

### The Dead (`dead.hpp`)

`Dead` is deliberately tiny; thousands must step per turn. They move in staggered slots
(D-031): every `dead_cycle` (9 s) the world polls the horde, and each unit with a stronger
neighbour gets `plan_slots`: 9 / `step_seconds` slots, the whole part always and the fraction a
hashed chance, evenly spaced from a hashed offset. Hashes of (stage seed, cycle, index), never a
shared stream, so who moves when changes each cycle. At its slot a unit `decide_move`s: its
strongest neighbour, only if no Dead stands there and no pending move reserved it; a calm Dead
never sidesteps or climbs over another, it stays put. The move lands one second later; in each
second the slot's units decide before last second's moves land, so a vacated tile is free only
from the next second and a crowd files through gaps, jamming a one-wide corridor. Target: the **weighted draw** of scent-mobs round 3 (scent, aggregate, company,
attractor, stimulus, repellent, footing terms), triggers by general direction, sound events,
trips and trample, population aggregates beyond the detailed radius. Struct-of-arrays when the
profiler asks.

## Turn model (ADR-0012)

Time moves only through `World::step(Action)`. Order within a turn: apply the player's action;
scent step; the Dead step (moves, contests, trample); sound events resolve. Core spawns no
threads; the frontend owns the worker and draws only the committed world.

### The clock (PEO-040, D-015)

The world counts game seconds (`World::seconds()`). Each `Action` carries a duration (`kStepSeconds`
and `kWaitSeconds`, both 6). Scent and the Dead update on their own cadence, once every
`WorldParams::update_period` (6 s), whatever the player does. An action applies at once, then
its seconds are logged on the player's tile (`occupancy()`, merged per tile). At each period
boundary one scent update runs: each logged tile deposits `strength - age_cost × (period -
seconds) / period` (a full period exactly `strength`, a runner's tiles a little less, never more),
then the wave updates. The Dead run every second, on their own 9 s cycle (above). Speculation is per update:
an action that crosses no boundary leaves it valid. `turn()` still counts actions.

### Computing while waiting (PEO-007)

While the player thinks, a worker in `src/app/main.cpp` runs `World::speculate(Speculation&)`:
the next update of the scent wave with no deposit, and the Dead's seconds up to
and including that boundary, recorded per second with any poll (PEO-060). Between updates the
Dead read only the last update's scent, never the player, so those seconds are fixed once an
update commits. On input, `World::commit(Speculation&, Action)` applies the action and runs its
seconds, replaying the recorded ones; at the boundary it adds the logged deposits with
`ScentWave::patch_deposit` (the min-plus patch, exact for one round per update; a faster wave
commits live). A stale speculation, or a second outside the record, runs live, exactly as
`step()`. The field is integer, so equal means equal. `Speculation` buffers are reused, so a
turn allocates nothing. Measured on the Builder's M1 (release, one 6 s step, 300 samples,
PEO-030): 200x120 with 5000 Dead, `commit` 0–1 µs, `speculate` 28–277 µs; 512x512 with 50,000,
`commit` 0–3 µs, `speculate` 0.1–1.5 ms.

Turn keys go through `TurnInput` (`turn_input.hpp`, D-032, amending D-011): a distinct press
waits in a queue of up to 3 and the queue plays out no faster than 3 turns a second; a held key's
auto-repeat is taken only when the cap allows and nothing waits, so it never builds lag and stops
the moment the key is released. While taps wait, one SDL timer wakes the loop at the next due time;
with none waiting nothing wakes it. Depth and rate are tunables in `TurnInputParams`.
R toggles running (steps of `kRunStepSeconds`, 3 s, D-015); the cap limits key presses, not game
time, so a runner covers two cells per update.

## Boundaries that tests protect

| Boundary | Test |
|----------|------|
| Determinism | `stage: generation is deterministic`, `rng: same seed` |
| Scent physics | `scent: mass is conserved ... while the front is interior`, `walls absorb scent` |
| The Dead | `dead: the dead never enter walls` |
| Scale | `dead: a thousand dead step` |
| Turn model | `world: commit(speculate()) is bit-identical to step()`, `scent_wave: patch_deposit after update equals deposit then update` |
| Content | `tools/content/lint.py`, `tools/content/test.py` (374 expectations, 109 chains) |
| Swarm behaviour | the scenario table in scent-mobs round 3 (harness, queued) |

## Planned seams (not built)

- `World`: **built (PEO-002)** in `peo/core/world.hpp` + `src/core/src/world.cpp`, with
  `peo/core/action.hpp`. Owns stage, scent, horde, player; `step(Action)` is the single
  entry point; `speculate` / `commit` built (PEO-007).
- `Content`: loads `content/` (registry, materials, items, modifiers) into runtime tables;
  the derivation rules in `tools/content/derive.py` are the executable spec to port.
- `Replay`: seed plus action log reproduces a run headlessly; the Architect's tool for
  reviewing bugs it cannot see. First piece of the playtest harness.
- `Fields`: the generic scalar-field service (scent channels, later sound, heat, light).
- `Population`: region-cell counts and attraction; spawn-in and merge-out at the edge.
