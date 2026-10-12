# Scent

The signature mechanic. One integer field over the stage that says, per cell, how strongly
the player's scent reads there now. The Dead climb it. Decisions: D-024 (the geodesic field),
D-020 (wind), PEO-078/079/085 (speed), PEO-048/087 (wind), PEO-030 (the patch). Code:
`src/core/include/peo/core/scent_wave.hpp`, `src/core/src/scent_wave.cpp`,
`src/core/src/wave_kernels.hpp`. Design of record: `docs/design/scent-mobs.md`.

## The formula

A cell holds the best value over every deposit that has reached it:

```
scent(cell) = max over deposits of  strength - distance_cost x route - age_cost x age
```

- `route` is the shortest walkable path, 8-connected, never cutting a wall corner (a
  diagonal step needs both orthogonal cells beside it open, PEO-044).
- `age` is updates since the deposit.
- Read as 0 when negative; a deposit is gone where it reaches 0.

Defaults (`WaveParams`, `scent_wave.hpp`): `distance_cost = 8`, `age_cost = 8`,
`strength = 60 x (8 + 8) = 960`, `speed = 1`, no wind. Equal costs mean a trail one update
old reads like one a cell further away. Because the front moves `speed` cells per update, a
cell `r` away is reached no sooner than `r` updates later, so a deposit from a standing player
reads out to 960 / 16 = **60 cells** (`kWaveReachCells`).

A trail: the player walks one cell per update and each cell deposits 960. The cell they just
left is one update older **and** one cell further, so along a trail the scent rises by
`distance_cost + age_cost` = 16 per cell toward the player. That slope is what the Dead feel.

## How it is stored: the age line

Ageing every cell every update would cost the whole map. Instead a value is stored with its
age folded in:

```
deposit(at, s):    stored = max(stored, age_line + s)
sample(at):        max(stored - age_line, 0)
age_line         = age_cost x updates
```

The line moves up by `age_cost` per update, so everything reads 8 weaker without being
touched. An unreached cell holds `kWaveUnreached` (INT32_MIN / 2). The line must stay inside
int32: `max_wave_updates()` gives 2^27 updates, about 12 game years, and a `static_assert`
fails the build if the defaults ever drop below one year (`scent_wave.hpp`). A wave lives
for one stage; outliving that would need a rebase pass (PEO-077, ADR-0011).

## One update: rounds of a pull

`ScentWave::update` (`scent_wave.cpp`):

1. `++updates` (the age line moves).
2. `speed` **full rounds**. In a round every open cell looks at its eight neighbours'
   **round-start** values and takes

   ```
   candidate = max over allowed neighbours n of (value(n) - step_cost(n -> cell))
   new value = candidate  if candidate > old and candidate > age_line, else old
   ```

   into a second buffer; then the buffers flip. Calm, `step_cost` is `distance_cost`.
3. Under a wind, `gust` extra rounds that carry scent only downwind and only from outdoor
   cells.
4. Refresh the direction bytes of the cells that changed (`flow_target`, below).

Because each round is a max over offers made from round-start values, the result does not
depend on the order cells are visited. That is what makes threads, SIMD, the GPU and the
patch all exact.

### Doing only the work that matters

- The map is cut into **tiles of 128x8 cells** (`kWaveTileWidth`, `kWaveTileHeight`). A
  round pulls only tiles that changed last round and their neighbours (`expand_active`). A
  tile nobody pulls holds the same values in both buffers, so skipping it is free. The work
  grows with the active front, not the map.
- The row kernel (`wave_kernels.hpp`, `pull_row`) is branch-free integer max and
  subtract, so compilers vectorise it. It is built twice from one source: plain (NEON on
  arm64, SSE2 on x86-64) and AVX2 (`wave_kernels_avx2.cpp`); a CPU check picks once per
  process. Both give the same bits; tests compare them.
- Rounds split by tile across the `Executor`; no two pieces write one cell.
- The World's wave and its `Speculation`'s wave are **partners** (same token): `sync_from`
  copies only the tiles either side wrote since they last matched.

## Wind

Each stage draws its own wind from its seed (`World::start_field`, `world.cpp`): a
direction 0 to 359 degrees (clockwise from east, `toward_degrees`) and an intensity 0 to
`WorldParams::wind_max`. Core's default `wind_max` is 0, always calm; the game sets 8 (full)
and `gust = 2` (`src/app/main.cpp`). The HUD shows it as "wind to SE 5".

`wind_table` (`wind.cpp`) turns it into a per-direction step adjustment, integers only:

```
progress(d) = dx(d) x cos(toward) + dy(d) x sin(toward)          (x 4096, fixed table)
step(d)     = round(intensity x progress / 4096) - wind_loss x intensity,  clamped to +-255
downwind(d) = progress > 0
```

and the kernel's cost for scent travelling in direction d into a cell from neighbour n
(`wave_kernels.hpp`):

```
step_cost = max(1, distance_cost - step(d) x openness(n) / 255)
```

So a full wind (8, `kWindFull`, which must equal `distance_cost`) makes a straight downwind
step outdoors cost the floor of 1 and an upwind step 16. `openness` is 255 outdoors and 0
indoors (`kOpennessOutdoors`/`kOpennessIndoors`). `generate_stage` marks every open cell
outdoors; hand-built layouts may pass their own grid, and buildings will mark their insides. `wind_loss` (default 0) models
scent lost upward outdoors. A calm wave keeps the windless kernel, bit-identical to a world
with no wind code at all.

Windy rounds cost about five calm ones on the CPU; that is the case where the GPU backend
pays (from 512x512, `docs/bottlenecks.md` row B3).

## The exact patch

To let `speculate` run the update before the player has moved, the speculated update runs
**with no deposit**, and on commit the player's logged deposits are added afterwards with
`patch_deposit(before, at, strength, blocked, openness)` (`scent_wave.cpp`). For speed 1
this is exact: a round is a max over offers, so the deposit's offers can be added after the
others. Under wind (gust rounds and all) the deposit is run alone through the update's rounds
on a small block round it and maxed in, which is exact for any speed. `World::commit` still
falls back to a live step when `speed != 1`.

## Reading the field

- `sample(at)`: scent now, never below 0.
- `strongest_neighbour(from, blocked)`: the neighbour with the greatest sample, strictly
  above `from`, first in `kNeighbours8` order on ties (N, NE, E, SE, S, SW, W, NW).
- `flow_target(from)`: the same answer from one stored direction byte per cell, rewritten
  for changed cells each update (PEO-079). It was the whole of the Dead's AI before the
  weighted draw; since PEO-009 nothing in play reads it, and PEO-100 asks whether to drop it.
- `values()`, `changed_tiles()`, `flow_bytes()`, `active_cells()`: for tests, the GPU
  backend and diagnostics.

The frontend's scent overlay (Shift+S) draws `*`, `+`, `:` where a cell is at least 10%,
0.1% or 0.001% of the strongest cell (`src/app/main.cpp`).

## Tunables

| Tunable | Where | Default | Raising it |
|---|---|---|---|
| `strength` | `WaveParams` | 960 | Longer reach (reach = strength / (distance + age cost)) |
| `distance_cost` | `WaveParams` | 8 | Scent falls off faster with distance; shorter reach. Must equal `kWindFull` (static_assert) |
| `age_cost` | `WaveParams` | 8 | Trails fade faster; steeper trail slope for the Dead |
| `speed` | `WaveParams` | 1 | Front moves faster; more work per update; commit falls back to live |
| `gust` | `WaveParams` | 0 core, 2 in game | Scent outruns a walker downwind |
| `wind_loss` | `WaveParams` | 0 | Outdoor wind also thins scent |
| `wind_max` | `WorldParams` | 0 core, 8 in game | Stronger winds possible per stage |
| tile size | `kWaveTileWidth/Height` | 128 x 8 | Performance only |

Any change here moves the swarm scenario numbers and the siege baseline
([Build and test](build-and-test.md#on-demand-suites)); a deliberate move rewrites the
baseline with its reason in the commit.

## Designed, not built

From `docs/design/scent-mobs.md` (later rounds win) and `docs/design/scent-performance.md`:
blood (PEO-003), rot from the Dead masking
human scent (PEO-039), soak, a per-tile scent memory from contact (PEO-038), a reach radius
with sealed buildings skipped (PEO-047), z-level routes (PEO-049), a 6x6 section aggregate
layer (PEO-051), several channels with a dominance matrix (`content/registry/scent.toml` lists them), and
bigger stages (PEO-083).
