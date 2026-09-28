# ADR-0012: Turn-based, with the next turn computed while waiting

- Status: Accepted (owner decisions D-002 and N014). Function names and budgets are proposals.
- Date: 2026-09-28
- Refines: ADR-0005 (player-stepped turns). Does not change its detail rings.

## Context

The world moves only when the player acts. Between actions the machine idles while the player
thinks, yet most of a turn's work (scent diffusion, population drift, the idle Dead's moves) does
not depend on what the player will do next. The owner asked that this time be used so a step
has as little latency as possible.

## Decision

1. **Time moves only through `World::step(Action)`.** No clock drives the simulation.
2. **The next turn is speculated while idle,** assuming the `Wait` action:
   `Speculation speculate(const World&)`, a pure function that does the full-field work.
3. **Input patches the speculation:** `commit(Speculation&&, Action)` applies the action and
   recomputes only what the action can influence: the player's scent deposit and its
   one-cell diffusion footprint, the Dead within the influence radius of the player's new cell,
   any sound event the action emitted. Everything else is adopted as computed.
4. **Equivalence is law:** `commit(speculate(w), a)` equals `step(w, a)` **bit for bit**, for
   every action, on every seed. A golden test enforces it on random worlds every CI run.
5. **Core spawns no threads.** The frontend owns the worker that calls `speculate`, and the
   buffers. Tests stay single-threaded and deterministic.
6. **The renderer draws only the last committed world**, never a speculation buffer (the
   owner's "player view separate from the world's run").
7. **Deeper lookahead** (several `Wait` turns ahead) is allowed only for `Wait`; any other action
   discards speculation deeper than one turn.

## What makes the patch exact

- Scent diffusion is linear, so `diffuse(field + δ) = diffuse(field) + diffuse(δ)`; a deposit
  δ in one cell touches five cells after one step. Non-linear steps (floor clamp, dominance)
  are applied after the patch, in the same order as `step`.
- A Dead's move reads only its eight neighbours' scent and occupancy, so only the Dead within
  two cells of any patched cell (and those touched by a new sound event) can change their mind.
  Their pre-turn state is kept in the speculation so they can be re-run.
- Systems that cannot be made patchable must be marked `player_dependent` and are computed in
  `commit`; the perf budget (a committed turn under 1 ms) is the pressure to keep that set small.

## Consequences

- Every new per-turn system declares whether it is player-independent (speculated) or
  player-dependent (committed), and its influence radius.
- Perf targets restate in turn terms: full turn under 16 ms, committed turn under 1 ms, at
  200×120 with 5,000 Dead (vision.md). The Builder records both on real hardware.
- The playtest harness and replays are unaffected: a replay is a seed plus a list of actions,
  and both `step` and `speculate/commit` reproduce it.
