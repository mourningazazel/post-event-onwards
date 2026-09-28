# Playtest harness — how Claude plays and tests the game (for review)

Origin: owner note [N001](notes/N001-claude-playtesting.md).

Status: **proposal.** The harness is part of the framework, not a later add-on. It is built in the
first engine phase, before any gameplay system, so that **every** feature is played as well as
unit-tested from the day it lands (rule R15 in `SYSTEMS.md`).

## Goal

Claude must be able to:

1. **Play the real game** turn by turn through keyboard input, and see exactly what a player sees.
2. **Set up any situation on demand:** place, unit counts and types, gear, scent, wind, time.
3. **Run full gameplay cycles** unattended, for thousands of turns.
4. **Catch gameplay-level bugs,** where one mechanic breaks another, not only crashes.
5. **Turn every problem found into a permanent, replayable test.**

## Why the architecture already supports this

These earlier decisions make the harness cheap to build:

| Existing decision | What it gives the harness |
|---|---|
| Deterministic simulation (seeded RNG streams, integer maths) | The same seed and inputs give the same game, so every run can be replayed exactly |
| Every intent is a **command** (player and AI) | A key script and a live player drive the identical code path |
| The renderer draws from a **render snapshot** of a character grid | The screen can also be emitted **as text**. Claude reads the real game screen, not a debug view. |
| Every system runs headless (R11) | No window or GPU needed; runs in CI |
| Systems expose stats and state (R10) | Structured state is available for checking beyond what's on screen |

## Components

### 1. Headless play (`rl-play`)

- **Same game, no window.** `rl-play` runs the full game. It renders the UI grid to a **text
  backend**: every cell is printed as its character, with an optional colour/legend layer.
- **Interactive mode.** Claude sends keys (`h j k l`, `g` for pick up, …) and gets the new screen
  back after each step. Same keys and screens as a human player.
- **Agent protocol mode.** One JSON line in (keys or commands), one JSON line out. The reply holds
  the screen text, the message log, and optional structured state (player status, visible units,
  and, with debug rights, the scent grid).
- **Image capture.** A PNG of a frame from an offscreen GPU render. Claude can read images, so
  real tilesets, colours and layout can be checked visually.

### 2. Scenarios (data files)

A scenario defines:

- seed
- world source: a hand-made test arena, or a real generated location
- player character and loadout
- spawns: unit type, count, placement pattern, initial state (idle, alerted)
- scent sources, wind, time of day
- step limit
- **expectations**

Any value can be overridden from the command line, e.g. `--set spawns.zombie.count=20000`.

Starter set (grows with each feature):

- `arena-empty`
- `corridor-horde`
- `open-field-10k`
- `downwind-approach`
- `upwind-approach`
- `masked-scent-walkthrough`
- `alert-cascade-city-block`
- `swap-contest-doorway`
- `dig-under-building`
- `loot-promoted-zombie`
- `nested-containers-stress`
- `save-load-mid-cascade`
- `walk-between-towns`
- `permadeath-ends-run`
- `turned-80-vs-99`
- `clock-sweep-7-to-1825`
- `revisit-after-months`
- `fire-collapse`
- `encampment-overrun`
- `rural-well-water`

### 3. Scripted and automatic players

- **Key scripts:** exact key sequences for a specific playthrough.
- **Bot policies** play full cycles without a human:
  - explore
  - flee from the horde
  - fight everything
  - loot everything
  - dig down
  - travel to the next town
  - hide and wait
  - **monkey** (random valid keys, which finds crashes, dead ends and UI states with no way out)
- Bots use only the player's commands. No cheating paths.

### 4. Gameplay rule checker (runs every step during tests)

- The owner-approved gameplay rules ([walkthrough G13](gameplay-model/README.md)) are checked
  after every step. Examples:
  - two units never occupy one cell
  - items never vanish except by an explicit destroy event
  - containers never exceed capacity
  - every mob is in a valid state
  - saving then reloading changes nothing
- A violation stops the run and writes a **replay file** (seed, scenario, commands) that
  reproduces it exactly.

### 5. Expectations: gameplay assertions in tests

Scenarios declare outcomes. For example:

- "Approaching upwind, fewer than 5% of mobs reach the player within 200 steps."
- "The zombie's pockets are generated when searched, and contain items consistent with its former
  occupation."
- "A trip during a swap contest never leaves a unit inside a wall."
- "One grunt alerts at most *N* units (the chain-depth cap works)."

### 6. Replays, regression and metrics

- **Replays.** Every run records a replay. A failure or suspicious run becomes a regression test
  with one command.
- **Per-run gameplay metrics:**
  - steps survived
  - damage taken and dealt
  - items found
  - mobs alerted, and alert chain depth
  - how often the player was surrounded
  - time per step
- **Balance tracking.** Metrics are compared across builds to catch balance shifts, e.g. an alert
  cascade that suddenly pulls in a whole city, or a mechanic that makes the game trivial.

### 7. Debug and wizard commands (playtest builds only)

These are reachable from the keyboard and the agent protocol:

- spawn, teleport, reveal map
- set scent, set wind
- advance time
- inspect any thing
- force-promote a zombie
- dump state

### 8. Playtest journal

When Claude plays exploratively (not scripted), findings go into
`docs/playtests/<date>-<topic>.md`. Each entry records what was tried, what felt wrong, and the
replay file. Every confirmed issue becomes a test before it's fixed.

### 9. Purpose tests: intended uses stay possible (owner note N002)

- **Every gameplay function** has an entry in the [purpose catalog](purposes.md) saying what it's
  *for*.
- **Every intended use** has a purpose test: a scenario plus a key script or bot, with an
  observable outcome as evidence.
- **The purpose suite is the long-term regression net.** It runs on every change, and tests are
  never deleted silently. If a new mechanic makes an old intended use impossible, e.g. a crowd
  change that makes scent masking useless, CI fails and names the broken purpose.
- **When Claude is unsure what something is for, it asks.** The entry stays `question`, and gets no
  test, until the owner answers.

## What "gameplay bugs" means here: cross-system hunting list

Playtests deliberately combine mechanics. Categories to probe:

| Category | Example probes |
|---|---|
| **Sense × behaviour** | Scent masking plus the alert cascade: can a masked player still set off a city-wide cascade by grunting? Should they? |
| **Movement × terrain** | A swap-contest trip next to a freshly dug hole: does the unit fall? Does it end up inside terrain? |
| **Terrain × structures** | Digging under a building: does the floor above hold, collapse, or float? Do items on it fall? |
| **Items × containment** | Splitting and merging stacks inside nested containers: duplication or loss? Is capacity enforced through parts? |
| **Detail levels × persistence** | Promote a zombie, loot half its pockets, walk away, return: are the same zombie and pockets there? |
| **Save × mid-action** | Save during an alert cascade or halfway through a dig: does the reload continue identically? |
| **Aggregation × individuals** | A horde merges into aggregate population and spawns back: are counts and wounded units conserved? |
| **Death × succession** | Does the heir spawn inside a horde? Does the old body rise with the right gear? |
| **Exploits** | Infinite items, trivial safety (e.g. standing in a doorway), scent tricks that remove all danger |
| **Dead ends** | Can the player get permanently stuck (terrain, UI state, overload)? Is that acceptable in this realistic setting, or a bug? |

## Owner decisions needed

- **D-PT1:** Should the owner-facing game ship with the wizard/debug mode? **Owner decision
  (N003): yes.** Clearly marked; wizard runs are flagged and excluded from scores.
- **D-PT2:** The gameplay rules list (G13). Claude drafts it; the owner approves which rules are
  laws and which may break for realism. **Owner principle (N003): realism over protection.**
  - Getting permanently stuck, or dying to bad luck or a bad decision, is intended, not a bug.
  - The rule checker enforces only *simulation integrity* (e.g. item conservation, no overlapping
    units, save/load identity), never *player safety*.
