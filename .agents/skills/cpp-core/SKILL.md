---
name: cpp-core
description: Checklist for writing or reviewing C++ in src/core and src/app (determinism, integer maths, allocation, ownership, API shape). Run it before a unit's commit and in /review's abstraction check.
---
# /cpp-core (both roles)

The C++ rules the build cannot enforce (warnings as errors, sanitizers and
clang-format already run). Read the diff against each heading: for the
Builder a miss is a fix before commit, for the Architect a review finding.

## Determinism (R8, D-002)

- Randomness comes only from a passed `Rng` or a hash of (seed, cycle,
  index). Never `std::rand`, `std::random_device` or `<random>`
  distributions: their output differs between standard libraries.
- No wall clock, thread timing or pointer values in anything that feeds the
  simulation.
- Never iterate an `unordered_map` or `unordered_set` to produce a result;
  its order is library-defined. Sort, or use a dense vector keyed by id.
- Integer maths for anything that feeds back into the simulation. Floats
  are for display and reporting. Core builds with `-ffp-contract=off`
  (PEO-028) because arm64 would otherwise fuse `a + b * x` into one FMA and
  round differently from x86; keep that flag on any new core target.
- New state is covered by `World::equivalent` and the
  `commit(speculate()) == step()` test, or the test says why not.

## Integer arithmetic

- Signed overflow is undefined, and UBSan sees only the paths tests hit.
  Pick a type with headroom and write the worst case next to it
  (`// max: 60 cells x 8 + 255`), or saturate.
- Convert signed and unsigned once, at the boundary, with `static_cast`.
- Division and right shift of negatives round toward zero or down; say
  which you meant.

## The per-tick path

- `step`, `speculate`, `commit` and everything they call allocate nothing
  after warm-up: buffers live in their owner and are cleared, not rebuilt
  (see `Speculation`).
- No `std::function`, string building, `shared_ptr` copies or exceptions on
  that path. Pass `std::span`, not containers by value.
- Things that come and go (the Dead, deposits, sound events) reuse slots in
  dense arrays, addressed by index or generational handle (R5). That is
  this project's object pool; no `new`/`delete` per event.
- One pass over the horde beats one call per unit; no virtual call per
  entity per step (R4).

## Ownership and types

- Values and dense arrays first. `std::unique_ptr` only where a seam needs
  a heap object; no raw `new`/`delete`, no `shared_ptr` in core.
- SDL handles in `src/app` sit in an RAII wrapper, so an early return
  cannot leak them.
- Kinds and ids are enums or registry ids; strings are for loading content
  and are never compared at runtime (R6).
- Behaviour states (calm, alerted) are an enum and a `switch` over a dense
  array, not a class per state.
- `const` on everything that does not change, `[[nodiscard]]` where the
  result is the point, `constexpr` for fixed tables.
- Templates and concepts only when a second real caller exists.
- Core does not throw. A call that can fail returns `std::optional` or an
  error enum, and the caller handles it.

## Tunables and tests

- A number with meaning is a named constant or a field in a `*Params`
  struct with its unit in the name (`step_seconds`). Prefer a scale over a
  switch between a few fixed cases (owner's direction).
- Each behaviour gets a doctest in `tests/core/test_<header>.cpp`, named as
  a behaviour, with the edges: map border, wall corners, zero and maximum
  values, an empty horde.
- A changed hot path gets a `perf:` case (see `/perf`).
