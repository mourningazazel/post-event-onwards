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

## D-047 · How much map the screen shows                (raised by architect · 2026-10-06 · PEO-117)
Why now: the view becomes player-centred and stages grow past the window.
Affects: gameplay · rewrite: none
- A) Whatever fits the window, resizable; a bigger screen shows more — sight limits come later from line of sight   ← recommended
- B) A fixed 80 x 45 cells for everyone; the window scales it — same view on every screen

## D-048 · How the screen follows the character          (raised by architect · 2026-10-06 · PEO-117)
Why now: decides whether the frontend draws only on change or every frame.
Affects: gameplay · rewrite: none
- A) Jumps one cell per step, the character always dead centre   ← recommended
- B) Glides smoothly over each step — needs per-frame drawing, a later polish item
- C) Moves only when the character leaves a middle box

## D-049 · What the screen does at the map's edge        (raised by architect · 2026-10-06 · PEO-117)
Why now: test stages have edges until the continuous map (PEO-118) removes them.
Affects: gameplay · rewrite: none
- A) Stays centred; beyond the map is dark   ← recommended
- B) Stops at the edge; the character drifts off centre

## D-050 · Zoom                                            (raised by architect · 2026-10-06 · PEO-117)
Why now: GAMEPLAN Phase 2 lists integer zoom; with one cell size (ADR-0003) zoom scales the text too.
Affects: gameplay · rewrite: none
- A) None for now; revisit with the tileset renderer   ← recommended
- B) Integer zoom keys now, text and map together
