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

## D-024 · Scent transport: geodesic propagating field replaces diffusion   (raised by architect · 2026-09-29 · PEO-030, PEO-035)
Why now: the diffusion field reaches 18% of a town map and parks the Dead at street crossings; the planned IIR far layer cannot see through doors (pathfinding review §3-4, §8); N016's wind, z-route, radius and sealed-building rules all need a transport that can express them.
Affects: abstraction · rewrite: small (scent.cpp, patch_deposit, the perf and golden tests)
- A) integer geodesic field: value = strength − C·distance − K·age along walkable routes; the front advances one cell per update and up to `gust` cells downwind only, with step cost cheaper downwind and dearer upwind scaled by a per-cell openness (0 indoors); stairs are routes; computed only within a radius of the player; walls absorb; min-plus patch in commit; no separate indoor rule needed   ← recommended
- B) keep diffusion, add renormalised spread indoors (N016's inner-wall buildup) and an openness term for wind — measured: reach recovers, the gradient goes flat in rooms and the front still cannot outrun a walker
- C) keep diffusion as the near layer and replace the IIR far layer with a geodesic Dijkstra every 8-16 updates — two solvers, two calibrations, the near layer still pools

## D-025 · Rot masks by scaling, not by subtraction   (raised by architect · 2026-09-29 · PEO-039)
Why now: a 1:1 subtraction of 14 zeroes every cell more than about 12 cells from the player in the open and beyond one room indoors, undoing D-007's reach (pathfinding review §5).
Affects: gameplay · rewrite: none (a formula in the draw)
- A) draw = human / (1 + rot / R), or subtract rot × scale in a log-valued field: a crowd dulls the pull, never blinds it   ← recommended
- B) keep 1:1 subtraction and accept that a lone Dead smells nothing past about 12 cells

## D-026 · Loot detail per line or per item   (raised by architect · 2026-09-29 · PEO-008, PEO-050)
Why now: the contents generator needs to know whether twelve cans in a pantry are one kind or twelve (item-system review §5).
Affects: gameplay · rewrite: none
- A) per line: one kind per pick
- B) per item: every can rolls its own kind and modifiers
- C) per line, but a pick of 4 or more splits into 2 or 3 kinds   ← recommended

## D-027 · Search granularity   (raised by architect · 2026-09-29 · PEO-050)
Why now: per-compartment generation is natural only if searching is per compartment too (item-system review §5).
Affects: gameplay · rewrite: none
- A) one action searches the whole object
- B) each compartment (drawer, shelf) costs its own time, from its volume   ← recommended

## D-028 · Where soak lives when a used object is moved   (raised by architect · 2026-09-29 · PEO-038)
Why now: PEO-038 charges soak per tile; a dragged mattress would leave its smell behind (item-system review §7).
Affects: gameplay | abstraction · rewrite: small
- A) tile only
- B) portable absorbers carry soak as a thing value that moves with them   ← recommended

## D-029 · Quality on a stack   (raised by architect · 2026-09-29 · PEO-013)
Why now: PEO-013 gives every item a quality number; a stack of six needs one rule (item-system review §5).
Affects: gameplay · rewrite: none
- A) one number per line
- B) mean plus spread, sampled when an item is taken from the stack   ← recommended
