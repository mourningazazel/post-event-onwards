---
name: start-work
description: Builder: take the next actionable queue item, implement it unit by unit, verify, commit, push, and report.
---
# /start-work (Builder)

Work the queue, earliest actionable item first, until told to stop.

1. `git fetch origin main && git merge --ff-only origin/main`.
2. `python3 tools/work_queue.py next --owner builder`. Nothing? Say so and
   stop; do not invent work.
3. `show <id>`; read every path in `brief.context`. If the brief is missing
   `units` or `acceptance`, set `--status AwaitingUser --note "needs brief"`
   and move on to the next item.
4. `set <id> --status InProgress --note "starting unit 1" --by builder`;
   commit the queue file `[PEO-xxx] start`.
5. For each unit, in order:
   - implement in the named layer only, then read the diff against
     `/cpp-core`; a hot path changed means `/perf` numbers in the report,
     before and after, for every `docs/bottlenecks.md` row it touches;
   - `python3 tools/verify.py --frontend --full` (the whole thing, every
     time; `--full` because each unit is pushed to main);
   - `git add <explicit paths>`; `git commit -m "[PEO-xxx] <unit summary>"`;
   - `git push origin main` (retry with fetch + ff-merge on rejection).
6. Run every `brief.manual` step in the real game. Note results verbatim.
7. Report: `note <id> --by builder --text "commits: … tests: … manual: …
   perf: … open: …"` (format in `docs/roles.md`), then
   `set <id> --status Validation --owner architect`; commit and push the
   queue file `[PEO-xxx] report`.
8. Bugs seen on the way: `/bug`, do not fix unless they block the unit.
   A unit that turns out to need a gameplay, abstraction or direction
   choice, or a big rewrite (three or more core headers, or a signature
   used outside its file): stop, add a `D-xxx` entry to
   `docs/production/DECISIONS_NEEDED.md` with A/B/C options and your
   recommendation, `set <id> --status AwaitingUser --note "D-xxx"`, commit,
   and take the next item. Technical method questions you answer yourself.
9. Blocked for more than one honest attempt: `/handoff`.
