# ADR-0016: Twelve substeps per walking turn; a walking turn is 3 game seconds

- Status: Accepted (owner note N023). The cost of long actions is D-039.
- Date: 2026-10-03
- Refines: ADR-0005 (integer time units) and amends D-015 (walk 6 s, run 3 s, update every 6 s)
  and D-031 (9 s Dead cycle). ADR-0012 and ADR-0014 are unchanged.

## Context

Today a walking turn is 6 game seconds, and the clock counts whole seconds: six time slots per
turn. The Dead take their moves in those slots (D-031) and the heavy update runs once per turn.
The owner wants room to put finer-grained events between today's slots, for special units,
processes and actions, without changing how anything behaves now, and wants a walking step to
read as 3 seconds, which is closer to a real walk across one metre.

## Decision

1. **The clock counts substeps.** One walking turn is **12 substeps**. Today's six slots sit on
   the even substeps (0, 2, ..., 10); the odd substeps are the **between layer**.
2. **Everything that exists today runs on even substeps only,** in the same order, with the same
   counts per turn: the heavy update once per walking turn, the Dead's slots as now (a 9-slot
   cycle becomes 18 substeps), a running step every 6 substeps. A seed and an action list give
   the same world as before, turn for turn; a golden test pins it.
3. **The between layer is opt-in.** Only content or systems the owner designates use odd
   substeps: special units with speeds between today's steps, timed processes, small deferred
   work moved off the main slots for performance. Nothing moves there by default.
4. **A walking turn is 3 game seconds** (a substep is 250 ms). This is a label on time, not a
   change in behaviour per turn: the update cadence, the Dead's speeds and scent's spread and
   fade per turn stay as they are.
5. **What keeps its hours and days:** anything stated in real time keeps that meaning and so
   takes twice as many turns as before: sleeping, waiting for hours, reading, searching and
   crafting times, soak half-life, rot, spoilage, the event clock (days since the event) and
   the revisit gate. Content durations are authored in seconds, minutes and hours and converted
   to substeps once, at load.

## Consequences

- The game-time type changes from whole seconds to substeps across core (`types.hpp`,
  `action.hpp`, `dead.hpp`, `scent_wave.hpp`, `world.hpp`), the app and tests: a large but
  mechanical change (PEO-090).
- Per turn, cost is unchanged. Per game hour, twice as many updates run, so sleeping 8 hours
  costs twice today's updates (D-039); PEO-082 (fast time passing) owns that cost.
- The scent age line's 2^27-update bound drops from about 25 to about 12 game years: still far
  beyond one stage's life (`scent_wave.hpp`).
- The siege suite's 1 to 48 game-hour checkpoints keep their hours, so it runs twice the updates.
