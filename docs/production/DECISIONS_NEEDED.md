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

## D-006 · Tile scale and vertical resolution                       (raised by architect · 2026-09-28 · world generation)
Why now: every generator, the census numbers and the perf target assume a cell size; changing it later rewrites all world data.
Affects: abstraction · rewrite: large later
- A) 1 tile ≈ 1 m, one z-level = one storey (about 3 m); streets ramp between levels   ← recommended
- B) 1 tile ≈ 1 m, z in 1 m slices (finer digging and climbing, 3× the vertical data)
- C) 1 tile ≈ 2 m (smaller cities on screen, coarser interiors)

## D-005 · Attributes: a small physical set, or skills only?         (raised by architect · 2026-09-28 · G04 D4.1)
Why now: carrying, pushing, contests and trample need a body number that skills do not give; the character sheet shape follows.
Affects: gameplay · rewrite: medium later
- A) Physical-realism set: strength, endurance, agility, dexterity, perception, plus Mood and Sanity as the mental side   ← recommended
- B) No attributes: body state, skills, Mood and Sanity only
- C) Classic six (STR/DEX/CON/INT/WIS/CHA)

## D-004 · After death: new world, or the same world?               (raised by architect · 2026-09-28 · G04 D4.4 follow-up)
Why now: hard permadeath is decided; whether the next run can find your former body decides what a save keeps after death.
Affects: gameplay · rewrite: small now
- A) New world every run (seed shareable, simplest)   ← recommended
- B) Same persistent world; your former character may be found risen, with its gear
- C) Player's choice at new-game

## D-003 · Confirm the gameplay-model recommendations as a block     (raised by architect · 2026-09-28 · G01–G03)
Why now: PEO-002 and the content loader will bake these in. All are the walkthrough's recommended options: walls are terrain and fittings are things (D1.1 C); the player is an ordinary creature with a controller (D1.2 B); the Dead are the same kind at a compact tier, promoted when examined (D1.3 B); things are template + components + open properties (D1.4 B); generation addresses (D1.5 B); hidden contents generate when opened (D2.1 B); instances reference templates plus deltas (D2.2 B); single inheritance plus traits (D2.3 B); unexplored areas may change after a game update, explored never do (D2.4); capacity is volume + weight + longest dimension (D3.1 C); identical items stack (D3.2 B); belongings live physically on the body (D3.3 B); condition per part on multi-part items (D3.5).
Affects: abstraction · rewrite: large later
- A) Accept all as listed   ← recommended
- B) Accept all except the ones you name in notes

