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

## D-042 · Who writes the game's text and the ASCII font             (raised by architect · 2026-10-03 · PEO-008)
Why now: N023 says you supply detailed descriptions, lore and all art; agents author item data and property-generated text today (N008).
Affects: direction · rewrite: none
- A) Agents keep authoring item data and property-generated short text, marked as placeholder for you to replace; you write detailed descriptions, lore and the font   ← recommended
- B) You write every player-facing line; agents author data only, and the game shows IDs until your text lands
- C) As A, but agents also draw a plain placeholder ASCII font until yours exists

## D-041 · What lies beyond one 100 km geography square                (raised by architect · 2026-10-03 · PEO-095)
Why now: ADR-0018 generates 100 km squares whole; ADR-0007 says the world is endless.
Affects: direction · rewrite: small
- A) Endless: neighbouring squares generate whole when the player nears an edge, with coast and ranges continuous across them   ← recommended
- B) One square per world: its edge is sea or impassable wilderness, and the world is 100 km across

## D-040 · Does a revisited area still need a month away to change        (raised by architect · 2026-10-03 · PEO-094)
Why now: ADR-0019 re-ages areas (your things included) when reloaded; N009 asked that nothing change under about a month away.
Affects: gameplay · rewrite: none
- A) No gate: any area out of range catches up when reloaded, so a day away changes a little and a month a lot (a gradient)   ← recommended
- B) Keep the 30-day gate, and age your placed and changed things too once past it
- C) A short gate (a data value, e.g. 3 days), then the gradient

## D-039 · Long actions cost twice the updates under 3 s turns          (raised by architect · 2026-10-03 · PEO-090)
Why now: with a walking turn at 3 s and sleep still 8 hours, sleeping runs 9,600 updates instead of 4,800, so time skips cost twice as much per game hour.
Affects: gameplay · rewrite: none
- A) Accept it: every turn behaves as today; fast time passing (PEO-082, threads and GPU) absorbs the cost   ← recommended
- B) During long actions only, run one update per 6 game seconds as today; cheaper, but scent and the Dead advance half as far per game hour while you sleep
- C) Keep the 6 s label; only add the between-step layer
