---
name: bug
description: Record a bug in the work queue with reproduction steps, from a description or screenshot.
---
# /bug

Turn a bug report into a queue item the Builder can reproduce.

1. Gather: seed (`peo <seed>`), stage index, what was done, what happened,
   what was expected. From a screenshot, transcribe the HUD line (stage,
   tick, horde count).
2. If it can be reproduced headlessly, write the failing doctest first under
   `tests/core/` and reference it. A red test is the best bug report.
3. `python3 tools/work_queue.py add --type bug --title "…" --severity S1..S3
   --effort S|M --goal "…" --ref tests/core/test_x.cpp`
4. Put the repro in the brief: `python3 tools/work_queue.py brief <id> --file
   <tmp.json>` with `acceptance` (the test passes / the in-game behaviour) and
   `manual` (exact steps).
5. `check`, then commit the queue file: `[queue] bug: <title>`.

S1 bugs go to the top: `set <id> --order 0`.
