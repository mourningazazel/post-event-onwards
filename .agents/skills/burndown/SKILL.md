---
name: burndown
description: Builder: work the queue in effort order S → M → L → XL until stopped, skipping items that need a decision. Each item runs in a fresh subagent context.
---
# /burndown (Builder)

Same work as `/start-work`, different order: all actionable `S` items first,
then `M`, `L`, `XL`. Skip anything `AwaitingUser` or without a brief.

## Fresh context per item (owner, 2026-10-05)

This session only picks items and keeps a tally; it never reads briefs,
diffs or logs itself. Every turn re-reads the whole session, so an item's
reading must not outlive the item.

1. `git fetch origin main && git merge --ff-only origin/main`, then
   `python3 tools/work_queue.py list` to pick the next item by effort.
2. Start one general-purpose subagent for that item, in the main checkout
   (nothing else touches it while the item runs), with this prompt:
   "Builder. Read `.agents/skills/start-work/SKILL.md` and follow it for
   `<id>` only. Do not take another item. Finish with one line: id, final
   status, commit count, anything open."
3. Keep only that line. Do not re-read what the subagent did.
4. Repeat. Stop when the queue is empty, the user interrupts, or three
   items in a row bounce back from review (then ask what is wrong before
   continuing).
