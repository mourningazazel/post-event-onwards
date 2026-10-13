# API reference

Every public header in `src/core/include/peo/core/`, namespace `peo::core`, grouped by job.
This is a map, not a copy: each header's own comments are the contract and say more. The
"engine API" for modders and scripting is a separate, future thing
(`docs/domains/engine-api.md`, intent only).

## Using the simulation: a minimal driver

```cpp
#include "peo/core/world.hpp"
using namespace peo::core;

WorldParams params;                 // defaults: 80x45, 40 Dead, calm
params.stage_width = 200;
params.stage_height = 120;
World world(/*seed*/ 42, params);

world.step(Action::step({1, 0}));   // walk east, 12 substeps
world.step(Action::wait());         // wait 12 substeps

Speculation spec;                   // keep one and reuse it
world.speculate(spec);              // may run on another thread; World must not change meanwhile
world.commit(spec, Action::step({0, 1}, kRunStepSubsteps));   // same result as step()

for (const Dead& d : world.horde()) { /* d.pos */ }
std::int32_t s = world.scent().sample(world.player());
```

That is the whole loop the frontend runs. Tests use the same calls, plus `load_layout` for
hand-built stages and `World::equivalent` to compare two worlds.

## The World (`world.hpp`)

| Symbol | Kind | What |
|---|---|---|
| `WorldParams` | struct | Tunables: `initial_dead`, `scent` (`WaveParams`), `stage_width/height`, `update_period`, `dead_cycle`, `wind_max`, `parallel_decide_min`, `draw` (`DeadDrawParams`), `start_day`, `attrition` |
| `World(seed, params)` | ctor | Clamps params (sides at least 3, periods rounded to slots, invalid curve replaced) and loads stage 0 |
| `load_stage(index)` | | Generated stage `index`; resets turn, stage clock, scent, horde |
| `load_layout(stage, player, horde)` | | A hand-built stage, no exit |
| `step(action)` | | Spend one action live |
| `speculate()` / `speculate(Speculation&)` | const | The next update ahead |
| `commit(Speculation&, action)` | | Spend an action using it; bit-identical to `step` |
| `deposit(at, strength)` | | Test hook for emitters; not a mechanic |
| `save(build)` / `restore(image, issues[, report])` | | Save sections and back; `report` gets the content migration ([Saves](persistence.md)) |
| `store_area()` / `resume_area(record)` | | Leave and return to a stage, thinned |
| `remember(sight)` | | Fold sight into map memory; returns cells newly remembered |
| `set_executor(Executor*)`, `set_field_backend(FieldBackend*)`, `set_content(const Content*)` | | Lend threads, a GPU or content; not owned |
| `stage()`, `params()`, `scent()`, `desire()`, `wind()`, `horde()`, `player()`, `memory()` | const reads | |
| `turn()`, `substeps()`, `game_ms()`, `updates()`, `since_event()`, `stage_index()`, `occupancy()` | const reads | Counters and clocks |
| `equivalent(a, b)` | static | Every piece of state equal; the golden tests' oracle |
| `HordeState`, `DeadMove`, `Occupancy`, `Speculation` | structs | The Dead between updates; one decided move; the occupancy log; the speculated update |

## Time and input

| Header | Symbols |
|---|---|
| `types.hpp` | `Vec2i`, `kNeighbours8` (N, NE, E, SE, S, SW, W, NW), `kNeighbours4`, `Substeps`, `Slot`, `Seed`, `Tick`, `kSubstepMs`, `kSlotSubsteps`, `kSubstepsPerWalk`, `kUpdatePeriodSubsteps`, `kSubstepsPerHour`, `to_substeps`, `ms_to_substeps`, `to_ms` |
| `action.hpp` | `ActionKind {Step, Wait}`, `Action::step(dir, substeps)`, `Action::wait(substeps)`, `kStepSubsteps`, `kWaitSubsteps`, `kRunStepSubsteps` |
| `turn_input.hpp` | `TurnInput` (`offer(action, repeat, now)`, `take(now)`, `next_due()`, `clear()`), `TurnInputParams` |

## Scent and wind

| Header | Symbols |
|---|---|
| `scent_wave.hpp` | `ScentWave` (`deposit`, `update`, `patch_deposit`, `sample`, `strongest_neighbour`, `flow_target`, `sync_from`, `set_wind`, `set_executor`, `set_field_backend`, `restore`, `use_kernel`, `values`, `changed_tiles`, `active_cells`), `WaveParams`, `WaveKernel`, `max_wave_updates`, `kWaveDistanceCost`, `kWaveAgeCost`, `kWaveReachCells`, `kWaveUnreached` |
| `wind.hpp` | `Wind {toward_degrees, intensity}`, `WindTable`, `wind_table(wind, loss)`, `cos_degrees`, `kWindFull`, `kMaxWindStep`, `kOpennessOutdoors`, `kOpennessIndoors` |
| `field_backend.hpp` | `FieldBackend::run(const FieldRounds&)`, `FieldRounds` (the views a GPU backend reads and writes) |

## The Dead

| Header | Symbols |
|---|---|
| `dead.hpp` | `Dead {pos, step_substeps, fate}`, `DeadState`, `kDeadStateNames`, `SlotPlan`, `plan_slots`, `slot_second`, `draw_word`, `draw_choice`, `decide_move`, `kDeadCycleSubsteps` |
| `desire.hpp` | `DesireField` (`build`, `rebuild`, `weights`, `log_odds`, `restore_log_odds`), `DeadDrawParams`, `weight_of`, `kOctave`, `kDrawChoices`, `kStayChoice` |

## Stages and the map

| Header | Symbols |
|---|---|
| `stage.hpp` | `StageSpec`, `Stage {spec, blocked, openness, entry, exit}`, `stage_seed`, `stage_spec`, `generate_stage`, `kNoExit` |
| `geography.hpp` | `GeographyParams`, `terrain_at`, `TerrainSample`, `generate_geography(seed, params, rect)`, `generate_square`, `GeographyRegion {rect, elevation_m, cover, body, bodies}`, `WaterBody`, `CellCorner`, `SquareCoord`, `square_of`, `square_rect`, `kGeoCellTiles`, `kSquareCells` |
| `settlement_site.hpp` | `SiteParams`, `place_sites`, `score_site`, `site_candidates`, `site_cell_rect`, `SettlementSite`, `SiteSize`, `Landmark` |
| `noise.hpp` | `value_noise`, `fbm`, `ridged`, `Octaves`, and the scanning forms `NoiseScan`, `FbmScan`, `NoiseRow`, `FbmRow` (same bits, faster in rows); `kNoiseOne` (Q16) |

## Seeing

| Header | Symbols |
|---|---|
| `sight.hpp` | `SightField` (`compute(opaque, origin, params)`, `seen`, `window`, `origin`, `range`), `SightParams`, `CellRect`, `kOpticalRangeCells` |
| `content.hpp` | `load_content(sources)`, `ContentSource`, `ContentLoad`, `Content` (`item`, `item_count`, `item_id`, `name`, `parent`, `ancestry`, `abstract`, `tags`, `has`, `int_at`, `string_at`, `has_record`, `remap`, `remap_count`, `for_each_leaf`), `ItemId`, `ContentLeaf`, `kJunkItemId` |
| `memory.hpp` | `MapMemory` (`reset`, `remember(sight)`, `remembered`, `mark`, `count`, `cells`) |
| `look.hpp` | `LookCursor` (`begin`, `end`, `step(dir, cells, sight, bounds)`, `cell`, `active`), `LookReport`, `look_at`, `legible`, `kLookJumpCells`, `kLegibleRangeCells` |

## Persistence and the long clock

| Header | Symbols |
|---|---|
| `content_migration.hpp` | `ContentRef`, `Migration`, `Resolved`, `resolve`, `content_ref`, `MigrationReport`, `migration_summary`, `kMaxSavedAncestry`, `kMaxContentIdBytes`, `kMaxContentNameBytes` |
| `save.hpp` | `SaveImage`, `SaveSection`, `SyncScope`, `SaveStatus`, `SaveIssue`, `SaveRead`, `write_save`, `read_save`, `section_id("WRLD")`, `section_name`, `crc32`, `kSaveFormat` |
| `byte_io.hpp` | `ByteWriter`, `ByteReader`: little-endian fields, bounds-checked reads |
| `aftermath.hpp` | `Curve`, `CurveKnot`, `affected_at`, `valid`, `normalized`, `draw_fate`, `kDefaultAttrition`, `EventSubsteps`, `kSubstepsPerDay`, `kAffectedScale` |
| `revisit.hpp` | `AreaRecord`, `thin_horde` |

## Foundations

| Header | Symbols |
|---|---|
| `rng.hpp` | `Rng` (xoshiro256\*\*: `range(lo, hi)`, `unit()`, `chance(p)`, `reseed`, `state`, `from_state`), `splitmix64`, `hash_u64(a, b, c)`, `hash_unit(a, b, c)` |
| `grid.hpp` | `Grid<T>`: dense row-major, `at(x, y)`, `in_bounds`, `fill`, `data`; `Grid<bool>` is one byte per cell |
| `executor.hpp` | `Executor` (`run(pieces, body)`, `width()`), `SerialExecutor`, `PieceFn`, `run_pieces`, `run_ranges` |

## Outside core

- `src/gpu/gpu_field.hpp`: `peo::gpu::GpuFieldBackend`, `create_compute_device()`.
- `src/app/*.hpp`: `peo::app::ThreadPool`, and the camera, HUD, peek and glyph-atlas helpers.
  Header-only and SDL-free, so tests include them directly.

## Adding to the API

The repo's rule (`docs/roles.md`): a new public header or a change to one the frontend uses is
an **abstraction** decision. In the agent workflow that stops in `DECISIONS_NEEDED.md` for the
owner. Technical method inside an agreed interface does not. One concept per header, a
`TEST_SUITE` per header in `tests/core/test_<name>.cpp`, tunables in a `...Params` struct with
named defaults.
