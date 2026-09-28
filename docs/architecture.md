# Architecture

One rule above all: **the simulation never knows about SDL.** `peo::core`
is a static library with no I/O, no globals and no clocks; given a seed it
produces the same world and the same horde behaviour on every platform. The
frontend feeds it input and draws its state.

```
 src/app (SDL3)  ──input──▶  peo::core  ──state──▶  src/app draws
                              │
                              ├─ rng.hpp    xoshiro256**, seeded, no std::rand
                              ├─ grid.hpp   dense row-major Grid<T>; Grid<bool> is bytes
                              ├─ scent.hpp  ScentField: deposit / step / strongest_neighbour
                              ├─ stage.hpp  StageSpec, stage_seed, generate_stage
                              └─ dead.hpp   Dead, step_horde
```

## Core systems

### Scent (`scent.hpp`)

The signature mechanic. A `ScentField` is a double-buffered `Grid<float>`.
Each tick the player (and later corpses, bait, fire) `deposit`s; `step`
diffuses to the four orthogonal neighbours and decays. Walls reflect scent.
`strongest_neighbour` is the entire horde AI today: climb the gradient.

Design constraints:
- `step` is O(cells), linear memory, no allocation. Keep it SIMD-friendly.
- `ScentParams` are per-stage tunables, never literals in code.
- Multiple layers (player, blood, fire) are separate fields, not channels.

### Stages (`stage.hpp`)

The world is an endless sequence indexed by `stage_index`. `stage_seed(world,
index)` is a pure hash, so any stage can be regenerated on demand and never
needs saving. `generate_stage` is currently random walls with a carved entry
and exit; room/cave generators replace it behind the same signature.

### The Dead (`dead.hpp`)

`Dead` is deliberately tiny; thousands must step per turn. `step_horde` is
one pass over a vector. When profiling asks, the vector becomes
struct-of-arrays without changing callers.

## Turn model (decision D-002)

Time moves only through `World::step(Action)`. An `Action` is a step in a
direction, a wait, or later an interaction. Order within a turn:

1. apply the player's action (move, deposit scent at the new cell);
2. scent step (diffuse + decay, all layers);
3. the Dead step (gradient climb, contact resolution).

### Computing while waiting

Steps 2 and 3 barely depend on the player's action. Scent diffusion is
linear, so `diffuse(field + δ) == diffuse(field) + diffuse(δ)`, and a
deposit δ at one cell touches only that cell and its four neighbours
after one step. A Dead's move depends only on the scent in its eight
neighbours, so only the Dead within two cells of the player's new position
can change their minds.

So the world computes the next turn speculatively while the player thinks
(assume "wait"), and on input patches the small player neighbourhood:

```
idle:   spec = speculate(world_T)            // full-field work, off the input path
input:  world_T+1 = commit(spec, action)     // O(neighbourhood) patch, then clamp floor
```

Rules that make this safe:
- `commit(speculate(w), a)` must equal `step(w, a)` bit for bit; a golden
  test enforces it for every action on random worlds.
- Core exposes `speculate` and `commit` as pure functions on plain data;
  the frontend owns the worker thread and the buffers. Core never spawns
  threads, so determinism and tests stay single-threaded.
- The renderer reads the last committed `World` only. A speculation buffer
  is never drawn.
- Lookahead deeper than one turn is allowed only for the "wait" action
  (an idle player is the common case); a step discards deeper speculation.
- The non-linear floor clamp is applied after patching, so it cannot break
  linearity.

## Frontend (`src/app/main.cpp`)

Uses SDL3's callback main (`SDL_AppInit/Event/Iterate/Quit`) and
`SDL_RenderDebugText`, so no font asset is needed yet. Fixed tick
(`kTickMs`), render every frame. Anything that looks like game logic here
should move to core with a test.

## Boundaries that tests protect

| Boundary | Test |
|----------|------|
| Determinism | `stage: generation is deterministic`, `rng: same seed` |
| Scent physics | `scent: mass is conserved`, `walls block scent` |
| The Dead | `dead: the dead never enter walls` |
| Scale | `dead: a thousand dead step` |
| Turn model | `world: commit(speculate) == step` (PEO-007) |

## Planned seams (not built)

- `World`: owns current stage, scent layers, horde, player; `step(Action)`
  is the single simulation entry point the frontend calls (PEO-002).
- `speculate` / `commit`: the compute-while-waiting pair above (PEO-007).
- `Replay`: input log + seed = reproducible run; the Architect's tool for
  reviewing bugs it cannot see.
- `Profile`: per-system tick timings the Builder reports back.
