# Decisions needed

The user's inbox. Both agents append here whenever a choice is about
**gameplay, engine abstraction, or project direction**, or when a useful
change would cause a **big rewrite**. Technical method (how to implement an
agreed thing) is never asked here; agents decide that themselves.

Answer by replying in chat with the id and a letter, e.g. `D-002: B`, plus any
notes. The agent that reads the answer moves the entry to `decisions.md` and
unblocks the related queue item. Newest at the top. Entry format is fixed:

```
## D-000 · short title                              (raised by architect|builder · YYYY-MM-DD · PEO-xxx)
Why now: one sentence.
Affects: gameplay | abstraction | direction · rewrite: none | small | large
- A) option — one line of consequence   ← recommended
- B) option — one line of consequence
```

---

## D-052 · Can the Look cursor go past the edge of the screen?      (raised by architect · 2026-10-08 · PEO-120)
Why now: sight reaches 152 cells (D-049) but the player-centred view (D-048) shows far fewer, so the Look cursor (PEO-120) either stops at the screen edge or the view must move while looking; binoculars (later) widen the gap.
Affects: gameplay · rewrite: none
- A) Screen-bound: the cursor rests only on seen cells that are on screen; the view never moves while looking — what you can inspect is what you can see on screen
- B) Pan while looking: the cursor reaches any seen cell; when it would leave the screen the view scrolls with it, and snaps back to the player when Look ends — the whole sight range (and later binoculars) can be inspected   ← recommended

## D-051 · Does the map remember terrain the player has seen?      (raised by architect · 2026-10-08 · PEO-119)
Why now: PEO-119 limits the view to what the player sees now (D-049), so a cell that drops out of sight (round a corner, behind a wall) goes dark at once; whether it stays drawn decides what the frontend keeps per stage and what a save carries.
Affects: gameplay · rewrite: none
- A) Remember seen terrain: cells once seen keep their walls and floor drawn dimmed, with no Dead, items or scent on them until seen again — a per-stage seen grid in core, saved with the stage   ← recommended
- B) Only what is seen now: anything out of sight draws dark — nothing kept, the player holds the layout in their head
