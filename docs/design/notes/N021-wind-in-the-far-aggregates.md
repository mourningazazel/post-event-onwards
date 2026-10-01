# N021 — Wind in the far aggregates

- Date: 2026-10-01
- Source: owner, in the Architect review thread, after PEO-048 (wind) passed review

> perfect. Keep this in mind. Currently outside the active scent grid there are aggregates for
> the scent values. I would like it, now that we have Wind, for that to play a part in those
> aggregates spreading across the outer map. Since we are offloading more on GPU now, would it
> make sense to do the wind calculation in a separate buffer, and then include it in the
> dispersion calculation? I'm asking it this way, because is there any way we can do that in
> parallel to the dispersion? It might not be the same level of accuracy, but if we could do the
> wind spread and the general dispersion calculations in tandem, and then add the result, would
> that save compute?

## Architect's answer, as given in the thread

- The far aggregates are not built yet; PEO-051 (section view) plans them, so wind goes into
  their design from the start.
- **Far layer: yes.** It is a coarse, smooth (linear) field, so spread and wind drift can both be
  computed from the same "before" buffer and their changes added (additive operator splitting:
  first-order, slightly less exact than applying them in order, fine at aggregate scale). The
  saving is fusing both into one pass over each cell and its neighbours: the pass is
  memory-bound, so wind costs close to nothing there, on the CPU or the GPU.
- **Detailed field near the player: no.** The geodesic field is a best-route maximum, not a sum;
  wind changes step costs along a route, so a windy-only and a calm field cannot be added back
  together without losing mixed routes. Its 2-5x wind cost on the CPU is the gust rounds, which
  the GPU absorbs.
- **What runs in tandem:** the detailed field and the windy far layer are independent jobs that
  exchange values at their boundary once per update, so they can run side by side (D-035
  executor, D-036 GPU).
