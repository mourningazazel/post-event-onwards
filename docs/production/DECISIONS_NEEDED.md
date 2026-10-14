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

## D-060 · Where real-size tests run                    (raised by architect · 2026-10-14 · PEO-145)
Why now: D-059 puts scenario and perf checks at 1024x1024 with play density, but there one update costs 60 ms in release on the cloud runner (109 ms to build the world), and the release scenario tier already measures 7.5 s of its 5 s there (D-033; CI's faster runners pass), so even a short real-size walk does not fit.
Affects: direction (test budgets) · rewrite: none
- A) A third tier: `realsize*` suites, release builds only, own budget (60 s), run by `verify.py --full` and CI's release job; unit and scenario keep 1 s and 5 s   ← recommended
- B) Raise the scenario budget for release builds to 60 s; real-size cases join the scenario tier
- C) Keep 5 s; real-size checks live only in on-demand tools (`peo_bench`, a siege-like binary) and manual steps, never in ctest or CI
