# docs · read by task

Load what the task needs, nothing more. Rolling docs have word caps enforced by
`tools/validate_docs.py`; archives are not indexed here.

| When you are… | Read |
|---------------|------|
| Starting any session | `../AGENTS.md`, then your role in [roles.md](roles.md) |
| Deciding what the game should be | [production/vision.md](production/vision.md) |
| Unsure why something is the way it is | [production/decisions.md](production/decisions.md) |
| Facing a gameplay / abstraction / direction choice, or a big rewrite | Add an entry to [production/DECISIONS_NEEDED.md](production/DECISIONS_NEEDED.md); set the item `AwaitingUser` |
| Adding or changing a system | [architecture.md](architecture.md) |
| Writing or running tests | [testing.md](testing.md) |
| Picking up a Blocked item | `production/handoffs/<id>.md` (index in [production/handoffs/README.md](production/handoffs/README.md)) |
| Checking what shipped | `COMPLETED_WORK/YYYY-MM-DD.md` |
| Recording, briefing or reviewing work | the skills in `../.agents/skills/` |

## Precedence

User instruction → `production/decisions.md` → topic page → `AGENTS.md`.

## Caps

| File | Cap |
|------|-----|
| AGENTS.md | 450 words |
| CLAUDE.md | 300 words |
| production/vision.md | 900 words |
| production/decisions.md | 1500 words (archive old ones to `production/decisions-archive/`) |
| production/DECISIONS_NEEDED.md | 800 words (answer some before adding more) |
| WORK_QUEUE.json / DEFERRED_WORK.json | 1500 content words each |
| any SKILL.md | 600 words |
