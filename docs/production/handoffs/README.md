# Handoffs

One file per Blocked queue item, named `<id>.md`. `tools/work_queue.py check`
fails when a Blocked item has no handoff.

Template:

```markdown
# PEO-123 · <title>

**Blocked on:** one sentence.
**Who can unblock:** user / architect / builder.

## State
- branch / commits so far
- what works, what does not

## What I tried
1. …

## Proposed next step
…
```

Delete the file when the item leaves Blocked.
