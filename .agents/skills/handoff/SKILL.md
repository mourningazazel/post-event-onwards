---
name: handoff
description: Write a handoff doc for a Blocked item so anyone can pick it up cold.
---
# /handoff

1. Create `docs/production/handoffs/<id>.md` from the template in
   `docs/production/handoffs/README.md`. Be specific: commands run, exact
   errors, the hypothesis you had not yet tested.
2. `python3 tools/work_queue.py set <id> --status Blocked --note "handoff
   written: <one line reason>" --by <you>`.
3. `check`, commit the handoff and the queue file: `[PEO-xxx] handoff`.
4. Move on to the next actionable item.

When the block clears, delete the handoff file in the same commit that
returns the item to `Pending` or `InProgress`.
