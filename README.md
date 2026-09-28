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
| [docs/design/scent-mobs.md](docs/design/scent-mobs.md) | Enemy movement analysis, with benchmark |
| [docs/design/world-generation.md](docs/design/world-generation.md) | Endless staged-generation analysis |
| [docs/LICENSING.md](docs/LICENSING.md) | License policy and verified dependency licenses |
| [docs/research/roguebasin-notes.md](docs/research/roguebasin-notes.md) | Condensed notes from ~244 RogueBasin pages |
| [docs/research/spikes/](docs/research/spikes/) | Throwaway measurement code |

## License

Open source. The project license has not been chosen yet (see `docs/LICENSING.md`).
