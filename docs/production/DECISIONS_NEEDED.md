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

## D-045 · the far Dead's lean is invisible in play          (raised by architect · 2026-10-06 · PEO-101)
Why now: the PEO-101 playtest could not see far Dead lean toward the player at seed 1 density, though the swarm test shows it (55% of moving draws climb beyond 45 cells) and near Dead converge faster (15 vs 11 within 3 cells after 80 waits).
Affects: gameplay · rewrite: none
- A) keep the D-038 knobs; a subtle lean at the edge of reach is intended   ← recommended
- B) raise the edge-of-reach sharpness knob so the lean shows; a tuning item
- C) revisit after a denser horde or bigger stage is played (PEO-083)
