# Briefs

One file per live queue item, named `<id>.md`. `tools/work_queue.py` owns
them: `brief <id> --file <json>` writes one, `show <id>` reads it back onto
the item. Do not hand-edit; the tool round-trips the format exactly.

Shape (a section is omitted when its field is empty, one entry per line):

```markdown
# PEO-123 · <title>

## Goal
<one sentence: what is true when this is done>

## Context
- <file or doc anchor to read first>

## Units
- <ordered, commit-sized step>

## Acceptance
- <observable criterion>

## Tests
- <headless test that must pass>

## Manual
- <in-game step and what to look for>

## Out of scope
- <what not to touch, and where it went instead>
```

The title line is regenerated from the queue, not brief data. Field meanings
are in `../../roles.md`. These files are uncapped — brief properly.

`work_queue.py complete` leaves the file behind; git holds the history.
