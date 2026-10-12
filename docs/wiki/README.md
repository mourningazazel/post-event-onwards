# Post Event: Onwards wiki

The reference for how the game is built: its systems, the subsystems inside them, the APIs
between them, the maths each one runs, the values that tune it, and the rules the code and
the project follow. Each page describes a system as it is in the code; when the code changes,
the page changes with it (see [Upkeep](#upkeep)).

## Pages

**The program**

- [Overview](overview.md): the game, the parts of the program and what each one does,
  which systems are built and which are designed.
- [Architecture](architecture.md): the rules the code obeys, the layers, `World` and the flow
  of a turn, threads and the GPU.
- [API reference](api-reference.md): every public type and call in `peo::core`.
- [Tunables](tunables.md): every lever in one table.

**Systems**

- [Time and turns](time-and-turns.md): substeps, actions, updates, the Dead's slots,
  speculate and commit, input pacing.
- [Scent](scent.md): the geodesic field, ageing, rounds, wind, the exact patch.
- [The Dead](the-dead.md): units, spawning, the desire field and the weighted draw, crowding.
- [World generation](world-generation.md): stages, geography, water, settlement sites.
- [Sight and memory](sight-and-memory.md): shadowcasting, map memory, Look.
- [Persistence](persistence.md): the save container and sections, the clock since the
  Event, aftermath curves, revisits.
- [Content](content.md): the TOML catalogue and the Python tools that check and derive it.
- [Frontend](frontend.md): the SDL3 app, drawing, input, the speculation thread, tilesets.

**The project**

- [Build and test](build-and-test.md): toolchain, presets, the game's flags and keys, test
  tiers, on-demand suites, `verify.py`, CI, hooks.
- [Development process](development-process.md): the Architect and Builder roles, the work
  queue, briefs, reviews, decisions, skills and guard rails.
- [Glossary](glossary.md).

## How this wiki relates to the other docs

| Source | Holds | This wiki |
|---|---|---|
| The code (`src/`, `tests/`) | The truth | Explains it; wins over nothing |
| `docs/adr/` | Accepted architecture decisions (never edited, superseded) | Cites them by number |
| `docs/production/decisions.md` and its archive | The owner's decisions (`D-0xx`) and why | Cites them by number |
| `docs/design/notes/` | The owner's words, verbatim (`N0xx`) | Cites them |
| `docs/design/` | The design corpus: what each system is meant to become | Links to it from each system's "Designed, not built" section |
| `docs/domains/` | Per-domain reading lists for agents | Parallel index, agent-facing |
| `docs/architecture.md` | The agents' compact system description | Overlaps; this wiki goes deeper and is written for a human reader |
| `WORK_QUEUE.json` | Planned and in-flight work (`PEO-xxx`) | Names the items behind each designed system |

## Conventions

- Code is referenced by file and symbol (`world.cpp`, `World::advance`) rather than line
  numbers, so references survive edits.
- Formulas are written as the code computes them, integer division and clamps included.
- Each system page ends with its tunables and a "Designed, not built" section pointing at the
  design docs and queue items.
- The enemies are **the Dead**; the word "zombie" is not used.

## Upkeep

The wiki is a living document. A change that alters a behaviour, formula, tunable, public
symbol, command or process described here updates the matching page in the same change. The
Architect checks it in review (`/review`), and the `docs-drift` subagent includes
`docs/wiki/` when it looks for stale docs. A system moving from designed to built moves its
section into the page body.
