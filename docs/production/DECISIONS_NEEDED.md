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

## D-043 · Whether one of the Dead may step onto the player's tile          (raised by architect · 2026-10-03 · PEO-102)
Why now: the siege suite (PEO-088) shows one of the Dead standing on the player's tile in every run with an opening, and nothing happens, because no rule forbids it and no contact rule exists yet.
Affects: gameplay · rewrite: small
- A) The player's tile counts as taken: the Dead stop beside the player until a contact rule (grab, bite) exists, then that rule decides what happens — realistic and keeps maps readable   ← recommended
- B) Leave it: the Dead may share the player's tile until the contact rule lands, and that rule takes it from there
- C) Stepping onto the player's tile is the contact: it triggers the future grab or bite, so it stays allowed and becomes the trigger
