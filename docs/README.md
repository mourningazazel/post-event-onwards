# docs · read by task

Load what the task needs, nothing more. Rolling docs have word caps enforced by
`tools/validate_docs.py`; archives are not indexed here.

Docs are organised by problem domain: **[domains/](domains/README.md)** lists the set every
session loads (role, vision, the brief, design checks, testing) and one page per domain
(the Dead, time, world, items, characters, persistence, presentation, performance, engine API,
process). Load the always set and the one domain your work names.

| When you are… | Read |
|---------------|------|
| Starting any session | `../AGENTS.md`, your role in [roles.md](roles.md), then [domains/](domains/README.md) |
| Working on a queue item | its brief, then the domain page in its references |
| Unsure why something is the way it is | the domain page's decisions, then [production/decisions.md](production/decisions.md) |
| Facing a gameplay / abstraction / direction choice, or a big rewrite | Add an entry to [production/DECISIONS_NEEDED.md](production/DECISIONS_NEEDED.md); set the item `AwaitingUser` |
| Adding or changing a system | [architecture.md](architecture.md), the target map in [SYSTEMS.md](SYSTEMS.md), the stack in [GAMEPLAN.md](GAMEPLAN.md) |
| Touching an accepted architecture decision | [adr/](adr/README.md) (never edit; supersede) |
| Reading what the owner actually said | [design/notes/](design/notes/README.md) (verbatim, never rewritten) |
| Checking which RogueBasin ideas we use | [research/roguebasin-notes.md](research/roguebasin-notes.md) |
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
| bottlenecks.md | 900 words |
| CLAUDE.md | 300 words |
| production/vision.md | 900 words |
| production/decisions.md | 1500 words (archive old ones to `production/decisions-archive/`) |
| production/DECISIONS_NEEDED.md | 800 words (answer some before adding more) |
| WORK_QUEUE.json | 2500 content words (metadata only; briefs are files) |
| production/briefs/`<id>`.md | uncapped, like handoffs |
| DEFERRED_WORK.json | 1500 content words |
| any SKILL.md or `.agents/agents/*.md` | 600 words |
