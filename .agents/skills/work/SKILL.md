---
name: work
description: Record, update, prioritise, defer or promote a work-queue item. Records only; never implements.
---
# /work

Record intent in `WORK_QUEUE.json` through `tools/work_queue.py`. This skill
never writes code.

## Steps

1. `git fetch origin main` and read the queue from the fetched state if your
   checkout lags (`git show origin/main:WORK_QUEUE.json`).
2. `python3 tools/work_queue.py summary` to avoid duplicates.
3. Ask up to three short questions if the request is ambiguous (what is true
   when done, severity, what it depends on). Otherwise decide.
4. Add: `python3 tools/work_queue.py add --type <t> --title "<t>" --severity S1..S4
   --effort S..XL --goal "<one sentence>" [--depends-on PEO-xxx] [--ref path]`.
   New items default to `owner: architect` so they get a brief before anyone
   builds them. Set `--owner builder` only when the brief already exists.
5. Update: `set <id> --status … --order … --note "…" --by <you>`.
6. Defer: `defer <id>`. Promote: only when the user says so, `promote <id>`.
7. `python3 tools/work_queue.py check`, then commit `WORK_QUEUE.json` (and
   `DEFERRED_WORK.json` if touched) with message `[queue] <what changed>`.

## Rules

- Severity: S1 blocks play or the build, S2 wrong behaviour, S3 missing
  behaviour, S4 polish.
- Effort: S fits in one commit, M a session, L several sessions, XL split it.
- Keep titles imperative and under ten words.
- Never mark an item InProgress from here; that is `/start-work`.
- A request that is really a gameplay or direction choice goes to
  `docs/production/DECISIONS_NEEDED.md` as a `D-xxx` entry, not the queue.
