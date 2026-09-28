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

## D-002 · Real-time ticks or turn-based movement?              (raised by architect · 2026-09-28 · PEO-002)
Why now: the current frontend runs the world at a fixed 10 Hz whether or not the player acts; classic roguelikes advance the world only when the player moves. PEO-002 (World) will bake this into the core API.
Affects: gameplay · rewrite: small now, large after PEO-002
- A) Real-time, fixed tick — pressure never stops; standing still is a choice with a cost. Hordes feel like a flood.   ← recommended
- B) Turn-based — world advances one tick per player action; tactical, readable, traditional. Scent still spreads per turn.
- C) Hybrid — turn-based, but the world auto-ticks after N seconds of idling.

## D-001 · What are the enemies called in code?                (raised by architect · 2026-09-28 · PEO-002)
Why now: the fiction calls them the Dead; the code currently says `Hordeling` / `step_horde`. Renaming is free before PEO-002 lands and costly after.
Affects: abstraction · rewrite: small now, medium later
- A) `Dead` for one unit, `horde` for the collection (`std::vector<Dead>`, `step_horde`) — matches fiction, "horde" stays as the group noun.   ← recommended
- B) Keep `Hordeling` — neutral, already written.
- C) Something else; tell me the word and I will use it everywhere.
