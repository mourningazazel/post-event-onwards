---
name: review
description: Architect: review a Validation item against its brief, the vision and architecture docs, and the tests; complete or bounce it.
---
# /review (Architect only)

Decide whether finished work goes where the project is going.

A batch is one thread, one item per subagent (D-044): the thread runs
`git fetch origin main && git checkout -B <branch> origin/main`, lists
`--status Validation`, and starts one general-purpose subagent per item,
in order, with: "Architect. Read `.agents/skills/review/SKILL.md` and
follow it for `<id>` only; commit; finish with one line: id, verdict,
commit." It keeps those lines, pushes once (step 6) and replies once.
When handed one item, do that item only.

1. `git fetch origin main`. `python3 tools/work_queue.py list --status
   Validation`; take the lowest `order`.
2. `show <id>`; read the Builder's report note and the item's
   `docs/domains/` page; review against that domain, the always set
   (`docs/domains/README.md`) and the diff, not the whole corpus. Collect the commits:
   `git log origin/main --grep="\[<id>\]" --oneline`, then
   `git diff <first>^..<last>`.
3. Check, in this order, and write one line per check in your notes:
   - **Brief:** every `unit` present, every `acceptance` met, nothing from
     `out_of_scope` touched.
   - **Direction:** consistent with `docs/production/vision.md` pillars and
     `docs/architecture.md` boundaries (logic in `src/app` is a finding).
   - **Abstraction:** new state lives in the right owner; tunables are named;
     no allocation in per-tick loops; interfaces match the planned seams;
     the diff passes `/cpp-core`, and `/perf` numbers are present for a
     changed hot path. A `docs/bottlenecks.md` row made worse without a
     lever or a `[perf]` item is a finding; a new one the numbers show gets
     its row.
   - **Tests:** read the CI result for the item's last commit on `main`
     (every push runs it); a red run, or none, is a finding. Rebuild here
     only when it is red and the cause is unclear, or when the brief asked
     for numbers (`perf-runner`). Behaviours added without a test are a
     finding unless the brief listed them as `manual`.
   - **Report:** `manual` results are specific; perf numbers present when
     asked.
4. Pass: `python3 tools/work_queue.py complete <id> --by architect --commit
   <last sha>`. Update `docs/architecture.md` if a seam changed.
   If the work is correct but raises a gameplay, abstraction or direction
   question (or the next step is a big rewrite), still complete it, then add
   a `D-xxx` entry to `docs/production/DECISIONS_NEEDED.md` and set the
   follow-up item `AwaitingUser`. Findings about technical method are
   yours to decide; do not send them to the user.
5. Fail: `set <id> --status Pending --owner builder --note "1. … 2. …" --by
   architect`. Findings are concrete edits, not opinions. If the failure is a
   direction change, also add a decision entry.
6. Commit queue and docs: `[PEO-xxx] review: pass|rework`. The batch
   thread pushes fast-forward to `main` (`git push origin HEAD:main`); on
   rejection, fetch, rebase its own commits, re-run the tool on a queue
   conflict, push again. A batch whose commits touch `src/`, `tests/`,
   `tools/`, `cmake/` or `.github/` goes through a pull request instead.
