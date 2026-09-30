# Post Event: Onwards

*Nearly everyone turned at once. You didn't.*

**Post Event: Onwards** is a turn-based survival roguelike set in the days, weeks and years
after the Event, when almost all of humanity became **the Dead**: walking corpses that
cannot see you, but never stop smelling you.

There are no heroes' quests and no one left to talk to. There is a town as full of things as
a real town, a population's worth of the Dead standing in it, and you, leaving a trail of
scent with every step you take. Where you walk, what you bleed, what you burn and how much
noise you make decide whether they find you. Being swarmed is how most runs end.

The world is endless and it only goes one way: **onwards**. Every step moves you further from
where you started and further along one clock, the days since the Event.

> **Status: early and playable as a prototype.** The simulation core runs today: a generated
> map, a player who walks or runs, a live scent field, and the Dead following it. Most of the
> survival game described below is designed and partly authored as data, but not yet in the
> game. This README marks which is which.

---

## What makes it different

Post Event: Onwards keeps the roguelike fundamentals: turn-based play on a grid, ASCII
presentation, procedural worlds, and hard permadeath. It then pushes on a few ideas most
roguelikes keep simple.

### Scent is the game

The Dead are blind. They hunt by smell, and your scent is a physical thing in the world. It
spreads from where you have been, **flows along walkable routes** (around walls and through
doors, never through them), and fades with distance and age. A trail you left ten minutes ago
reads like one a few metres further away. Stand in one place and scent builds up; run and you
leave less per tile but cover more ground.

The Dead climb that field towards the strongest scent they can reach. They don't know where
you are. They know where you *were*.

### The Dead move like a crowd, not a squad

There are no initiative rolls and no enemy turns. The Dead move on their own staggered
schedule inside the world clock, each finding its moment in a nine-second cycle, so a horde
shuffles rather than marching in lockstep. A calm corpse will not climb over another one:
crowds file through gaps, jam in corridors and pile up at doors, and that congestion is
emergent, not scripted. Numbers are the threat. The performance targets are tens of thousands
of the Dead on a single map.

### A real clock, not abstract turns

Time is kept in game seconds. Walking a tile takes 6 seconds and running takes 3. The world
updates on a fixed cadence whatever you do, and your actions land between those updates.
While you think, the game computes the next update ahead of time, and your move only patches
what it touched, so you never wait on the world.

### Realism over convenience, with menus that respect your time

Buildings are fully furnished, and the tedious parts of survival stay real: every drawer,
cupboard and box is its own search. A kitchen is not a loot pile. It has cutlery drawers,
a cupboard under the sink and a fridge you probably don't want to open. Containers have a
**purpose** and a few tags (a tool chest, a medicine cabinet, a teenager's desk) that decide
what can plausibly be inside. Contents are generated only when you look, and stay exactly as
you left them.

The rule for the interface is the opposite: searching a room should take in-game time, not
real-world patience. Menus are built to be fast to traverse.

### Scales, not switches

Wherever the design can use a gradient instead of a fixed case, it does. Rot doesn't hide you
from the Dead; it turns their interest down. Scent soaks into what you sit and sleep on and
leaks out over days. Skills run 1 to 100 with honest percentages. Item quality is a spread,
not a label. Locks, wind and the Dead's speed are bands and curves rather than three hard-coded
cases.

---

## Playable today

What the current build actually does:

- **Generated stages** from a world seed. The same seed gives the same world, and any stage
  can be rebuilt on demand from its seed and index.
- **Walking and running**, on a world clock in game seconds, with quick key taps queued rather
  than lost and a held key capped at three moves a second.
- **A geodesic scent field.** Integer scent propagates along walkable routes one cell per
  update and decays with distance and age. A scent-view overlay shows it live.
- **The Dead**, spawned one per tile, polling every 9 seconds, each moving in its own hashed
  time slot, following the scent field and never cutting wall corners or sharing a tile when
  calm.
- **Speculated turns.** The next world update is computed in the background and committed
  when you act; at 200×120 with 5,000 Dead a commit takes well under a millisecond.
- **Fully deterministic.** The whole simulation is integer maths from a seed. Golden tests pin
  the exact world state after scripted runs, so any change in behaviour is caught.

Controls: arrows or WASD move, Space or `.` waits, R toggles running, Shift+S shows the scent
view, N goes to the next stage, Esc quits.

## Authored, not yet in the game

A lot of the world exists as data and design already, ahead of the code that will use it:

- **Around 400 item archetypes**, built from materials, features and quality, with fictional
  brands and descriptions that tell you what an item does and what "broken" means for it.
- **Rooms, buildings and loot tables** with container purposes and tags. A linter checks that
  every loot table physically fits the container it is rolled into.
- **Scent soak values** for bedding, furniture and other absorbent things.
- **A catalogue of survival actions and skills** mapped to the capabilities items provide.

The content loader that brings this into the running game is next in the queue.

## Coming next

Designed and queued, roughly in the order they depend on each other:

- **Wind.** Outdoors, scent is carried ahead downwind and held back upwind; indoors there is no
  wind, and scent builds up behind inner walls.
- **Soak and rot.** Tiles and objects hold a capped memory of scent that leaks away slowly.
  Crowds of the Dead rot, and rot dampens how strongly they pull on each other and on you.
- **Sound.** Footsteps, breaking glass, gunshots and ambient noise as distinct kinds the Dead
  react to, over an ambient noise floor.
- **Scent that stays near you.** Detailed scent is simulated only in reach of the player;
  sealed buildings are skipped until opened; routes run between floors.
- **Items as seeds.** Every container knows what it *could* hold, and resolves only when you
  open it, drawer by drawer.
- **Random characters.** A background, physique, conditions, a job and where you were when it
  happened, with numeric physical stats and five skill bands.
- **Crowd physics.** Trips, contests and trample on shared tiles; alerted Dead that push past
  the calm ones.

## Further out

- **One clock for the whole world.** The global days-since-Event clock drives decay: early is
  richer and more dangerous, late is safer and picked over. Utilities fail, fires burn out,
  rivers cut the map.
- **Hard permadeath with consequences.** When you die, your character rises as one of the Dead,
  one that *can* see, carrying your stats and gear, unless you made sure it couldn't. Your next
  survivor starts a mile or two away in the same persistent world.
- **A mind under pressure.** Mood, from depressed to manic, and Sanity are core mechanics. Low
  sanity makes the world unreliable. Drugs, psychedelics included, are real choices with real
  costs.
- **No NPCs, no wildlife.** The story is told through what you find and the notes survivors
  left behind. Every animal is inexplicably dead; crows are heard, never found.
- **Swappable fonts and tilesets** on top of the ASCII presentation.

---

## Under the hood

- **C++20, SDL3, CMake**, on Linux and Windows. Warnings are errors; `.clang-format` is enforced.
- **A deterministic core** (`src/core`): no SDL, no I/O, no globals. Every mechanic is
  implemented and unit-tested here first; the SDL3 frontend (`src/app`) only draws and reads
  input.
- **CPU integer fields.** Scent is a propagating min-plus field with an incremental patch for
  the player's move, rather than floating-point diffusion, so results are bit-identical across
  machines and cheap to commit.
- **Performance as a feature.** No allocation in per-tick loops. The budget is the gap between
  keypresses: a speculated update under 100 ms and a commit under 1 ms on an M1 MacBook Air,
  measured at 200×120 with 5,000 Dead and at 512×512 with up to 50,000.
- **Headless tests** with doctest: unit, golden and town-scale scenario tests, plus content
  lint and content expectation tests.

## Build and run

```sh
python3 tools/bootstrap.py          # once: git hooks, toolchain report
cmake --preset dev                  # or: release · headless · win-dev
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/peo [world-seed]
python3 tools/verify.py             # the gate: docs, content lint+tests, format, build, tests
```

The `headless` preset builds only the simulation and tests, with no SDL. SDL3 is found on the
system or fetched from source.

## Repository layout

| Path | What |
|------|------|
| `src/core/` | The simulation library. Deterministic, no SDL, fully unit-tested. |
| `src/app/` | The SDL3 frontend: window, input, drawing. |
| `tests/` | doctest suite for `core`, plus content expectations and chains in `tests/content/`. |
| `content/` | Game data: registry, materials, substances, items, modifiers, world. Schema in `content/README.md`. |
| `tools/` | The verify gate, the work queue tool, doc and content linters, git hooks. |
| `docs/` | Start at `docs/README.md`. Vision and decisions in `docs/production/`, architecture in `docs/adr/`, design in `docs/design/`. |
| `WORK_QUEUE.json` | The backlog, managed with `tools/work_queue.py`. |

## How it's made

Post Event: Onwards is designed by one developer and built with AI coding agents working in
two roles: an **Architect** that plans, writes briefs and headless tests, and reviews, and a
**Builder** that implements and plays the game on real hardware. Gameplay and direction stay
with the designer; every call is recorded in `docs/production/decisions.md`. The process is
in `docs/roles.md` and the shared rules in `AGENTS.md`.

## License

[MIT](LICENSE). Dependency licences and policy: `docs/LICENSING.md`.
