---
name: review
description: Architect: review a Validation item against its brief, the vision and architecture docs, and the tests; complete or bounce it.
---
# /review (Architect only)

Decide whether finished work goes where the project is going.

1. `git fetch origin main`. `python3 tools/work_queue.py list --status
   Validation`; take the lowest `order`.
2. `show <id>`; read the Builder's report note. Collect the commits:
   `git log origin/main --grep="\[<id>\]" --oneline`, then
   `git diff <first>^..<last>`.
3. Check, in this order, and write one line per check in your notes:
   - **Brief:** every `unit` present, every `acceptance` met, nothing from
     `out_of_scope` touched.
   - **Direction:** consistent with `docs/production/vision.md` pillars and
     `docs/architecture.md` boundaries (logic in `src/app` is a finding).
   - **Abstraction:** new state lives in the right owner; tunables are named;
     no allocation in per-tick loops; interfaces match the planned seams.
   - **Tests:** `git checkout origin/main -- .` in a worktree, run
     `python3 tools/verify.py`. Behaviours added without a test are a
     finding unless the brief listed them as `manual`.
   - **Report:** `manual` results are specific; perf numbers present when
     asked.
4. Pass: `python3 tools/work_queue.py complete <id> --by architect --commit
   <last sha>`. Update `docs/architecture.md` if a seam changed.
5. Fail: `set <id> --status Pending --owner builder --note "1. … 2. …" --by
   architect`. Findings are concrete edits, not opinions. If the failure is a
   direction change, also add a decision entry.
6. Commit queue and docs: `[PEO-xxx] review: pass|rework`.
