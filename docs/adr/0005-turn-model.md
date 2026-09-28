# ADR-0005: Player-stepped turns with level-of-detail simulation

- Status: Accepted
- Date: 2026-09-27

## Decision

- **The player acts in discrete steps.** The world advances in response to player steps.
- **Not everything in the world is simulated every step.** Simulation fidelity follows the
  detail rings of ADR-0007:
  - **Full:** individual actors every step, near the player.
  - **Reduced:** lower cadence or cheaper rules, further away.
  - **Aggregate:** groups and hordes as single objects, beyond that.
- **Time is tracked in integer time units.** A timing-wheel scheduler handles actors, effects and
  timers. Actors due on the same tick are processed as a batch.

## Consequences

- Each step has a time budget. The Budget Manager (see `SYSTEMS.md`) enforces it, by expanding
  or contracting simulation rings.
- Promoting an aggregate group to individuals and demoting it back must preserve its state (count,
  composition, wounds) deterministically.
