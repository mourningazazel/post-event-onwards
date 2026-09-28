# Roguelike (working title)

A performance-heavy, open-source roguelike:

- ASCII-first grid presentation in the spirit of Dwarf Fortress, with swappable PNG
  fonts and tilesets
- blind scent-driven hordes
- an endless, staged, persistent world
- Linux and Windows, built with C++20 and SDL3

**Status: framework design phase.** There is no game code yet; architecture is being designed
system by system, with review gates.

## Documents

| Doc | Purpose |
|---|---|
| [docs/GAMEPLAN.md](docs/GAMEPLAN.md) | Research-backed stack and architecture proposal, and the decisions log |
| [docs/SYSTEMS.md](docs/SYSTEMS.md) | Framework rules and the proposed map of major systems |
| [docs/adr/](docs/adr/) | Architecture Decision Records |
| [docs/design/gameplay-model/](docs/design/gameplay-model/) | **Gameplay model walkthrough:** owner decisions on how gameplay concepts are represented |
| [docs/design/notes/](docs/design/notes/) | Owner design notes, recorded as given |
| [docs/design/playtest-harness.md](docs/design/playtest-harness.md) | How Claude plays and tests the game: headless play, scenarios, bots, rule checker, replays |
| [docs/design/scent-mobs.md](docs/design/scent-mobs.md) | Enemy movement analysis, with benchmark |
| [docs/design/world-generation.md](docs/design/world-generation.md) | Endless staged-generation analysis |
| [docs/LICENSING.md](docs/LICENSING.md) | License policy and verified dependency licenses |
| [docs/research/roguebasin-notes.md](docs/research/roguebasin-notes.md) | Condensed notes from ~244 RogueBasin pages |
| [docs/research/spikes/](docs/research/spikes/) | Throwaway measurement code |

## License

[MIT](LICENSE). Dependency licenses and policy: `docs/LICENSING.md`.
