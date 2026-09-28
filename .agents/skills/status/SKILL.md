---
name: status
description: Print the queue summary, recent completed work, and what each role should do next.
---
# /status

1. `git fetch origin main`.
2. `python3 tools/work_queue.py summary`.
3. Show the latest `docs/COMPLETED_WORK/*.md` (last 10 lines).
4. Say in three lines: what the Builder should take next (`next --owner
   builder`), what the Architect should brief or review (`list --owner
   architect`), and what is waiting on the user (`list --status
   AwaitingUser`).
