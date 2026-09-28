# Post Event: Onwards

A performance-heavy, open-source roguelike of super-realistic zombie survival:

- ASCII-first grid presentation in the spirit of Dwarf Fortress, with swappable PNG
  fonts and tilesets
- blind scent-driven hordes
- an endless, staged, persistent world
- Linux and Windows, built with C++20 and SDL3

**Status: framework design phase.** There is no engine code yet; architecture is being designed
system by system, with review gates. **Content data** (items, materials, substances, the world
catalogue) is being authored now in `content/`, validated by the content tools.

```sh
python3 tools/content/lint.py      # schema, vocabulary and reference checks for content/
python3 tools/content/test.py      # derivation expectations + survival-action chain feasibility
```

## Documents

| Doc | Purpose |
|---|---|
| [docs/GAMEPLAN.md](docs/GAMEPLAN.md) | Research-backed stack and architecture proposal, and the decisions log |
| [docs/SYSTEMS.md](docs/SYSTEMS.md) | Framework rules and the proposed map of major systems |
| [docs/adr/](docs/adr/) | Architecture Decision Records |
| [docs/design/gameplay-model/](docs/design/gameplay-model/) | **Gameplay model walkthrough:** owner decisions on how gameplay concepts are represented |
| [docs/design/notes/](docs/design/notes/) | Owner design notes, recorded as given |
| [docs/design/survival-actions.md](docs/design/survival-actions.md) | What a real survivor would do, the mechanics that need, and the item vocabulary; worked chains |
| [docs/design/world-catalog.md](docs/design/world-catalog.md) | Settings, buildings, rooms, outdoor sets, profiles, item plan; the content id contract |
| [content/](content/) | Game content data and its schema reference (`content/README.md`) |
| [docs/design/content-backlog.md](docs/design/content-backlog.md) | Reconciliation decisions, vocabulary proposals, open owner questions from content authoring |
| [docs/design/content-model.md](docs/design/content-model.md) | Content model: materials, features and joints, capabilities, reactions, tags; how Claude authors content |
| [docs/design/brands.md](docs/design/brands.md) | Fictional brand universe (parent companies, brands, store own labels) |
| [docs/design/purposes.md](docs/design/purposes.md) | Purpose catalog: what each gameplay function is for, and its permanent purpose tests |
| [docs/design/playtest-harness.md](docs/design/playtest-harness.md) | How Claude plays and tests the game: headless play, scenarios, bots, rule checker, replays |
| [docs/design/scent-mobs.md](docs/design/scent-mobs.md) | Enemy movement analysis, with benchmark |
| [docs/design/world-generation.md](docs/design/world-generation.md) | Endless staged-generation analysis |
| [docs/LICENSING.md](docs/LICENSING.md) | License policy and verified dependency licenses |
| [docs/research/roguebasin-notes.md](docs/research/roguebasin-notes.md) | Condensed notes from ~244 RogueBasin pages |
| [docs/research/spikes/](docs/research/spikes/) | Throwaway measurement code |

## License

[MIT](LICENSE). Dependency licenses and policy: `docs/LICENSING.md`.
