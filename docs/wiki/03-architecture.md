# 3 · Architecture

## The rules the code obeys

These are not style preferences; tests, CI and the review checklist
(`.agents/skills/cpp-core/SKILL.md`) enforce them. Break one and something downstream
(replays, saves, speculate/commit, multiplayer later) stops being possible.

1. **The simulation never knows about SDL.** `peo::core` (`src/core`) is a static library
   with no I/O, no globals, no threads of its own and no clocks. The frontend feeds it
   `Action`s and draws its state. Logic in `src/app` is a bug.
2. **Deterministic from a seed.** Same seed plus same actions gives the same bits, on every
   platform and compiler. So: no `std::rand`, no `std::mt19937`, no `std::uniform_*`
   distributions (their output differs between standard libraries), no wall clock, no
   unordered iteration feeding results. Randomness is `Rng` (xoshiro256\*\*) or, mostly,
   **stateless hashes** of named inputs (`hash_u64(a, b, c)`, a splitmix64 fold in
   `rng.hpp`), so one draw never shifts another.
3. **Integers in the simulation.** Scent, desire, wind, noise, curves and sight are all
   integer or fixed-point (Q16 = 65536 is 1.0). Floats differ across compilers and SIMD
   paths; integers do not. Trig comes from a fixed cosine table (`wind.cpp`). The float
   uses left (`Rng::unit` and `Rng::chance` behind the stage's wall density, `hash_unit`
   behind the slot chance in `plan_slots`) are simple single operations, deterministic on
   IEEE platforms, but they are the first place to look if a cross-platform mismatch ever
   appears.
4. **No allocation per tick.** Buffers are sized when a stage loads and reused; a
   `Speculation` recycles its vectors. Some tests pin it (map memory, turn input, the
   executors); for the World's turn it rests on review (`/cpp-core`) and the perf numbers.
5. **Every mechanic lands in core first, with a headless test.** The frontend gets it last.
6. **Performance is a feature.** The budget (D-021): a speculated update under 100 ms and its
   commit under 1 ms on the M1 Air, at 200x120 with 5,000 Dead, 512x512 with 20,000 and
   512x512 with 50,000. `docs/bottlenecks.md` lists the passes that cost the most.
7. **Public headers in `src/core/include/peo/core/`, one concept per file.** Named constants
   instead of magic numbers; tunables live in `...Params` structs.

## Layers

```
 src/app (SDL3, peo)                       src/gpu (peo_gpu, optional)
   keys -> TurnInput -> Action               GpuFieldBackend : FieldBackend
   ThreadPool : Executor                         |
   Speculator thread                             |
        |  Action, executor, backend             |
        v                                        v
 peo::core (src/core, peo_core)  <----- seams: Executor, FieldBackend
   World  owns  Stage, ScentWave, DesireField, HordeState, player, MapMemory, clock
   free-standing: geography, settlement sites, noise, sight, look, save, aftermath
        |
        v  const World& / snapshots
 src/app draws (camera, HUD, peek, glyph atlas)
```

`docs/SYSTEMS.md` describes the target: Foundation, Engine, World, Simulation, Game, plus
Tools, with 15 rules (R1 to R15). The code grows toward it one system at a time through the
queue, never by a framework-first rewrite. Today there is one core target; the split into
per-system targets happens as systems appear (`docs/architecture.md`, table at the top).

## `World`: the one entry point

`World` (`src/core/include/peo/core/world.hpp:151`) owns the whole simulation. Time moves only
through it:

| Call | Does |
|---|---|
| `World(seed, params)` | Clamp the params, load stage 0 |
| `step(action)` | Spend one action live |
| `speculate(spec)` | Compute the next update ahead, read-only (safe on another thread) |
| `commit(spec, action)` | Spend one action using the speculation: bit-identical to `step` |
| `load_stage(i)` / `load_layout(stage, player, horde)` | A generated stage, or a hand-built one (tests, siege) |
| `save(build)` / `restore(image, issues)` | The whole world as save sections and back |
| `store_area()` / `resume_area(record)` | Leave a stage and come back to it later, thinned |
| `remember(sight)` | Fold what the player sees into map memory |

Everything else is a `const` read: `stage()`, `scent()`, `horde()`, `player()`, `desire()`,
`wind()`, `memory()`, `turn()`, `substeps()`, `updates()`, `since_event()`. Full list in the
[API reference](11-api-reference.md).

### One turn, step by step

`World::step` (`src/core/src/world.cpp:521`) is three lines; `advance` (`:498`) is the
clock:

1. `apply_action`: a step moves the player at once; into a wall or off the map it becomes a
   wait (the turn is still spent).
2. `advance(action.substeps)`: for each substep,
   - log one substep on the player's tile (the occupancy log, merged per tile);
   - on **even** substeps, run the Dead's slot (`dead_slot`): at a cycle start, the poll
     assigns everyone's slots for the cycle; the slot's units decide; last slot's moves land;
   - on an **update boundary** (every 12 substeps), `run_update`: each logged tile deposits
     scent, the scent field updates, the desire field is rebuilt, the log clears.
3. `finish_turn`: count the turn; if the player stands on the exit, load the next stage.

Odd substeps only count and log: they are the "between layer" ADR-0016 reserves for later.
[Time and turns](04-time-and-turns.md) has the numbers.

### What the World holds

- `Stage stage_`: the wall grid (`blocked`), `openness` to wind per cell, entry and exit.
- `ScentWave scent_`: the field. Rebuilt per stage, with a fresh partner token.
- `DesireField desire_`: nine log-odds per cell that the Dead draw from until the next update.
- `HordeState dead_`: the Dead, occupancy and reservation grids, this cycle's slot buckets,
  the moves deciding and landing (`world.hpp:84`).
- `player_`, `memory_`, `log_` (occupancy), the stage clock (`substeps_`, `updates_`,
  `turn_`), `earlier_substeps_` for the clock since the Event, the spawn `Rng`, and an
  `epoch_` bumped on every load and restore so old speculations can never commit.

## Threads and the GPU are lent, never owned

Core never creates a thread or a device (D-035, ADR-0014). Two seams let the frontend lend
them:

- **`Executor`** (`executor.hpp`): a parallel-for, `run(pieces, body)`. Null means serial.
  The scent rounds split by tile, the desire build by bands of rows, `speculate` runs its
  scent half and its Dead half as two pieces, and a slot with at least
  `parallel_decide_min` (16,384) deciding Dead splits by unit, with claims resolved
  afterwards in unit order. Every split writes disjoint cells or its own slots, so serial,
  shuffled and threaded runs are bit-identical; golden tests check it and CI runs them under
  ThreadSanitizer. The frontend's `ThreadPool` (`src/app/thread_pool.hpp`) implements it.
- **`FieldBackend`** (`field_backend.hpp`): one calm (or windy) scent update's rounds on
  some other device. `peo_gpu` implements it on SDL_GPU compute over Vulkan, with GLSL
  compiled to SPIR-V at build time (`src/gpu/shaders/`). It must match the CPU pull bit for
  bit, and may decline. Only `speculate` uses it, so a live turn never waits on a GPU.

## Determinism tools you will meet

- `stage_seed(world_seed, index)`: a splitmix mix, so neighbouring stages are unrelated.
- Salts: each consumer XORs its own constant into a seed so streams never overlap
  (`world.cpp:14` to `:21`: horde placement, wind, the Dead's draw, fates).
- `hash_u64(a, b, c)` and `hash_unit`: counter-based draws keyed by (salt, cycle or slot,
  unit index). This is why the Dead can decide in any order or on any thread and still
  give the same result.
- `World::equivalent(a, b)`: compares every piece of state; the golden tests' oracle.
