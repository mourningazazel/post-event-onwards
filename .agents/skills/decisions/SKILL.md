---
name: decisions
description: Show open decisions for the user in answer-ready form, or record the user's answers (e.g. "D-002: B") and unblock the related items.
---
# /decisions

## Show (no arguments)

Print `docs/production/DECISIONS_NEEDED.md` entries newest first, then one
line: "Reply with e.g. `D-002: B` — add notes after the letter if you like."
Do not add commentary; the entries already carry the recommendation.

## Record (arguments like `D-002: B, D-001: A keep step_horde`)

For each answer:
1. Move the entry from `DECISIONS_NEEDED.md` to `docs/production/decisions.md`
   as `## <date> · <title>` with the chosen option, the user's notes, and one
   line of consequence.
2. `python3 tools/work_queue.py list --status AwaitingUser`; for every item
   whose note names that D-id: `set <id> --status Pending --note "D-xxx
   answered: <letter>" --by <you>`.
3. If the answer changes `vision.md` or `architecture.md`, edit them in the
   same commit.
4. `python3 tools/verify.py --skip-build`; commit `[decisions] D-xxx: <letter>`.

## Raising (any skill, any time)

Only for gameplay, abstraction, direction, or a big rewrite. Use the fixed
format in `DECISIONS_NEEDED.md`, recommend one option, keep each line short,
and never implement an option on speculation.
