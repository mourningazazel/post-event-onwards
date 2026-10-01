---
name: perf
description: Measure, profile and report core performance against the D-021 budgets. Use when a brief asks for numbers, a hot path changes, or something feels slow.
---
# /perf (both roles; budget numbers come from the Builder)

A guess is not a measurement. Owner's direction: cramped maps and many Dead
and corpse emitters, so exhaust algorithmic options and spread load across
updates before asking for a mechanics change.

## Budgets

- D-021, release build on the Builder's M1 Air: `commit` under 1 ms,
  speculated update under 100 ms.
- Headless suite (Debug with sanitizers): unit tier under 1 s, enforced by
  `suite_budget`; scenario tier (`scenario*` suites) under 5 s, enforced by
  `scenario_budget` (D-033).

## Measure

1. `python3 tools/verify.py --perf` builds `headless-release` and prints
   the `perf …` lines from the release-only `perf:` cases in
   `tests/core/test_world.cpp`.
2. A new hot path adds a case beside `run_perf`: warm up, take the best of
   N samples, `MESSAGE("perf …")`, and `CHECK` correctness, never time
   (CI runners are too noisy). Keep it inside `#ifdef NDEBUG`.
3. Cover both scales already used (200x120 with 5000 Dead, 512x512 with
   50,000, with and without emitters), plus the case that stresses the
   change: a one-wide corridor jam, the whole horde alerted, the reach
   radius full.
4. Cloud numbers only compare before and after on the same runner. Only
   the Builder's figures go into `docs/architecture.md`.

## Profile before optimising

- macOS (Builder): `xcrun xctrace record --template 'Time Profiler'
  --launch -- ./build/headless-release/tests/peo_core_tests -tc='perf*'`;
  the Allocations template proves a turn allocates nothing.
- Linux (Architect): `perf record -g` on the same binary where the kernel
  allows it, otherwise `valgrind --tool=callgrind` on a small case, and
  `valgrind --tool=massif` for allocation.
- Look, in order, for: work that scales with the map instead of the reach
  or the horde; a per-unit pass that could be one batch pass; allocation
  or hashing inside the loop; pointer chasing or an array of structs where
  one field is read; unpredictable branches in the inner loop.

## Optimise

- Algorithm first: bound work by what changed, update incrementally, and
  spread it over the Dead's 9 s cycle or the 6 s update. Then data layout:
  struct-of-arrays when the profiler asks, smaller integer types, dense
  grids. SIMD and micro-tuning last, and only with a measured win.
- Every change keeps `commit(speculate()) == step()` bit-identical and the
  determinism tests green.
- Core creates no thread or GPU device; it uses the executor and field backend
  it is given (ADR-0014). Serial, threaded and GPU runs must match bit for bit.

## Report

In the item's note, one line per case, before and after:
`perf <w>x<h> dead=<n> emitters=<n> speculate=<us> commit=<us>`, with the
machine and build, and the profiler's top three frames if you profiled. A
number without its case and machine is not a result.
