# Post Event: Onwards · the handover wiki

This wiki is for someone who owns the game but did not write the code: you can read it
start to finish and come out able to run, change and steer the project yourself. It explains
what is built today, how each system works, the maths behind it, which numbers you can
change, and how the project is organised around two AI agents and a work queue.

It is a **guide**, not the record. Where it summarises something, it links to the source of
truth: the code (`path:line`), an architecture decision (`docs/adr/`), a settled decision
(`D-0xx` in `docs/production/decisions.md` or its archive), or the owner's own words
(`docs/design/notes/N0xx`). If the wiki and the code disagree, the code wins; see
[known drift](14-taking-over.md#known-drift-in-the-existing-docs) for places the older docs
have already fallen behind.

Written 2026-10-12 against `main` at `fddde4c` (PEO-127 reviewed, the queue all `Pending`).

## Reading order

About two to three hours, in this order. Pages 1 to 4 are the foundation; after that you can
jump to whichever system you want to change.

| # | Page | You will know |
|---|------|---------------|
| 1 | [Orientation](01-orientation.md) | What the game is, what exists today versus what is designed, and where everything lives in the repo |
| 2 | [Build, run and test](02-build-run-test.md) | How to build it, play it, run every test, and what CI checks |
| 3 | [Architecture](03-architecture.md) | The rules the code obeys (determinism, no SDL in core, integers), the layers, and how a turn flows through `World` |
| 4 | [Time and turns](04-time-and-turns.md) | Substeps, actions, updates, the Dead's slots, and computing the next turn while you think |
| 5 | [Scent](05-scent.md) | The geodesic scent field: the formula, ageing, wind, the kernels, and the exact patch |
| 6 | [The Dead](06-the-dead.md) | Spawning, who moves when, the weighted draw (desire field), crowding rules, and the swarm tests |
| 7 | [World generation](07-world-generation.md) | Today's random stages, and the geography, lakes and settlement sites built for the real map |
| 8 | [Player view and frontend](08-player-and-frontend.md) | Sight, map memory, Look, the camera and HUD, input pacing, threads and the GPU in the app |
| 9 | [Saves and revisits](09-saves-and-revisits.md) | The save container and sections, and how a horde thins while you are away |
| 10 | [Content data](10-content.md) | The TOML item and world catalogue, and the Python tools that check and derive it |
| 11 | [API reference](11-api-reference.md) | Every public type and call in `peo::core`, grouped by job |
| 12 | [Levers](12-levers.md) | Every tunable in one table: where it is, its default, what changing it does |
| 13 | [How the project runs](13-workflow.md) | Architect and Builder, the queue, briefs, reviews, decisions, skills, caps and hooks |
| 14 | [Taking over](14-taking-over.md) | Three ways to step in, a first week plan, the state of the queue, and known doc drift |
| 15 | [Roadmap](15-roadmap.md) | What is designed but not built, grouped by system, with queue ids |
| | [Glossary](glossary.md) | Every term of art in one place |

## The project in one paragraph

A turn-based survival roguelike set after the Event, when nearly everyone became **the
Dead**: blind walking corpses that follow scent. The player cannot help leaving scent, and
the core of the game is a scent field the Dead climb. The code is C++20: a deterministic,
headless simulation library (`src/core`, namespace `peo::core`) and a thin SDL3 frontend
(`src/app`) that turns keys into `Action`s and draws the committed world. Today you can walk
a stage of random walls while a few hundred Dead drift up your trail; most of the design
(buildings, items, characters, sound, real towns) is written down but not yet built. Work is
planned and reviewed by an "Architect" Claude in the cloud and built by a "Builder" Claude on
your machine, through `WORK_QUEUE.json`.

## Where the other docs fit

The repo already has a lot of documentation. This wiki does not replace it; it gives you the
path through it.

- `docs/README.md`: the agents' "read by task" index, and the word caps.
- `docs/domains/`: one short page per problem domain listing its docs, ADRs, decisions and
  queue items. Good second stop after the matching wiki page.
- `docs/architecture.md`: the dense, current-state system description the agents maintain.
- `docs/production/vision.md`: the pillars. Read it; it is short.
- `docs/SYSTEMS.md`, `docs/GAMEPLAN.md`: the target framework and the stack research, written
  before any code. The destination, not the present.
- `docs/design/`: the design corpus (scent and the Dead, world generation, content model,
  survival actions). Large; the domain pages say which part to read.
- `docs/adr/`: accepted architecture decisions. Never edited, only superseded.
