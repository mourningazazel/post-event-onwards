# ADR-0011: One global "days since event" clock, and per-world event parameters

- Status: Accepted (owner note N007). Curves and parameters are proposals.
- Date: 2026-09-27
- Supersedes: the "stages are fixed per run" part of ADR-0009. ADR-0009's
  baseline → event → aftermath passes and monotonic threshold sampling still apply.

## Decision

1. **Per-world event parameters,** rolled from the world seed and shown on the opening splash
   screen:
   - the **moment of the event**: time of day and day of the week. This decides where people
     were: homes at night, workplaces and schools by day.
   - the **turned fraction**, from **80% to 99%**. A lower fraction means more survivors, which
     scales up:
     - struggle damage
     - looting and scarcity
     - barricades
     - notes and encampments
     - **dead bodies** (survivors who died)
2. **The game clock *is* days-since-event.**
   - Every aftermath effect is a continuous function of it: Dead leaving buildings,
     attrition, looting, spoilage, damage, **overgrowth**, **environmental damage**, utilities
     failing, dead tissue fragility, fire aging.
   - **Difficulty selection sets the starting clock:** 7, 30 or 365 days. No code branches on a
     stage.
   - Curves must behave sensibly for any value, e.g. 5 years (1,825 days): they saturate rather
     than overflow.
3. **An area generates at the clock value current when it is first generated.** A 1-week start
   that has played for a year meets year-old new areas.
4. **Revisits: stored areas stay as they were, unless the player has been away about a month**
   (owner note N009).
   - Within the gate (default **30 days** of game time since the last visit), a stored area loads
     exactly as it was.
   - Beyond the gate, it is brought forward from its stored clock to `t_now`, using the same
     per-object thresholds. Monotonic sampling makes this identical to generating it at `t_now`.
   - Player-made changes are preserved (confirmed by the owner in N011).
   - The gate is a data value.
5. **Dead fragility is a function of the clock** (gated by months). Damage output stays
   similar, but limbs and appendages become easier to damage and sever. Together with falling
   supplies, this gives the intended **difficulty ↔ reward gradient**: early is harder but
   richer, late is easier but poorer.

## Consequences

- One number explains the whole world state. Switching "stages" is a start setting, and the
  gradient continues naturally during play.
- Saves record the world event parameters, the current clock, and the last-updated clock per
  stored area.
- Every aftermath curve lives in content data. It is tested at 7, 30 and 365 days and at an
  extreme (1,825 days), and must be monotonic (purpose P-WO-17).
