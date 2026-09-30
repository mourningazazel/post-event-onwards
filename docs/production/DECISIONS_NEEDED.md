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

## D-033 · The 1 s test budget and the town scenario tests      (raised by architect · 2026-09-30 · PEO-066)
Why now: the town scenarios (PEO-030) add ~350 ms under the sanitizers; the suite runs 0.86-1.08 s on slower machines, so CI will go red again.
Affects: direction · rewrite: none
- A) Keep one 1 s budget for everything — scenario tests shrink to small map slices, losing some realism
- B) Two tiers — unit tests keep the 1 s budget for the edit-test loop; town and scenario tests run in the same CI build under their own 5 s budget   ← recommended
- C) Raise the single budget to 2 s — simplest; the edit-test loop gets slower as scenarios keep growing
