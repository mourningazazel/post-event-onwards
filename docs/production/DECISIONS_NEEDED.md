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

## D-009 · Where should briefs live?   (raised by architect · 2026-09-29 · tooling)
Why now: WORK_QUEUE.json hit its 4000-word cap the same day it was raised from 2500. Briefs live on the queue item, a usable brief runs 300-500 words, and 28 items do not fit. The next brief fails validation, and trimming briefs to fit means briefing worse.
Affects: abstraction · rewrite: small (tools/work_queue.py, docs/roles.md, docs/README.md)
- A) Move briefs into their own files — docs/production/briefs/PEO-xxx.md, exactly as handoffs already work. The queue keeps id, title, status, owner, effort, depends_on and a pointer; work_queue.py gains read/write of the brief file. The queue stops growing with detail and the cap becomes easy to hold.   ← recommended
- B) Raise the cap again, to 8000 or so — one line, no refactor, but the same problem returns and the cap stops meaning anything.
- C) Keep the cap and brief fewer items at once — only brief what the Builder takes next, stripping briefs from distant items. No code change, but it discards prepared work and needs constant pruning.
