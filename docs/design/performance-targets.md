# Performance targets: horde size and active map by machine

Architect analysis, 2026-09-30, for D-034. The measurements were taken in the cloud container (Xeon at 2.1 GHz,
`headless-release`, single thread). Tier figures are **estimates** from public
single-thread and memory benchmarks. They are not measurements. PEO-071 and PEO-072 replace
them with real numbers.

## What costs time

The simulation runs on one thread, on the CPU, with integer fields (D-021). The GPU only
draws the view, and a viewport of about 80x45 tiles shows a few thousand Dead at
most. A GTX 1080 and an RTX 4070 Super give the same horde ceiling. Three things
scale independently:

| axis | what grows it | bound by | container cost |
|---|---|---|---|
| horde | Dead that decide per update | memory latency (scattered reads of 9 scent cells plus occupancy) | 100 ns per Dead per update when the map fits in cache, 150 ns when it does not |
| active area | scent cells re-propagated per update | cache, then latency | 43 ns per active cell (512²), 53 ns (1024²), 73 ns (2048²) |
| stage size | the whole wave copied per speculate | memory bandwidth | 0.9 ms at 1024², 3.1 ms at 2048², 26 ms at 4096² |

Commit, which runs on the input path, replays the recorded Dead moves. It takes about
2-5 ns per Dead, with a worst case of 2 ms at 400k Dead. The container runs about 1.4x slower than the M1 Air.
That ratio comes from PEO-030's M1 medians against the same cases here: 200x120 with 5,000 Dead,
and 512x512 with 50,000 Dead.

Horde and active area are Ryan's "two different hardware limits". Horde size is
limited by latency and core speed. Active area is limited by cache size, which is why a
large L3 matters. Stage size is limited by bandwidth.

## A flaw in the current perf lines

The `perf:` cases report the best of 10 samples. With standing emitters, the field
takes about 13 updates to fill its reach, so the best sample is an early, empty
field. At steady state, 512² with 50,000 Dead and 6 emitters per 1000 tiles costs
**15.8 ms median, 23 ms worst**. The best sample reads 1.0 ms. Any emitter density
above about 1 per 1000 tiles makes every reachable cell active on every update, because a
standing emitter re-propagates its whole reach each time. The D-021 table is still
inside budget (about 11 ms on the M1), but it has about 10x less headroom than it shows.
PEO-070 changes the harness to report steady state.

## Targets (Ryan, 2026-09-30)

A Mac tier plus one build each for low, medium and high:

| tier | CPU | memory | GPU | vs M1 (CPU / bandwidth) |
|---|---|---|---|---|
| Mac | M1 Air | 8 GB LPDDR4X | integrated | 1.0 / 1.0 |
| Low | i5-8400 or i5-9400F, 6 cores, 9 MB L3 | 16 GB DDR4-2666 | GTX 1080 | ~0.6 / ~0.25 |
| Medium | i5-12400F or Ryzen 5 5600X, 18-32 MB L3 | 16-32 GB DDR4-3200 | RTX 3060 or 4060 | ~1.05 / ~0.45 |
| High | i7-13700K or 14700K, 30-33 MB L3 | 32 GB DDR5-6000 | RTX 4070 Super | ~1.3 / ~0.6 |

These are the common builds on today's hardware surveys: 6-core i5 or Ryzen 5 CPUs, 16 GB of RAM,
and 60-class GPUs. The M1 has unusually high single-core bandwidth, so every PC tier
copies large grids more slowly than the Mac, even when its core is faster.

## Estimated ceilings

The budget is D-021's: a speculated update under 100 ms and a commit under 1 ms. The
estimates assume a 1024² stage and a 256x256 active scent window, fully saturated (PEO-047
bounds scent by reach). "Planning Dead" means four times today's decision cost, which
leaves room for hearing, soak, rot and pushing. That multiplier is an assumption.

| tier | today: speculate limit | today: commit limit | planning Dead: speculate limit | active side with 100k planning Dead |
|---|---|---|---|---|
| Low | ~500k | ~200k | ~125k | ~560 cells |
| Mac | ~900k | ~350k | ~230k | ~1150 |
| Medium | ~970k | ~375k | ~240k | ~1200 |
| High | ~1.2M | ~475k | ~310k | ~1450 |

Today the limit is commit, not speculate. Richer Dead move the limit to speculate.
Ryan accepts sizes that would run slower than required on the Mac. Past the 100 ms figure,
the hard wall is the 333 ms between walking steps (D-011, D-015: one update per walked tile).
That is about 3x the ceilings above, before a held key visibly stalls.

## Levers before any mechanics change

These follow the owner's direction: algorithms first, and spread the load.

1. **Commit swaps instead of replaying.** Between updates the Dead never read the
   player (D-031), so at the boundary the speculated horde should already be the answer
   (to verify: nothing the player does between updates may change it). Swapping it in makes commit O(1). This removes today's binding limit.
2. **Speculate copies only what changes.** Copying the whole wave makes stage size a
   per-update cost. Copying only the dirty cells, or double-buffering, makes it proportional to the active area.
3. **Standing emitters become a static layer.** An unchanged emitter at steady state
   gives a fixed field relative to the age line. Propagating it once removes the saturation cost.
4. **Parallel Dead decisions.** Within one second, decisions read only the last
   update, so they can be split across cores deterministically. A conflict pass in index order
   handles reservations. This is 3-5x on the 6-core tiers, but it changes ADR-0012
   ("core spawns no threads"), so it goes to Ryan as a decision when a tier misses its target.

## Auto-config (D-034)

This is not a set of presets. The game runs a short calibration and places two
continuous sliders on the machine's own curve:

- **Calibrate** (under 2 s at first launch and after a hardware change). The same
  kernels as `peo_bench` time each axis separately: ns per Dead at two horde sizes,
  ns per active cell at two window sizes, and ns per stage cell for the copy. The fit
  is `update = a + b x dead + c x active_cells + d x stage_cells`, per machine.
- **Choose.** Solve for the largest horde and active window that keep the fitted
  update under the budget with a margin, weighted by a player preference ("bigger hordes"
  against "wider world"). Both are real numbers. Nothing snaps to a tier.
- **Adapt.** Every update reports its measured time. A slow controller nudges the
  active window by a few percent when the rolling p95 drifts from target. The horde is
  fixed once a stage is generated, so drift moves only the window, never the Dead
  already placed. Determinism holds because the settings are inputs to a
  run and are recorded with the seed (PEO-006 replay).

The tiers above become test points, not settings: each real machine run in PEO-072
checks that the calibration's predicted ceiling matches the measured one within 20%.

## Test plan and the suite budget

- `peo_bench` (PEO-070) is a separate release-only binary. It is not a ctest case, so
  **the 1 s unit suite and D-033 are unaffected**. CI runs one tiny smoke sweep for
  correctness only, never timing, because shared runners are too noisy.
- The sweeps measure each axis alone and then together. Horde: 10k to 1M. Active side:
  64 to 1024. Stage: 512² to 4096². The sampling runs past saturation and covers whole
  18 s cycles (LCM of the 6 s update and the 9 s Dead cycle), and reports median, p95 and worst.
- `--ceiling` bisects the largest horde, and the largest active window, that fit
  the budget on the machine it runs on. That is the number each tier needs, and the
  number auto-config must predict.
- Machines: the Builder's M1 Air (PEO-071), then one real PC per tier (PEO-072).
