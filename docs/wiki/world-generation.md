# World generation

World generation has two subsystems that are not yet connected:

1. **Stages**: the maps play happens on. Random walls, an endless sequence.
2. **Geography**: the real map, built bottom-up (terrain, seas, lakes, settlement sites).
   Tests and `peo_geo` use it; no stage reads it yet.

The target is one continuous map with no stage jumps (ADR-0007, D-041, PEO-118), generated
in levels (macro, region, settlement, structure, tile, contents) as a baseline world plus the
Event plus aftermath on one clock (ADR-0009, ADR-0011, ADR-0018). Design:
`docs/design/world-generation.md`, `docs/design/world-catalog.md`.

## Stages (`stage.hpp`, `stage.cpp`)

- `stage_seed(world_seed, index)`: a splitmix64 mix, so stage 5 and 6 are unrelated.
- `stage_spec(world_seed, index)`: 80x45 by default (the World overrides size from
  `WorldParams`), wall density `0.10 + 0.01 x index`, capped at 0.35 (`stage.cpp`).
- `generate_stage(spec)`: each cell is a wall with chance `wall_density`; the border is
  always wall; the entry is a random cell on column 1, the exit a random cell on column
  width-2, both carved open. Openness is outdoors on every open cell.

There is no connectivity pass, so the exit is not guaranteed reachable; the frontend's N key
skips a stage. `generate_stage`'s signature is
the seam: generators (caves, PEO-005; towns later) plug in behind it.

Two hand-built layouts exist for tests and are played through `World::load_layout`:
`tests/core/town_fixture.hpp` (a cramped 200x120 town with houses, offices and a one-wide
corridor, from the pathfinding review) and `tests/siege/siege_layouts.hpp` (three small
buildings on a 141x141 stage).

## Geography (`geography.hpp`, `geography.cpp`, ADR-0018, PEO-095)

**Scale**: one geography cell is 100 m (100 tiles of 1 m); a **square** is 1000x1000 cells,
100 km a side (`kGeoCellTiles`, `kSquareCells`). The world is endless in squares (D-041).

**Source of truth**: `terrain_at(seed, params, x, y)` gives a cell's elevation and forest as
a pure function of the world seed and the cell's world coordinates, so squares join without
seams in any order. `generate_geography(seed, params, rect)` fills any rectangle cell for
cell equal to `terrain_at` (scanning by row, for speed); `generate_square` does one square.

### Elevation (`geography.cpp`, `terrain_from`)

Every noise value is Q16 (65536 = 1.0), from integer value noise with an integer smoothstep
(`noise.hpp`): `value_noise`, `fbm` (octaves, each half the wavelength), `ridged`
(`1 - 2|fbm|`). Lattice corners are hashes of (seed, x, y), so a value never depends on what
else was asked.

```
elevation = continent_bias_m + continent_m x fbm_continent            (60 + 700 x [-1, 1])
if range_mask > range_mask_none:
    m      = min(1, (mask - none) / (full - none))
    crest  = ((ridged + 1) / 2) ^ 4                                   sharp crests, broad valleys
    elevation += range_m x m x crest                                  (up to 1800 m)
elevation += hills_m x fbm_hills                                      (+-160 m)
sea    = elevation < sea_level_m (0)
forest = land and elevation < treeline_m (1700) and fbm_forest > forest_threshold (0)
```

Wavelengths (`GeographyParams`): continents 2^11 cells (about 200 km, 5 octaves); the range
mask 2^12 (2 octaves), so mountains come in bands with lowland between; ridges 2^9 (4
octaves); hills 2^7 (5 octaves); forest 2^8 (4 octaves).

### Water

- **Seas**: every cell below sea level; within a region each 4-connected sea component is a
  `WaterBody` with outline rings.
- **Lakes**: one candidate site per 64-cell (6.4 km) grid square, jittered so its disc (radius
  up to 24 cells) stays inside its square, so no two overlap. A site rolls a lake with a 60%
  chance; the lake fills the hollow round the lowest cell within 16 cells up to a hashed depth
  of 4 to 30 m, never reaching its disc's rim or the sea. A lake is found whole from
  `terrain_at`, so it is the same lake from every square it touches.
- Water bodies carry their outlines as closed rings of cell corners (outer rings positive
  area, island holes negative), ADR-0018 point 2.

Rivers are designed, not built (see below).

## Settlement sites (`settlement_site.hpp`, `settlement_site.cpp`, PEO-096)

**Site cells** of 125x125 geography cells (12.5 km, an eighth of a square) tile the world from
cell 0. Each holds at most one site:

1. A hashed roll with chance 35% (`site_chance_q16`): the owner wanted long empty stretches,
   towns about 20 km apart.
2. A hashed size: hamlet, village, town, city with weights 40/35/20/5 and radii 3, 6, 15, 35
   cells (0.3 to 3.5 km).
3. 16 hashed candidate centres, kept at least 40 cells inside the site cell.
4. Each candidate is rejected if it is water, above 1,400 m, or its disc is under 60% land;
   otherwise scored in integers:

   ```
   score = 500 if sea in the disc + 400 if lake in the disc + 150 if mountains in view
         - 2 x (highest - lowest land in the disc, m)
         - 1 x (centre height above sea level, m)
         + hashed jitter in [0, 60]
   ```

   "Mountains in view" means some land cell in the site cell is at least 900 m above the
   centre.
5. The best candidate (ties to the lower index) is kept if it scores at least -900.

A site reads only its own site cell of the region, so the same seed gives the same sites
whichever region is generated.

## Viewing the map

`peo_geo --seed N --square X,Y [--span K] [--scale S] [--grid] --out f.ppm` draws squares
(about 0.1 s each in release). The defaults in `GeographyParams` and `SiteParams` were chosen
by reading these images; the reasons are in PEO-095's and PEO-096's notes
(`docs/production/notes/`).

## Tunables

Every field of `GeographyParams` (`geography.hpp`) and `SiteParams`
(`settlement_site.hpp`) is a tunable, with its unit in a comment. `valid()` on each says
which combinations the generators accept (for example the lake spacing must be at least
twice the max radius plus one, and the site cell side must divide 1000). The stage levers
are `kBaseDensity`, `kDensityPerStage`, `kMaxDensity` in `stage.cpp`, and the stage size
in `WorldParams` or `--stage`. All values are in [Tunables](tunables.md#the-map).

## Designed, not built

- Rivers: courses on the square (PEO-111, ADR-0010 rivers as lazy long features); sites that
  know their rivers (PEO-112).
- Settlements: the layout generator, outline to streets, blocks and lots (PEO-114); towns
  fitted to rivers and shores (PEO-115).
- Buildings and contents: the building census, category retail and persona houses (PEO-014,
  PEO-113, `docs/design/world-catalog.md`), rooms and loot from `content/world/`, a room slot
  for one item from a list (PEO-064).
- Other stage generators behind `generate_stage`: caves (PEO-005).
- One continuous map with no stage jumps (PEO-118, ADR-0007, D-041), z-levels, and the
  aftermath layer on the global clock (ADR-0009, ADR-0011).
