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
The window shows a player-centred view (`src/app/camera.hpp`, PEO-117) over a stage larger than it.

## Current code

```
 src/app (SDL3)  ──Action──▶  peo::core  ──state──▶  src/app draws
                              │
                              ├─ rng.hpp    xoshiro256**, seeded, no std::rand
                              ├─ grid.hpp   dense row-major Grid<T>; Grid<bool> is bytes
                              ├─ scent_wave.hpp  ScentWave: deposit / update / patch_deposit / strongest_neighbour
                              ├─ stage.hpp  StageSpec, stage_seed, generate_stage
                              ├─ noise.hpp  integer value noise, fbm, ridged (Q16)
                              ├─ geography.hpp  terrain_at, generate_geography / generate_square
                              ├─ settlement_site.hpp  place_sites: one scored site per 12.5 km cell
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
`patch_deposit`. `strongest_neighbour` (climb to the strongest neighbour, no corner cutting) is
still the whole AI; since PEO-079 the Dead read it as one stored direction byte per cell
(`flow_target`), rewritten with each update. ScentWave is the only field: the diffusion field it
replaced (`ScentField`) was removed in PEO-076.
Since PEO-078 a round is a **pull** between two buffers over active 128x8 tiles (those that
changed last round and their neighbours); a tile nobody pulls holds the same values in both, and
the branch-free row kernel vectorises. Since PEO-085 the row kernels are one source built twice,
plain (NEON on arm64, SSE2 on x86-64) and for AVX2 on x86-64 with every compiler, and a CPU check
picks once per process (`src/core/src/wave_kernels.hpp`). The World's and the Speculation's waves
are partners that swap in `finish_from`, so a warm `speculate` syncs only the tiles either changed
instead of copying the field; tokens are unique per process, so a Speculation moved to another
World cold-copies. Since PEO-048 a stage's wind (`peo/core/wind.hpp`) and openness make step costs
directional and add `gust` downwind rounds; a calm wave runs the windless kernel unchanged.

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

### Geography (ADR-0018, PEO-095)

The map under everything, generated before anything inside it: 100 m cells in 100 km squares
(`kSquareCells` 1000), from the world seed alone. `terrain_at(seed, params, x, y)` is the source
of truth for a cell's elevation and forest: integer value noise (`noise.hpp`, Q16) for
continents and seas, ridged ranges inside a low-frequency band mask, hills, and forest on land
below the treeline; sea is every cell below sea level. `generate_geography` fills any rect cell
for cell equal to `terrain_at` (it evaluates by row for speed), so squares join without seams in
any order (D-041). Lakes come from sites on a jittered 6.4 km grid, each disc kept inside its
grid square so no two overlap; a lake fills the hollow round its site's lowest cell and never
reaches its disc's rim or the sea, and is found whole from `terrain_at`, so it is the same lake
from every square it touches. Lakes and seas carry their outlines as cell-corner rings
(ADR-0018 point 2). `tests/geo/geo_dump.cpp` (`peo_geo`) draws squares for review. Not built:
river courses (PEO-111), towns fitted to the map (PEO-096), keeping neighbouring squares
resident; no stage reads the map yet.

**Settlement sites** (ADR-0018 point 3, PEO-096; `settlement_site.hpp`). Site cells of 125 cells
(12.5 km, an eighth of a square) tile the world from cell 0, each inside one square; a cell
holds a site with a hashed chance (about one in three: long empty stretches). Its size is a
hashed weighted draw (hamlet to city, 0.3 to 3.5 km across); 16 hashed candidate centres are
scored in integers (bonuses for sea and lake inside the disc and for mountains in view, less the
disc's relief and the centre's height, plus a hashed tie-breaker) and the best wins. A site reads
only its own site cell of the region, so it is the same whatever region holds the cell. Not
built: rivers and river towns (PEO-112), town layouts (PEO-114, PEO-115).

### The Dead (`dead.hpp`)

`Dead` is deliberately tiny; thousands must step per turn. They move in staggered slots
(D-031): a slot is two substeps, the even ones (ADR-0016). Every `dead_cycle` (18 substeps, 9
slots) the world polls the horde, and each unit gets `plan_slots`: 9 / (`step_substeps` / 2)
slots, the whole part always and the fraction a
hashed chance, evenly spaced from a hashed offset. Hashes of (stage seed, cycle, index), never a
shared stream, so who moves when changes each cycle. At its slot a unit `decide_move`s: its
strongest neighbour, only if no Dead stands there and no pending move reserved it; a calm Dead
never sidesteps or climbs over another, it stays put. The move lands one slot later; in each
slot the slot's units decide before last slot's moves land, so a vacated tile is free only
from the next slot and a crowd files through gaps, jamming a one-wide corridor. Target: the **weighted draw** of scent-mobs round 3 (scent, aggregate, company,
attractor, stimulus, repellent, footing terms), triggers by general direction, sound events,
trips and trample, population aggregates beyond the detailed radius. Struct-of-arrays when the
profiler asks.

## Turn model (ADR-0012)

Time moves only through `World::step(Action)`. Order within a turn: apply the player's action;
scent step; the Dead step (moves, contests, trample); sound events resolve. Core spawns no
threads; the frontend owns the worker and draws only the committed world.

Core does use threads it is lent (D-035, ADR-0014, PEO-080): `peo/core/executor.hpp` is a
parallel-for over N pieces, null meaning serial. `World::set_executor` hands it to the scent
wave, whose rounds pull active tiles as pieces and whose direction bytes refresh by tile (no
two pieces write one cell); `speculate()` runs the scent half and the Dead's half as two
pieces; a slot's batch of the Dead's decisions at least `WorldParams::parallel_decide_min`
long splits by unit, each into its own slot, with claims resolved after in slot order. The
frontend's `ThreadPool` (`src/app/thread_pool.hpp`, `--threads N`) runs a run started inside a
piece, or beside another thread's, inline. Serial, shuffled and threaded runs are bit-identical
(golden tests; the threaded ones also run under ThreadSanitizer in CI).

The scent field can also run on a GPU (ADR-0014, PEO-081). Core holds only the seam,
`peo/core/field_backend.hpp`: a calm update's rounds, handed back as values, per-tile changed
flags and direction bytes; the CPU pull is the reference. `peo_gpu` (`src/gpu`, SDL_GPU on
Vulkan, GLSL compiled to SPIR-V at build time and embedded) implements it bit for bit, and is
built only when a shader compiler is found. Only `speculate()` uses it, so a turn never waits on
the device; the frontend makes the device and picks the backend by stage size (`--gpu-min-cells`,
`--no-gpu-compute`). Its tests (`tests/gpu`) run on the M1 and on lavapipe in CI.

### The clock (PEO-040, D-015, ADR-0016)

The world counts substeps (`World::substeps()`; `game_ms()` for display): a substep is 250 ms
and a walking step is 12, so a walk reads as 3 game seconds. Each `Action` carries a duration
(`kStepSubsteps` and `kWaitSubsteps`, both 12; `kRunStepSubsteps` 6). Scent and the Dead update
on their own cadence, once every `WorldParams::update_period` (12 substeps), whatever the player
does. An action applies at once, then its substeps are logged on the player's tile
(`occupancy()`, merged per tile). At each period boundary one scent update runs: each logged tile
deposits `strength - age_cost × (period - held) / period` (a full period exactly `strength`, a
runner's tiles a little less, never more), then the wave updates. Everything runs on the even
substeps, which carry the Dead's slots (above); the odd ones are the between layer, which passes
unused until the owner designates something for it. So every seed and action list gives the same
world as with whole seconds. Durations authored in real time (sleep, crafting, rot) convert once
with `to_substeps` / `ms_to_substeps`, keeping their hours: a game hour is 1200 updates (D-039).
Speculation is per update: an action that crosses no boundary leaves it valid. `turn()` still
counts actions.

### Revisits (ADR-0019, PEO-094)

Beside the stage clock runs one clock since the Event, `World::since_event()`: the start day
(`WorldParams::start_day`, the one-week preset) plus the substeps of every earlier stage and
this one; it never decreases. Each of the Dead carries a `fate`, a threshold drawn from a hash
of the stage seed when it spawns (no spawn draw moves), uniform above the attrition curve at
that clock (`peo/core/aftermath.hpp`: integer, piecewise linear, saturating). `store_area()`
copies a generated stage's index, clock and horde into an `AreaRecord`; `resume_area()`
rebuilds the stage as `load_stage` does, moves any unit off the entry (before thinning, so
where it lands never depends on when), then `thin_horde` drops every unit whose fate the curve
has passed: one comparison a unit, reading only the clock now, so several short absences
equal one long one. Nothing in play calls these yet; aging player-made things is PEO-109 and
the streaming catch-up job PEO-110.

### Computing while waiting (PEO-007)

While the player thinks, a worker in `src/app/main.cpp` runs `World::speculate(Speculation&)`:
the next update of the scent wave with no deposit, and the Dead's slots up to
and including that boundary, recorded per slot with any poll (PEO-060). Between updates the
Dead read only the last update's scent, never the player, so those slots are fixed once an
update commits. On input, `World::commit(Speculation&, Action)` applies the action and runs its
substeps, replaying the recorded slots; at the boundary it adds the logged deposits with
`ScentWave::patch_deposit` (the min-plus patch, exact for one round per update; a faster wave
commits live). A stale speculation, or a slot outside the record, runs live, exactly as
`step()`. The field is integer, so equal means equal. `Speculation` buffers are reused, so a
turn allocates nothing. Measured on the Builder's M1 (release, one walking step, 300 samples,
PEO-030): 200x120 with 5000 Dead, `commit` 0–1 µs, `speculate` 28–277 µs; 512x512 with 50,000,
`commit` 0–3 µs, `speculate` 0.1–1.5 ms.

Turn keys go through `TurnInput` (`turn_input.hpp`, D-032, amending D-011): a distinct press
waits in a queue of up to 3 and the queue plays out no faster than 3 turns a second; a held key's
auto-repeat is taken only when the cap allows and nothing waits, so it never builds lag and stops
the moment the key is released. While taps wait, one SDL timer wakes the loop at the next due time;
with none waiting nothing wakes it. Depth and rate are tunables in `TurnInputParams`.
R toggles running (steps of `kRunStepSubsteps`, 6 substeps, D-015); the cap limits key presses, not game
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
| Persistence | `save: a restored world continues bit-identical to the one never saved` |
| Revisits | `revisit: one long absence equals several short ones` |

## Planned seams (not built)

- `World`: **built (PEO-002)** in `peo/core/world.hpp` + `src/core/src/world.cpp`, with
  `peo/core/action.hpp`. Owns stage, scent, horde, player; `step(Action)` is the single
  entry point; `speculate` / `commit` built (PEO-007).
- `Save`: **built (PEO-091)**. `peo/core/save.hpp` is the container (ADR-0002, ADR-0017): a
  versioned image of sections, each with its id, version, sync scope and crc32, written and
  read as bytes in memory; `read_save` never trusts a length and names what it refuses.
  `peo/core/byte_io.hpp` reads and writes payloads little-endian. `World::save` /
  `World::restore` own the sections: WRLD, STAG (hand-built stages only), SCNT, HORD, OCCL
  in the World scope, CHAR in the Character scope. Restore checks every value against the
  stage it builds and changes nothing unless the image is Ok. Files, the two fallbacks and
  zstd are the app's and PEO-107 / PEO-108.
- `Content`: loads `content/` (registry, materials, items, modifiers) into runtime tables;
  the derivation rules in `tools/content/derive.py` are the executable spec to port.
- `Replay`: seed plus action log reproduces a run headlessly; the Architect's tool for
  reviewing bugs it cannot see. First piece of the playtest harness.
- `Fields`: the generic scalar-field service (scent channels, later sound, heat, light).
- `Population`: region-cell counts and attraction; spawn-in and merge-out at the edge.
