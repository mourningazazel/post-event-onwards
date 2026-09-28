# Framework and system map — proposal for review

Status: **proposal.** The owner will set the design process and phase structure. This document
gives that process a starting inventory of major systems, and the engineering rules every system
follows.

Goal: build the game as a **framework of well-bounded systems with stable internal APIs**, so
later features are compositions of existing building blocks, and changes stay local.

---

## 1. Framework rules (apply to every system)

| # | Rule | Why it reduces future rework |
|---|---|---|
| R1 | **A system is a module.** Public API in `include/<system>/`, implementation private, one CMake target per system. Other systems use only the public API. | Implementation can be rewritten without touching callers |
| R2 | **The dependency graph is layered and acyclic,** checked in CI (target link graph). Lower layers never include higher ones. | Stops the "everything depends on everything" slide |
| R3 | **Communication has three channels:** direct API calls *downward*; typed, queued **events** *upward and sideways*; **commands** for intents (player input, AI decisions). | Systems can be added or removed without editing each other |
| R4 | **Batch-first, data-oriented APIs:** operations take spans and views, not one call per entity; no virtual call per entity per step. | Performance is built into the API shape, not bolted on later |
| R5 | **Stable IDs and generational handles** across system boundaries; never raw pointers. The same IDs are used in saves. | Serialization, networking, undo and replay stay possible |
| R6 | **Content is data.** Definitions (tiles, materials, creatures, items, scents, generators) live in a **Content Registry** under stable string IDs, loaded from files. | New content ≠ new code; mod support comes almost free |
| R7 | **Each system owns its save section** with its own version number and migration path (see ADR-0002). | Save formats evolve per system without breaking the whole save |
| R8 | **Determinism contract:** systems receive their RNG streams and the step clock through their context. No global RNG, no wall-clock time in simulation, integer maths wherever results feed back into the simulation. | Replays, bug reproduction and golden tests |
| R9 | **Budgets are explicit.** Heavy work is submitted as prioritized jobs to the job system; the **Budget Manager** enforces memory and time ceilings. | Scaling knobs live in one place, not in scattered constants |
| R10 | **Every system is observable:** stats and metrics API, a debug-UI panel, Tracy zones, and structured log categories. | Performance problems can be diagnosed, not guessed at |
| R11 | **Every system runs headless** for tests. Each GPU kernel has a CPU reference implementation and an equivalence test. | CI without a GPU; protection against driver bugs |
| R12 | **No singletons.** Dependencies are passed explicitly in a context struct at construction. | Testability, and multiple worlds (e.g. a gen-preview tool) side by side |
| R13 | **Configuration is layered:** defaults → data → user config → command line, typed and validated in one place. | Tuning without recompiling |
| R14 | **Architecture decisions are recorded as ADRs** (`docs/adr/`) before implementation. | Future contributors and future-us know *why* |
| R15 | **The game is playable headless from day one** (owner note N001, `design/playtest-harness.md`). Every system exposes its state to the gameplay rule checker. Every feature lands with at least one **scenario plus playtest** (a key script or bot policy with expectations), not only unit tests. Every gameplay function declares its **intended uses** in the purpose catalog (owner note N002, `design/purposes.md`). Each use gets a **purpose test** that stays in the regression suite permanently. | Claude plays the real game throughout development. Cross-system gameplay bugs are caught when they're introduced, and each one becomes a permanent replay test. |

## 2. Proposed major systems (bottom layer first)

```
Game          Rules/Effects · Combat · AI/Behaviour · UI Framework · Screens/States · Audio
                                   │
Simulation    Sim Core (ECS, step clock, scheduler) · Perception · MobMovement ·
              Navigation (NPC pathfinding) · Items & Containment · Fields (scent, …)
                                   │
World         Spatial Model (coords, chunks, tile layers, z) · Generation Framework (L0–L5) ·
              Streaming/LOD Manager · World Database (persistence)
                                   │
Engine        GPU (device, buffers, kernels) · Renderer (grid, tilesets) · Input & Commands ·
              Content Registry · Events
                                   │
Foundation    Core (containers, handles, hashing, RNG, maths) · Diagnostics (log, assert,
              profiling, metrics) · Jobs & Budget Manager · Platform (window, filesystem, time) ·
              Config · Serialization
                                   │
Tools         Playtest Harness (rl-play, scenarios, bots, rule checker, replays) ·
              Map-gen viewer · Content validator · Replay/diff tool · Benchmark harness
```

The **Playtest Harness** is built alongside the first engine systems, not at the end. It needs
three things:

- Input & Commands, for scripted keys.
- A text backend for the Renderer, so Claude can read the screen.
- The determinism contract (R8), so runs can be replayed.

Design-relevant notes per system:

- **Spatial Model.** The canonical coordinate types (64-bit world, chunk, local), tile-layer
  structure-of-arrays layout, z-levels, and read-only views handed to other systems. *Almost
  everything depends on this, so it should be designed first and carefully.*
- **Generation Framework.** Not the generators themselves: the *contract* (pure function of seed,
  coordinates, parent and version), level definitions, and a registry of generator plug-ins.
  Individual generators (cities, rivers, caves) are then plug-ins, and can be added one at a time.
- **World Database.** Stores materialized chunks, structures and entities. Keyed by stable IDs and
  coordinates, versioned, zstd-compressed. Writes asynchronously.
- **Fields.** A generic "scalar field over the grid" service: channels, emitters, per-channel
  diffusion/decay rules, CPU and GPU backends. Scent is the first client; heat, noise, light,
  gas and fire can follow.
- **Items & Containment.** One item life-cycle API, containment as a relationship, and a stack
  model. Research flagged this as a classic rewrite trap if it isn't designed once, early.
- **Renderer.** Consumes a *render snapshot*: it never reads simulation state directly. Tile
  appearance comes through logical tile names (see `GAMEPLAN.md` §3.1). Single cell size
  (ADR-0003).
- **Input & Commands.** All player and AI intents are commands, which makes replays and rebinding
  natural.

## 3. Suggested design order (subject to the owner's process)

1. Framework conventions, meaning this file and the repo layout
2. Core (IDs, handles, RNG, hashing) and Diagnostics
3. Jobs and Budget Manager
4. Platform and Config
5. Serialization and the World Database contract
6. Content Registry
7. **Spatial Model**
8. Renderer and Input/Commands
9. Generation Framework and Streaming/LOD
10. Sim Core (ECS decision recorded as an ADR here)
11. Fields → Perception → MobMovement
12. Items & Containment → Navigation → Rules/Effects → UI

Each system design produces:

- purpose and non-goals
- public API sketch (headers)
- data layout
- events it emits and consumes
- its save section
- budgets and metrics
- a test plan
- ADRs for any decisions made
