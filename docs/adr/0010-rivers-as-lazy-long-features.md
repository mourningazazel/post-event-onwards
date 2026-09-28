# ADR-0010: Rivers are long features, generated lazily at finer detail

- Status: Accepted (direction from owner note N006). The parameters are proposals.
- Date: 2026-09-27
- Replaces: the "coarse drainage with margins" plan and the rivers risk in ADR-0007 and
  `world-generation.md`.

## Decision

1. **Rivers are not part of top-level geography.** They are produced by a dedicated **river
   generator** when L1/L2 detail is generated for an area.
2. **A river = a source point and a mouth point far apart, joined by a meandering path.**
   - The end at higher macro elevation (sampled from the L0 noise field, no simulation) becomes
     the **source**. The lower end, or a lake, becomes the **mouth**.
   - Both endpoints lie well **beyond** the area being generated, so the river always enters and
     leaves it.
   - The path is a smooth curve through a few control points with seeded wiggle, refined as each
     area along it is generated.
3. **Order-independent, even though it's lazy.** Whether an area *spawns* a river is decided from
   `hash(seed, area)` plus a spacing rule. For example, an area spawns a river only if its roll
   beats the rolls of candidate areas within the spacing radius, and a river is suppressed if
   another candidate's path passes within that radius.
   - Rivers have a maximum length, so any area only checks candidates within that range to learn
     which rivers cross it.
   - Result: the same seed gives the same rivers **whatever order the player explores in**, and in
     all three stages (ADR-0009).
4. **Terrain conforms to the river,** not the other way round. At tile detail, the river carves
   its bed and banks along its path, with a gentle descending bed profile. Width and depth come
   from the river's size class.
5. **Confluences.** If a river's path reaches an existing river, it ends there as a tributary.
   Candidates are processed in hash order, so this stays deterministic.

## Consequences

- The biggest world-generation risk is gone. No drainage computation, no margins.
- Rivers are always continuous across area boundaries, and never depend on exploration history.
- The spacing radius, length range, size classes and meander amount are content data, tuned for a
  realistic density.
