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

## D-038 · How sure-footed the Dead are on a scent trail      (raised by architect · 2026-10-01 · PEO-009)
Why now: PEO-009's draw needs it; the geodesic field (D-024) drops by the same step every cell, so D-007's power law no longer makes the Dead surer near the player.
Affects: gameplay · rewrite: none
- A) Same odds wherever scent reaches: a climbing tile is a fixed few times likelier than a level one, near or far
- B) Surer as the scent gets stronger: near a fresh trail they climb almost every step, at the edge of reach they wander with a lean; two knobs, a gradient between   ← recommended
- C) Greedy wherever there is scent, as today; the draw only shapes wandering where there is none
