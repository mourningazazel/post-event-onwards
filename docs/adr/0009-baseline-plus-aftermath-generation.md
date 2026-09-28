# ADR-0009: World generation = pre-event baseline + event + time-driven aftermath

- Status: Accepted (direction from owner note N005). The parameters are proposals.
- Date: 2026-09-27

## Context

The game has three difficulty stages, which are three world states: **1 week, 1 month and
1 year** after an instant, simultaneous turning event. Later stages have fewer Dead, fewer
supplies, and more spoilage and damage. Switching between stages must be easy, and world
generation must account for it everywhere, especially in buildings and items.

## Decision

Every generator (L0–L5, ADR-0007) produces the world in **three conceptual passes**:

1. **Baseline:** the world the instant before the event.
   - Buildings as built and furnished, fresh food, intact windows.
   - Occupants where they were at that moment.
   - Doors in their normal state, vehicles in use.
   - **Identical for a given seed in every stage.**
2. **Event:** the instant of turning.
   - Everyone turns except about 1 in 100. *(Refined by ADR-0011: the turned fraction is rolled
     per world, from 80% to 99%.)*
   - Each non-turned person generates a **struggle trace**: remains, forced doors and windows,
     dropped belongings, blood.
   - Moving vehicles crash or stop.
   - **Also identical in every stage.**
3. **Aftermath:** a pure function of `elapsed_days`. It covers:
   - Dead leaving buildings through open paths
   - dead attrition
   - looting by the other survivors, prioritized by building and container type
   - spoilage by shelf life
   - damage and decay
   - aging of traces

The **stages are presets** of `elapsed_days` (7 / 30 / 365). The aftermath is continuous, so a
future stage (e.g. 6 months) is only a new preset.

### Monotonic by construction

Each object gets one seeded random threshold *u* in [0, 1) per aftermath effect. Its state at
time *t* is "affected" if `P_effect(t) > u`, where `P_effect` rises with time.

As a result, anything broken, looted, spoiled or emptied at 1 week is **also** broken, looted,
spoiled or emptied at 1 month and at 1 year. The three stages of one seed are consistent views of
the same world at three moments. Switching stage is just a different `elapsed_days`; no
generator code branches on the stage.

### Data-driven

Aftermath behaviour lives in content data:

- **Items:** shelf-life curves.
- **Fittings:** break curves (e.g. windows).
- **Buildings and containers:** loot priorities.
- **Occupants:** leave-rates for Dead, depending on how open the building is.
- **Global:** attrition curves.

## Consequences

- Stage switching is cheap and coherent, and purpose tests can compare stages of one seed
  directly: supplies and dead counts must fall monotonically.
- Saves record the stage (`elapsed_days`) with the world seed, and it never changes for a run.
  *(Superseded by [ADR-0011](0011-global-event-clock-and-world-event.md): the clock advances
  during play, and areas generate at the current clock.)*
- Frozen, instantiated content (ADR-0002) is frozen *after* the aftermath pass, so what the
  player sees is exactly the stage's state.
- **No NPC systems are needed** (N005). The other survivors exist only as aftermath effects and
  traces.
