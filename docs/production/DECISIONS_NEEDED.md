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

## D-008 · How much scent machinery do we build?   (raised by architect · 2026-09-29 · PEO-026)
Why now: the Dead cannot head off a moving player, which is the behaviour you want — but it currently falls out of the solver being too slow to converge, not from any dial we control. Measurements and the maths are in docs/design/scent-performance.md.
Affects: gameplay | abstraction · rewrite: none to medium
- A) Ship the retune only (PEO-026) — three constants and two tests. Reach ~57 cells for a standing player, the doorway scenario works, the Dead never intercept. Cheapest to build, but "how far ahead can they smell me" stays an accident of the solver rather than a setting.
- B) Two layers, staggered — a near layer (small lambda, every turn) for the cloud around you, and a far layer solved directly with a separable IIR every 8-16 turns for scent pooled where you lingered. Amortises to ~0.02 ms/turn, below what you pay now, and converged instead of perpetually lagging; the near layer's lambda becomes an explicit interception-distance dial. Costs a real rewrite of ScentField.
- C) Both, in order — ship A now to see it in game, then B as its own item with the doorway scenario as its acceptance test.   ← recommended
