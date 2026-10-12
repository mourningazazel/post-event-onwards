# Tunables

Every tunable value in one place; each system page repeats its own. "Core default" is what a bare `WorldParams{}`
gives (tests use it); "game" is what `src/app/main.cpp` sets. Paths are under
`src/core/include/peo/core/` unless given in full.

Most of these change what the Dead do, so the swarm scenarios (`test_swarm.cpp`) report new
numbers and the siege baseline moves; a deliberate change rewrites the baseline with its
reason in the commit ([Build and test](build-and-test.md#on-demand-suites)). Constants marked *structural* are baked into slot maths, saves or static_asserts:
changing them is a design change, not a tweak.

## The player and time

| Tunable | Where | Default | Effect |
|---|---|---|---|
| Substep length | `kSubstepMs`, `types.hpp` | 250 ms | *Structural.* Game time per substep |
| Walk / wait cost | `kSubstepsPerWalk`, `types.hpp`; `kStepSubsteps`, `kWaitSubsteps` | 12 substeps (3 s) | *Structural* (ADR-0016) |
| Run cost | `kRunStepSubsteps`, `action.hpp` | 6 substeps | Two cells per update; each deposits slightly less |
| Update period | `WorldParams::update_period` | 12 substeps | How often scent and desire update. Rounded to whole slots |
| Turns a second, taps queued | `TurnInputParams` | 3, 3 | Input pacing (D-032) |
| Start day | `WorldParams::start_day` | 7 | Days since the Event when play starts |

## Scent

| Tunable | Where | Default | Effect |
|---|---|---|---|
| Deposit strength | `WaveParams::strength` | 960 | Reach = strength / (distance + age cost) = 60 cells |
| Distance cost | `WaveParams::distance_cost` | 8 | Falloff per cell of route. Must equal `kWindFull` |
| Age cost | `WaveParams::age_cost` | 8 | Falloff per update; trail slope is distance + age cost |
| Speed | `WaveParams::speed` | 1 | Cells the front advances per update. Above 1, commit runs live |
| Gust rounds | `WaveParams::gust` | 0 core, 2 game | Extra downwind-only rounds outdoors |
| Wind loss | `WaveParams::wind_loss` | 0 | Scent lost upward outdoors per unit of wind |
| Max wind | `WorldParams::wind_max` | 0 core, 8 game | Each stage draws 0 to this |
| Openness | `Stage::openness` | 255 on open cells | 0 indoors: wind does not reach |
| Tile size | `kWaveTileWidth/Height`, `scent_wave.hpp` | 128 x 8 | Performance only |

## The Dead

| Tunable | Where | Default | Effect |
|---|---|---|---|
| Horde size | `WorldParams::initial_dead` | 40 core; 40 per 80x45 in game | Clamped to open cells |
| Speed range | `kMinDeadSpeed`, `kMaxDeadSpeed`, `src/core/src/world.cpp` | 1 to 3 | Updates per move; changes the spawn draws |
| Cycle | `WorldParams::dead_cycle` | 18 substeps (9 slots) | Moves per unit per cycle; every slot hash changes |
| Lean at full scent | `DeadDrawParams::lean_full` | 48 | Sureness on a fresh trail (x64 per trail step at full strength) |
| Lean at the edge | `DeadDrawParams::lean_edge` | 0 | The far Dead's lean (D-045: keep it subtle) |
| Stay | `DeadDrawParams::stay` | 0 | Log-odds of staying versus a level neighbour |
| Company gain | `DeadDrawParams::company_gain` | 4 | Pull toward other Dead (+16 would double a weight) |
| Company radius | `DeadDrawParams::company_radius` | 3 | Box half-size and distance |
| Attrition curve | `WorldParams::attrition`, `kDefaultAttrition` | 5% / 25% / 70% / 85% at 1 week / month / year / 5 years | Who is gone on a revisit |
| Parallel split | `WorldParams::parallel_decide_min` | 16,384 | Performance only |

## The map

| Tunable | Where | Default |
|---|---|---|
| Stage size | `WorldParams::stage_width/height`, game `--stage` | 80x45 core, 200x120 game |
| Wall density | `src/core/src/stage.cpp` | 0.10 + 0.01 per stage, max 0.35 |
| Continents | `GeographyParams::continent`, `continent_bias_m`, `continent_m` | 2^11 cells, 60 m, 700 m |
| Mountain bands | `range_mask`, `range_mask_none`, `range_mask_full`, `range`, `range_m` | quarter to 3/5 of the mask, 1800 m |
| Hills | `hills`, `hills_m` | 2^7 cells, 160 m |
| Forest | `forest`, `forest_threshold`, `treeline_m` | 2^8 cells, 0, 1700 m |
| Sea level | `sea_level_m` | 0 |
| Lakes | `lake_site_spacing_cells`, `lake_chance_q16`, `lake_seed_radius_cells`, `lake_max_radius_cells`, `lake_min/max_depth_m` | 64, 60%, 16, 24, 4 to 30 m |
| Settlement density | `SiteParams::site_cells_per_side`, `site_chance_q16` | 8 per square side, 35% |
| Settlement sizes | `SiteParams::radius_cells`, `weight` | 3/6/15/35 cells, 40/35/20/5 |
| Site scoring | `sea_bonus`, `lake_bonus`, `mountain_bonus`, `mountain_rise_m`, `relief_points_per_m`, `altitude_points_per_m`, `jitter_points`, `min_score`, `max_site_elevation_m`, `min_land_q16` | 500, 400, 150, 900 m, 2, 1, 60, -900, 1400 m, 60% |

## The view

| Tunable | Where | Default |
|---|---|---|
| Sight range | `kOpticalRangeCells`, `sight.hpp` | 152 cells (500 ft) |
| Look jump | `kLookJumpCells`, `look.hpp` | 8 |
| Peek depth | `kPeekDepthPercent`, `src/app/peek.hpp` | 15% |
| View size | `kDefaultViewCols/Rows`, `kMinViewCols/Rows`, `src/app/main.cpp` | 80x45, at least 80x21 |
| Scent overlay bands | `kScentBands`, `src/app/main.cpp` | 10%, 0.1%, 0.001% of the strongest |
| GPU thresholds | `kGpuFieldMinCells`, `kGpuWindyMinCells`, `src/app/main.cpp` | never calm, 512x512 windy |

## Process limits

Rules rather than numbers in code; only the owner may relax them (`docs/roles.md`):

| Limit | Where | Default |
|---|---|---|
| Unit and scenario test budgets | `AGENTS.md` ("under 1 s", "under 5 s"), read by `tools/suite_budget.py` | 1 s, 5 s |
| Doc word caps | `WORD_CAPS` in `tools/validate_docs.py` | see `docs/README.md` |
| Perf budget | D-021, `docs/production/vision.md` | speculate under 100 ms, commit under 1 ms on the M1 Air |
| Bottleneck threshold | `docs/bottlenecks.md` | 20% of speculate or 10% of commit is a row |
