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
                              └─ horde.hpp  Hordeling, step_horde
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

### Hordes (`horde.hpp`)

`Hordeling` is deliberately tiny; thousands must tick per frame. `step_horde`
is one pass over a vector. When profiling asks, the vector becomes
struct-of-arrays without changing callers.

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
| Horde correctness | `horde: units never enter walls` |
| Horde scale | `horde: a thousand units tick` |

## Planned seams (not built)

- `World`: owns current stage, scent layers, horde, player; `tick()` is the
  single simulation entry point the frontend calls.
- `Replay`: input log + seed = reproducible run; the Architect's tool for
  reviewing bugs it cannot see.
- `Profile`: per-system tick timings the Builder reports back.
