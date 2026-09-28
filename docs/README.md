# docs · read by task

Load what the task needs, nothing more. Rolling docs have word caps enforced by
`tools/validate_docs.py`; archives are not indexed here.

| When you are… | Read |
|---------------|------|
| Starting any session | `../AGENTS.md`, then your role in [roles.md](roles.md) |
| Deciding what the game should be | [production/vision.md](production/vision.md) |
| Unsure why something is the way it is | [production/decisions.md](production/decisions.md) |
| Facing a gameplay / abstraction / direction choice, or a big rewrite | Add an entry to [production/DECISIONS_NEEDED.md](production/DECISIONS_NEEDED.md); set the item `AwaitingUser` |
| Adding or changing a system | [architecture.md](architecture.md), then the target map in [SYSTEMS.md](SYSTEMS.md) and the stack in [GAMEPLAN.md](GAMEPLAN.md) |
| Touching an accepted architecture decision | [adr/](adr/README.md) (never edit; supersede) |
| Working on the Dead, scent, sound, crowds | [design/scent-mobs.md](design/scent-mobs.md) (round 3 wins over earlier sections) |
| Working on world generation, the clock, buildings | [design/world-generation.md](design/world-generation.md), [design/world-catalog.md](design/world-catalog.md) |
| Authoring or loading content | [content/README.md](../content/README.md), [design/content-model.md](design/content-model.md), [design/survival-actions.md](design/survival-actions.md) |
| Deciding how a gameplay concept is represented | [design/gameplay-model/](design/gameplay-model/README.md) |
| Asking what a mechanic is for, or adding a test that proves it | [design/purposes.md](design/purposes.md), [design/playtest-harness.md](design/playtest-harness.md) |
| Reading what the owner actually said | [design/notes/](design/notes/README.md) (verbatim, never rewritten) |
| Choosing a dependency | [LICENSING.md](LICENSING.md) |
| Writing or running tests | [testing.md](testing.md) |
| Picking up a Blocked item | `production/handoffs/<id>.md` (index in [production/handoffs/README.md](production/handoffs/README.md)) |
| Checking what shipped | `COMPLETED_WORK/YYYY-MM-DD.md` |
| Recording, briefing or reviewing work | the skills in `../.agents/skills/` |

## Precedence

User instruction → `production/decisions.md` and `adr/` → `design/notes/` (owner's words) →
topic page → `AGENTS.md`. Within `design/`, a later "round" section wins over an earlier one.

## Caps

| File | Cap |
|------|-----|
| AGENTS.md | 450 words |
| CLAUDE.md | 300 words |
| production/vision.md | 900 words |
| production/decisions.md | 1500 words (archive old ones to `production/decisions-archive/`) |
| production/DECISIONS_NEEDED.md | 800 words (answer some before adding more) |
| WORK_QUEUE.json | 2500 content words (briefs live here) |
| DEFERRED_WORK.json | 1500 content words |
| any SKILL.md | 600 words |
