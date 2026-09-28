---
name: plan
description: Architect: turn a Pending item into a full brief with units, acceptance criteria, tests and manual steps.
---
# /plan (Architect only)

Turn an item into something the Builder can finish without asking.

1. `python3 tools/work_queue.py show <id>`; read `docs/production/vision.md`
   and the `docs/architecture.md` section for the system involved.
2. Decide the seam. If the item needs a new public interface, write the
   header in `src/core/include/peo/core/` with doc comments and a doctest in
   `tests/core/` that pins the behaviour. Leave the implementation as a
   stub only if it keeps the build green; otherwise leave the test red and
   say so in the brief.
3. Write the brief JSON (fields: goal, context, units, acceptance, tests,
   manual, out_of_scope; see `docs/roles.md#handoff-formats`) and apply it:
   `python3 tools/work_queue.py brief <id> --file <path>`.
   - Units: ordered, commit-sized, each named with its layer: `core:`,
     `test:`, `app:`, `docs:`.
   - Acceptance: observable. "`ctest` passes", "in game, hordelings stop at
     the wall".
   - Manual: what the Builder should do and what to look for, including
     one perf observation when the change touches a per-tick loop.
4. Split anything over effort L into several items with `depends_on`.
5. `set <id> --owner builder --note "briefed" --by architect`.
6. If it needs a decision, add it to `docs/production/decisions.md` in the
   same commit.
7. `python3 tools/verify.py`, then commit the queue file, any headers, tests
   and docs: `[PEO-xxx] brief: <title>`.
