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

## D-046 · What "nearest kind" means for a removed item's stand-in      (raised by architect · 2026-10-06 · PEO-092)
Why now: ADR-0017 turns a removed item into "a generic stand-in of the nearest kind still present", but content has no kind hierarchy yet (`claw_hammer` has no parent and there is no generic hammer), so PEO-092 cannot be briefed; it also waits on PEO-008 (content registry), whichever is chosen.
Affects: gameplay | abstraction · rewrite: none
- A) The `parent` chain: the nearest spawnable ancestor still present; families gain a plain generic archetype (`hammer`) as parent — automatic, needs a content pass adding generic parents   ← recommended
- B) The most specific `cat.*` tag: each category gets one generic archetype ("hand tool") — automatic, no restructure, but stand-ins are coarse
- C) Explicit only: the remap table names a stand-in for every removed ID and anything unlisted becomes junk — no inference, each removal must be authored
