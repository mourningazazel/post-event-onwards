# Domains · load one, not the whole corpus

Every piece of work belongs to one problem domain. A session loads the **always** set, then the
one domain page its work names, and stops there. A queue item names its domain page in its
`references`; a brief's `context` starts from it. Reaching into a second domain is fine when
the work crosses it, but load that domain's page, not its whole folder.

## Always, for every session

- `../../AGENTS.md`, then your role in [roles.md](../roles.md)
- [production/vision.md](../production/vision.md): intent and pillars
- The brief (`production/briefs/<id>.md`) or the user's ask
- Design checks: the purpose rows your change touches in
  [design/purposes.md](../design/purposes.md) (search by ID, don't read it whole), and
  [bottlenecks.md](../bottlenecks.md) for any per-update work
- Testing rules: [testing.md](../testing.md)
- Before deciding anything: open entries in
  [production/DECISIONS_NEEDED.md](../production/DECISIONS_NEEDED.md); the domain page lists the
  settled decisions that apply

## The domains

| Domain | Covers |
|---|---|
| [dead](dead.md) | The Dead, scent, soak, rot, wind, sound, crowds, the siege suite |
| [time](time.md) | Turns, the clock and substeps, speculate/commit, threads and the GPU executor |
| [world](world.md) | Geography, settlements, buildings, aftermath and the clock's effects, revisits |
| [items](items.md) | Content model, items, containers, survival actions, skills content, brands |
| [characters](characters.md) | Character model, stats and skills, mood and sanity, death and succession |
| [persistence](persistence.md) | Saves, migration, replays, future network sync |
| [presentation](presentation.md) | Renderer, glyphs and tilesets, input, UI |
| [performance](performance.md) | Budgets, measuring, hardware tiers, auto-config |
| [engine-api](engine-api.md) | The future engine split and scripting surface (intent only) |
| [process](process.md) | Queue, skills, CI, tooling, licensing |

A doc lives in one domain. Owner notes (`design/notes/`) are listed in the domain they act on;
read one only when the domain page points you at it for your task.
