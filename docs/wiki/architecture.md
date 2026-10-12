# Architecture

## Rules the code obeys

Enforced by tests, CI and the review checklist (`.agents/skills/cpp-core/SKILL.md`).
Speculation, saves, threads, the GPU backend and future replays and multiplayer all depend on
them.

1. **The simulation never knows about SDL.** `peo::core` has no I/O, no globals, no threads
   of its own and no clocks. The frontend feeds it `Action`s and draws its state; logic in
   `src/app` is a defect.
2. **Deterministic from a seed.** The same seed and actions give the same bits on every
   platform and compiler. No `std::rand`, `std::mt19937` or `std::uniform_*` distributions
   (their output differs between standard libraries), no wall clock, no unordered iteration
   feeding results. Randomness comes from `Rng` (xoshiro256\*\*) or, mostly, from **stateless
   hashes** of named inputs (`hash_u64(a, b, c)`, a splitmix64 fold in `rng.hpp`), so one draw
   never shifts another.
3. **Integers in the simulation.** Scent, desire, wind, noise, curves and sight are integer or
   fixed point (Q16: 65536 is 1.0). Trigonometry comes from a fixed cosine table
   (`wind.cpp`). The float uses that remain (`Rng::unit` and `Rng::chance` behind the stage's
   wall density, `hash_unit` behind the slot chance in `plan_slots`) are single IEEE
   operations and the first suspects in any cross-platform mismatch.
4. **No allocation per tick.** Buffers are sized when a stage loads and reused; a
   `Speculation` recycles its vectors. Map memory, turn input and the executors have tests
   that pin it; for the turn as a whole it is a review rule.
5. **Every mechanic lands in core first, with a headless test.** The frontend gets it last.
6. **Performance is a feature.** Budget D-021: a speculated update under 100 ms and its
   commit under 1 ms on the M1 Air, at 200x120 with 5,000 Dead, 512x512 with 20,000 and
   512x512 with 50,000. `docs/bottlenecks.md` registers the costliest passes.
7. **One concept per public header** in `src/core/include/peo/core/`. Named constants, no
   magic numbers; tunables in `...Params` structs.

## Layers

```
 src/app (peo)                              src/gpu (peo_gpu, optional)
   keys -> TurnInput -> Action                GpuFieldBackend : FieldBackend
   ThreadPool : Executor                          |
   Speculator thread                              |
        |  Action, executor, backend              |
        v                                         v
 peo::core (peo_core)  <------ seams: Executor, FieldBackend
   World  owns  Stage, ScentWave, DesireField, HordeState, player, MapMemory, clock
   free-standing: geography, settlement sites, noise, sight, look, save, aftermath
        |
        v  const reads
 src/app draws (camera, HUD, peek, glyph atlas)
```

The target framework is `docs/SYSTEMS.md`: Foundation, Engine, World, Simulation, Game and
Tools layers, under fifteen rules R1 to R15. The code grows toward it one system at a time
through the queue rather than by a framework-first rewrite; `docs/architecture.md` tracks
which rules already hold. There is one core library target; it splits per system as systems
appear (R1, R2).

## `World`

`World` (`world.hpp`, `world.cpp`, `world_save.cpp`) owns the simulation, and time moves only
through it.

| Call | Does |
|---|---|
| `World(seed, params)` | Clamps the params and loads stage 0 |
| `step(action)` | Spends one action live |
| `speculate(spec)` | Computes the next update ahead; `const`, safe on another thread |
| `commit(spec, action)` | Spends one action using a speculation; bit-identical to `step` |
| `load_stage(i)`, `load_layout(stage, player, horde)` | A generated stage, or a hand-built one |
| `save(build)`, `restore(image, issues)` | The world as save sections, and back |
| `store_area()`, `resume_area(record)` | Leaves a stage and returns to it later, thinned |
| `remember(sight)` | Folds a sight field into map memory |

Everything else is a `const` read. Full list: [API reference](api-reference.md).

### State

- `stage_`: walls (`blocked`), per-cell `openness` to wind, entry, exit.
- `scent_`: the [scent](scent.md) field, rebuilt per stage with a fresh partner token.
- `desire_`: nine log-odds per cell that [the Dead](the-dead.md) draw from until the next
  update.
- `dead_` (`HordeState`): the units, occupancy and reservation grids, the cycle's slot
  buckets, moves deciding and landing.
- `player_`, `memory_`, `log_` (occupancy), the stage clock (`substeps_`, `updates_`,
  `turn_`), `earlier_substeps_` for the clock since the Event, the spawn `rng_`, and
  `epoch_`, bumped by every load and restore so a speculation from before can never commit.

### The flow of one turn

`World::step` applies the action, advances the clock, and finishes the turn:

1. `apply_action`: a step moves the player at once; into a wall or off the map it becomes a
   wait, and the turn is still spent.
2. `advance(duration)`, one substep at a time:
   - log the substep on the player's tile (`log_substeps`);
   - on an **even** substep, run the Dead's slot (`dead_slot`): at a cycle start the poll
     assigns everyone's slots, then the slot's units decide, then last slot's moves land;
   - on an **update boundary**, `run_update`: logged tiles deposit scent, the field updates,
     the desire field is rebuilt, the log clears.
   Odd substeps only count and log: the "between layer" ADR-0016 reserves.
3. `finish_turn`: count the turn; on the exit, load the next stage.

`commit` follows the same path but replays the speculation's recorded slots and finishes the
first boundary from the speculated field ([Time and turns](time-and-turns.md)).

## Threads and the GPU

Core never creates a thread or a device (D-035, ADR-0014). Two seams lend them:

- **`Executor`** (`executor.hpp`): a parallel-for, `run(pieces, body)`; null means serial.
  Scent rounds split by tile, the desire build by bands of rows, `speculate` runs its scent
  half and its Dead half as two pieces, and a slot with at least `parallel_decide_min` deciding
  units splits by unit with claims resolved afterwards in unit order. Pieces write disjoint
  cells or their own slots, so serial, shuffled and threaded runs are bit-identical; golden
  tests check it, under ThreadSanitizer in CI. The frontend's `ThreadPool`
  (`src/app/thread_pool.hpp`) implements it; a run started inside a piece, or while another
  thread's run holds the pool, runs inline.
- **`FieldBackend`** (`field_backend.hpp`): one scent update's rounds on another device,
  described by `FieldRounds` (views of the wave's masks, values, flags and direction bytes).
  `peo_gpu` implements it on SDL_GPU compute over Vulkan, with GLSL compiled to SPIR-V at build
  time (`src/gpu/shaders/`). It must match the CPU pull bit for bit and may decline. Only
  `speculate` uses it, so a live turn never waits on a device.

## Determinism tools

- `stage_seed(world_seed, index)`: a splitmix mix, so neighbouring stages are unrelated.
- Salts: each consumer XORs its own constant into a seed so streams never overlap (in
  `world.cpp`: `kHordeSeedSalt`, `kWindSeedSalt`, `kDrawSalt`, `kFateSalt`).
- `hash_u64(a, b, c)`, `hash_unit(a, b, c)`: counter-based draws keyed by (salt, cycle or
  slot, unit index), which is why the Dead can decide in any order or on any thread.
- `World::equivalent(a, b)`: compares every piece of state; the golden tests' oracle.

## Tests that guard the architecture

| Promise | Test |
|---|---|
| Speculation is exact | `world: commit(speculate()) is bit-identical to step()`, and its wind variant |
| The patch is exact | `scent_wave: patch_deposit after update equals deposit then update` |
| Saves are exact | `save: a restored world continues bit-identical to the one never saved` |
| Revisits are closed form | `revisit: one long absence equals several short ones` |
| Threads change nothing | the serial, shuffled and threaded golden runs in `test_executor.cpp` |
| Determinism | `world: same seed same world after 100 turns`, `stage: generation is deterministic`, `rng: same seed gives same sequence` |
