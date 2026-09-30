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

## D-032 · Fast direction taps are dropped                     (raised by architect · 2026-09-30 · PEO-065)
Why now: playtesting found quick taps ignored; D-011 drops any key sooner than 1/3 s after the last step.
Affects: gameplay · rewrite: none
- A) Keep D-011 — taps faster than 3 a second are lost; holding stops at once on release
- B) Queue every press, drained 3 a second, queue cleared when all keys are released — nothing lost; a long burst of taps still plays out after you stop tapping
- C) Taps and holds differ — each distinct tap is kept (up to 3 waiting, one second's worth); a held key's auto-repeat stays capped at 3 a second and stops the moment it is released   ← recommended
