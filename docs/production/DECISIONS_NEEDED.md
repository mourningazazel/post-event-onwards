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

## D-007 · How far should the player's scent carry, and how fast should the Dead converge?   (raised by architect · 2026-09-28 · PEO-026)
Why now: PEO-001's playtest shows scent reaching only ~8 cells in ~20 s, so Dead beyond that never move; before we tune the scent rate or write the front-radius test, the target feel is a pacing choice.
Affects: gameplay · rewrite: none
- A) Medium reach — raise diffusion so the scent front crosses the 80-wide map in ~40-60 turns; near Dead home fast, distant ones drift in slowly. Balanced, steady pressure.   ← recommended
- B) Long reach — near map-wide pull within a few turns; the whole horde is always closing in. Tense, few safe cells, easy to get swarmed.
- C) Sound-driven — keep scent short (~10 cells); movement/noise events (per N014) wake distant Dead in bursts, so convergence is event-driven rather than passive. Stealth-forward, more to build.
