# ADR-0019: Revisited areas catch up to the clock, player-made things included, one area at a time

- Status: Accepted (owner note N023). Whether a minimum time away still applies is D-040.
- Date: 2026-10-03
- Refines: ADR-0011 point 4 (revisits) and ADR-0009 (monotonic aftermath). Supersedes ADR-0011's
  "player-made changes are preserved" (N011): they now age too.

## Context

ADR-0011 brings a stored area forward to the current clock when the player returns after the
30-day gate, but keeps the player's changes as they were. The owner wants an area that has
dropped out of the active range to answer to the clock like the rest of the world: its horde
thinned, other (never seen) survivors searching what the player left unsearched, and the
player's own barriers, seals and caches wearing, failing or being broken into. It must never
cause a world-wide "new day" pass, now or after years of play over a large area.

## Decision

1. **Catch-up is lazy and per area.** An area is brought forward only when it is loaded again
   (or about to be, by the streaming rings), from its stored clock to `t_now`. Nothing runs at
   midnight or on any global tick; areas the player never returns to cost nothing.
2. **Bounded cost.** Catch-up is closed form, not stepped: each object and unit has its seeded
   thresholds (ADR-0009), so the state at `t_now` is a comparison per effect, the same work
   whether the player was away a day or ten years. It runs as a streaming job, split across
   frames, before the area enters the detailed ring.
3. **The horde thins naturally.** Each of the Dead in a stored area keeps its attrition
   threshold; the area's count is brought to what the attrition curve leaves between the two
   clocks. Dead the player drew in stay subject to the same curve.
4. **Player-made and player-changed things age on their own, gentler curves,** starting from
   when the player last touched them, not from the event: barricades lose strength then fail,
   a sealed building may be broken into (a door forced, a window broken), stashed or dropped
   items may be taken by unseen survivors. Curves live in content data beside the aftermath
   curves and saturate (ADR-0011 point 2).
5. **What is not regenerated:** terrain, buildings, room layouts, containers and their purposes.
   Unopened containers already resolve on access at the current clock (D-023), so "searched by
   someone else" needs no extra pass.
6. **Monotonic for everything:** an object broken at one return is still broken at every later
   one.

## Consequences

- Stored records gain a `last_touched` clock for player-made or player-changed objects; areas
  keep their last-updated clock (ADR-0011 consequences).
- A purpose test pins it: leaving and returning repeatedly gives the same state as one long
  absence of the same total length.
- PEO-094 builds it.
