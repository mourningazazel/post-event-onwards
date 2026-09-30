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
| R4 batch-first APIs | `step_horde` is batch-first; keep it that way |
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
                              ├─ scent.hpp  ScentField: deposit / step / strongest_neighbour
                              ├─ stage.hpp  StageSpec, stage_seed, generate_stage
                              └─ dead.hpp   Dead, step_horde
```

### Scent (`scent.hpp`)

The signature mechanic. A `ScentField` is a double-buffered `Grid<float>`. Each turn the
player (and later blood, bait, fire) `deposit`s; `step` diffuses to the four orthogonal
neighbours and decays. Walls and the map edge absorb what flows into them. `strongest_neighbour` is the whole AI today.

Where it is going (`docs/design/scent-mobs.md`): two layers per channel (a cheap fine trail and
a coarse diffusing cloud that drifts with wind), several channels with a dominance matrix, and
**aggregates** (coarse sums) that the movement rule reads. Constraints that stay: `step` is
O(cells), linear memory, no allocation, and **linear** so ADR-0012's patch is exact.

### Stages (`stage.hpp`)

Today: an endless sequence of random-wall stages from `stage_seed(world, index)`. Target: the
hierarchical generation levels of `docs/design/world-generation.md` (macro → region →
settlement → structure → tile → contents), baseline + Event + aftermath on the global clock
(ADR-0009, ADR-0011), the building census and persona houses (`world-catalog.md` §10). The
`generate_stage` signature is the seam; generators are plug-ins behind it.

### The Dead (`dead.hpp`)

`Dead` is deliberately tiny; thousands must step per turn. `step_horde` is one pass over a
vector. Target: the **weighted draw** of scent-mobs round 3 (scent, aggregate, company,
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
boundary one update runs: `step_linear`, `patch_deposit` per logged tile with
`player_scent × seconds / period`, `clamp_floor`, the Dead (`cooldown_s`, `step_seconds`). A
full period on one tile gives a factor of exactly 1, so 6 s steps replay the pre-clock world bit
for bit (pinned hash test). Speculation is per update: an action that crosses no boundary leaves
it valid, and `commit` re-decides the Dead near any logged tile. `turn()` still counts actions.

### Computing while waiting (PEO-007)

While the player thinks, a worker in `src/app/main.cpp` runs `World::speculate(Speculation&)`:
the next turn assuming a Wait with no deposit, scent left after `ScentField::step_linear` and
the Dead decided on a clamped copy. On input, `World::commit(Speculation&, Action)` applies the
action, adds the player's deposit with `patch_deposit` (the cell and its four open neighbours),
runs `clamp_floor`, and re-decides only the Dead within Chebyshev 2 of the player. A stale
speculation falls back to `step()`. Exactness comes from order, not algebra: `step()` runs the
same `step_linear` → `patch_deposit` → `clamp_floor` sequence, so float rounding matches bit for
bit. `Speculation` buffers are reused, so a turn allocates nothing. Measured on the Builder's
M1 (release, 200x120, 5000 Dead, best of 10): `speculate` 224–542 µs, `commit` 31–65 µs (PEO-040, best of 200: 225 µs and 32 µs). In
game at 80x45 the HUD read `spec:hit` on every turn, including ~46 keys/s.

The frontend accepts at most `kMaxTurnsPerSecond` (3) turn keys a second (D-011); a press or
auto-repeat inside the interval is dropped, never queued, so releasing a key stops at once.
R toggles running (steps of `kRunStepSeconds`, 3 s, D-015); the cap limits key presses, not game
time, so a runner covers two cells per update.

## Boundaries that tests protect

| Boundary | Test |
|----------|------|
| Determinism | `stage: generation is deterministic`, `rng: same seed` |
| Scent physics | `scent: mass is conserved ... while the front is interior`, `walls absorb scent` |
| The Dead | `dead: the dead never enter walls` |
| Scale | `dead: a thousand dead step` |
| Turn model | `world: commit(speculate()) is bit-identical to step()`, `scent: step_linear is linear` |
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
