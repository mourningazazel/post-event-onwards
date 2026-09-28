# Roguelike — base-engine gameplan (v0.2, for review)

Status: **proposal.** No game code exists yet.

**v0.2 (2026-09-27)** records the owner's decisions (§6, `adr/`). Parts of the plan are now
superseded:

- The **§5 roadmap** is replaced by the owner's upcoming phase approach: a framework-first,
  system-by-system design (see `SYSTEMS.md`).
- **Enemies:** see `design/scent-mobs.md`.
- **World:** see `design/world-generation.md`.

Research backing this plan is in `research/roguebasin-notes.md`. Items marked *(external)* come
from outside RogueBasin, because the wiki does not cover them.

---

## 0. Constraints found during research

| Fact | Consequence |
|---|---|
| Dev machine: Apple M1 (aarch64) on Asahi Linux, 8 cores (4 performance + 4 efficiency), **8 GB RAM shared with the GPU** | Memory budget is real. The world must stream; no whole-world dense arrays. |
| GPU driver: Mesa 26.2.3 **Honeykrisp**, a conformant Vulkan driver. No CUDA or Metal on this OS. | GPU work must go through Vulkan, i.e. SDL_GPU's Vulkan backend |
| **Verified:** SDL3 3.4.16 creates a GPU device on Vulkan/Honeykrisp. A GLSL compute shader compiled to SPIR-V ran over 1,048,576 elements; dispatch + readback took **0.9 ms** and the results were correct (`research/spikes/gpu-compute-probe/`). | GPU offload needs no extra library: SDL3 already has compute |
| Arch repos have GCC 16, Clang 22, CMake 4.4, Ninja, glslang/glslc, SPIRV-Cross, SDL3_image/ttf/mixer, fmt, spdlog, zstd, catch2, doctest | Toolchain is ready. flecs, FastNoise2, enkiTS, Tracy and ImGui would come in through CMake. |

---

## 1. What the research says (one screen)

1. **Mass pathfinding** means **shared Dijkstra maps (flow fields)**, not one A\* per agent. One
   field per goal serves unlimited agents, and each agent's per-turn cost is O(1). Monsters
   combine several fields with weights: chase, flee, food, noise, herd (Brogue's "desire-driven
   AI"). Traffic jams are fixed by an idle-time occupancy cost layer.
2. **Field-shaped problems are GPU-shaped problems.** Dijkstra relaxation, scent/sound/heat
   diffusion, cellular-automata caves and noise terrain are all stencil passes over a grid.
3. **Field of view** means recursive shadowcasting, which is the fastest in the benchmark study.
   A symmetric variant pays off at scale: if FOV is symmetric, *every monster inside the player's
   FOV can see the player*. That turns thousands of "does it see me?" checks into one FOV pass
   *(Albert Ford's symmetric shadowcasting, external; libtcod ships it)*.
4. **Time** is handled with an absolute-time bucketed scheduler (timing wheel). Naive energy
   systems and the wiki's priority-queue reference are O(n) per tick and don't scale.
5. **World** storage: chunks in a hash map, dense structure-of-arrays tile layers plus a flag
   bitfield, sparse entities. `chunk_seed = hash(world_seed, cx, cy, z)` makes terrain
   regenerable, so saves store only deltas.
6. **Generation** architecture follows *Abstract Dungeons*: generate a coarse topology graph,
   then dispatch independent "painters" per area. That is the scalable, parallel pattern for
   overworld, districts and city blocks. RogueBasin has no city-generation article; road growth
   and lot subdivision come from external literature *(Parish & Müller 2001, external)*.
7. **Architecture:** ECS with stable integer IDs that double as save-file references, a
   data-driven Effect system shared by combat, crits, monster attacks and spells, declarative
   content files, and scripting later if at all.
8. **Rendering:** the consensus is a thin custom grid renderer modeled on libtcod and
   BearLibTerminal, not a dependency on either.

---

## 2. Proposed stack

| Concern | Choice | Why | Rejected alternatives |
|---|---|---|---|
| Language | **C++20** (use C++23 features only when GCC, Clang and MSVC all support them) | Your existing C++ toolchain and experience. All chosen libraries are native C/C++. Easy SIMD and data-oriented layouts. | Rust + Bevy (a whole second ecosystem to learn; the ECS drives everything). C# (GC pauses at mass-sim scale). |
| Build | **CMake + Ninja**, `CMakePresets.json`, **CPM.cmake** with pinned versions | Reproducible on Linux and Windows. System SDL3 on Arch, fetched elsewhere. | Meson (fine, but SDL, flecs and FastNoise2 all ship CMake builds). vcpkg (keep as an option for Windows CI). |
| Window, input, audio, filesystem | **SDL3** | Required. Also provides pref/base paths, gamepad input and the GPU API. | — |
| Rendering | **Custom grid renderer on SDL_GPU** (not `SDL_Renderer`) | See §3.1. The whole console is one instanced draw per layer, and shaders handle tinting, colour and effects. **The same device runs our compute work.** | libtcod's console (one tileset and cell size per console; nothing to share with our compute). BearLibTerminal (unmaintained, legacy OpenGL). `SDL_Renderer` (fine for a prototype, but it can't share buffers with compute). |
| Shaders | **GLSL → SPIR-V with `glslc` at build time.** Vulkan backend on Linux and Windows (ADR-0001). | Toolchain already installed, no runtime shader compiler, one language. | HLSL + SDL_shadercross (add later only if we want D3D12 on Windows or Metal on macOS; it pulls in DXC and SPIRV-Cross) |
| PNG loading | **SDL3_image** | Already in repos; returns `SDL_Surface`. | stb_image (also fine; keep it as a zero-dependency fallback) |
| ECS | **flecs v4** *(leaning; the owner delegated the final call until the Sim Core system design, where it will be recorded as an ADR)* | Relationships (item *in* container, unit *in* squad, creature *member of* faction). Prefabs map directly onto data-file monster templates. Built-in system pipeline and multithreading. Web-based live world explorer for debugging. The C core compiles fast. | **EnTT**: faster raw iteration and header-only, but template-heavy (compile times), and we'd write scheduling, relationships and prefabs ourselves. Solid runner-up. |
| Job system | **enkiTS** | Tiny, battle-tested, does parallel-for over ranges and main-thread-pinned tasks. For world-gen chunk jobs and CPU pathfinding batches. | Taskflow (heavier, graph-oriented). `std::execution` (not shipped yet). |
| Noise | **FastNoise2** | SIMD with runtime dispatch, **including NEON on this M1**. Node-graph noise with a visual editor tool for designing terrain. | FastNoiseLite (simpler, scalar, fine for prototypes). libtcod's noise. |
| RNG | **PCG32 / xoshiro256\*\*** streams plus a **counter-based hash** (SplitMix64-style) for position-seeded generation | Small state, fast, seekable. The hash makes chunk generation independent of generation order. **We won't use `std::*_distribution`**: its output differs between libstdc++ and MSVC, which breaks cross-platform seeds. | Mersenne Twister (2.5 KB state, slow seeding). libc `rand()` (never). |
| Pathfinding, FOV, scheduler | **Our own code** | No library covers mass grid flow fields plus hierarchical pathfinding on our chunk layout. FOV is about 200 lines. libtcod serves as a **test oracle** for FOV and A\*. | libtcod as a runtime dependency (its pathfinding is single-agent) |
| Content data | **TOML (toml++)** for definitions: tiles, materials, creatures, items, effects, tilesets | Human-editable, supports comments, fast. Matches the wiki's "info file" rules. | JSON (no comments). Lua from day one (deferred; see Phase 7). |
| Scripting | **Deferred.** If needed later, Lua 5.4 via sol2. | Don't pay for it until behaviour, not data, is the bottleneck. | — |
| Save format | Versioned binary chunks, **zstd**-compressed, checksum per section, stable string IDs for content with a remap table | Seed regeneration plus deltas keeps saves small. Checksums catch corruption (the wiki's objection to length-prefixed formats). | Plain-text saves (the wiki's preference; too slow and large at our scale) |
| Debug UI | **Dear ImGui** with the official `imgui_impl_sdlgpu3` backend | Inspectors, generator tweak panels and profiling overlays. **Debug builds only.** The game UI is drawn on the grid. | — |
| Profiling | **Tracy** | Frame, zone, lock and memory profiling from day one. A performance-heavy game needs it built in, not added later. | — |
| Logging | **spdlog** (fmt) | In repos, fast, supports sinks. | — |
| Tests and benchmarks | **doctest** (fast compile) + **nanobench** | Unit tests, golden-seed determinism tests, CPU-vs-GPU equivalence tests, micro-benchmarks. | Catch2 (heavier compile). Google Benchmark (heavier). |
| Audio | SDL3_mixer, later | — | — |

All licenses are zlib, MIT or BSD, so any later open- or closed-source decision stays open.

---

## 3. Architecture

```
┌────────────────────────── main thread ──────────────────────────┐
│ SDL events → input mapping → Commands ──┐                        │
│                                          ▼                        │
│   UI (grid widgets) ◄── render snapshot ◄─── Simulation (worker) │
│        │                                       │  ▲               │
│        ▼                                       ▼  │ results       │
│   Grid renderer (SDL_GPU draw) ──── shared GPU device ──► Compute │
└──────────────────────────────────────────────────────────────────┘
```

Modules (static libraries, dependencies point downward only):

| Module | Contents |
|---|---|
| `core` | RNG, hashing, math, containers, job system, logging, profiling |
| `platform` | SDL window, input, paths, timing |
| `gpu` | Device, buffers, pipeline cache, kernel registry |
| `render` | Tilesets, grid layers, camera |
| `world` | Chunks, tile layers, z-levels, streaming, persistence |
| `gen` | Generation pipeline stages |
| `sim` | ECS, scheduler, movement, FOV, pathfinding, AI |
| `ui` | Grid widget tree |
| `game` | Content loading, rules, effects |
| `app` | Executable, and tools (e.g. a map-gen viewer) |

### 3.1 Grid renderer (the "Dwarf Fortress presentation")

- **Cell** is `{ u32 glyph, u32 fg_rgba, u32 bg_rgba, u32 flags }`, 16 bytes.
- **Layers.** A frame is a stack of grid layers: map, items and creatures, effects, overlays, UI.
  Each layer has its own tileset, but **every layer uses the same cell size**
  ([ADR-0003](adr/0003-single-cell-size.md)).
- **Tileset = PNG atlas + TOML mapping.**
  - Default layout is the CP437 16×16 sheet, so Dwarf Fortress / Cogmind / REXPaint-style fonts
    work as-is. Sheets may add extra rows beyond 256 glyphs.
  - Glyphs are white on transparent and tinted with `fg` in the shader. A per-tile flag marks
    **full-colour tiles** that are drawn untinted (graphics packs).
- **The indirection that makes swapping work.** Game content never references atlas slots. It
  references **logical tile names** (`wall.granite`, `creature.goblin`, `ui.box.nw`).
  - The base tileset maps every name to a codepoint plus default colours, e.g. `'#'`, grey.
  - A graphics pack can **override any subset** of names with sprites. This is how DF graphics
    sets work.
  - Swapping the font, or switching ASCII ⇄ tiles, is a runtime data reload. No game logic
    changes.
- **Drawing.**
  - Each frame, the renderer writes the visible cells of each layer into a GPU storage buffer
    through a transfer buffer.
  - The vertex shader builds quads from `instance_id`. That's **1 draw call per layer**.
  - Scale: 1080p with 8 px cells is 240×135 ≈ 32k cells per layer. Even at 4K with several
    layers that is well under 1M instances, trivial for any GPU.
  - Integer scaling and zoom (Cogmind-style map zoom) come from changing cell size, not from
    resampling.
- **Effects** (flashes, projectile trails, glow) are shader parameters and short-lived overlay
  cells, not a separate sprite system.

### 3.2 World representation

- **Chunk** = 32×32 tiles × 1 z-level. The world is a hash map from `(cx, cy, z)` to chunk.
  Layers are stored structure-of-arrays:
  - `terrain` u16
  - `material` u16
  - `flags` u32: blocks move, blocks light, liquid, has item, has creature, seen, mapped, …
  - optional sparse overlays: liquid depth, temperature
- **Active window.** The player's region is kept resident. Example: 512×512 tiles × 16 z-levels
  × 8 B = **32 MB**. Chunks outside it are evicted. Unmodified chunks are *dropped* and
  regenerated from the seed; modified chunks write a delta.
- **Entities** live in the ECS, not in tiles. A per-chunk spatial index keeps "what's here"
  queries cheap. The occupancy grid is a dense layer, because pathfinding and crowds read it
  every turn.
- **Z-levels from day one**, even if early content is flat. Retrofitting a third coordinate is
  one of the rewrite traps the wiki warns about.

### 3.3 Simulation, time and determinism

- **Deterministic simulation.** Given the same seed and the same commands you get the same
  world. This gives replays, one-command bug reproduction, and golden tests. Rules:
  - Seeded RNG streams per system.
  - Stable iteration order.
  - No `std` distributions.
  - Integer math in anything that feeds back into the simulation. GPU kernels on integer fields
    are exactly reproducible.
- **Scheduler** is an absolute-time timing wheel: near-term buckets plus a heap for far-future
  events. Buffs, damage-over-time and regeneration go through the same queue as actor turns.
- **Actors due on the same tick are processed as a batch:**
  - (a) *intents* are computed in parallel (enkiTS parallel-for; read-only on world state);
  - (b) intents are *resolved* serially in a deterministic order (movement claims, attacks).

  This is how thousands of actors use 8 cores without losing determinism.
- **Simulation level of detail:**

  | Tier | What it covers | How it runs |
  |---|---|---|
  | **Active** | Near the player | Full AI every turn |
  | **Near** | Loaded but off-screen | Reduced cadence, cheap AI (field-following only) |
  | **Abstract** | Everything else | Groups and armies move on the world or chunk graph as aggregate objects. They become individuals only when they enter the loaded area (a DF-like approach). |

- The simulation runs on a worker thread. Rendering and UI stay responsive during heavy turns.

### 3.4 Pathfinding and crowds (the performance core)

> **v0.2:** the main enemy population is **blind scent-following mobs** and needs **no
> pathfinding** ([ADR-0006](adr/0006-scent-driven-mobs.md), `design/scent-mobs.md`). The tiers
> below remain for sighted NPCs, the player's auto-move and long-distance travel. The "diffusion
> fields" paragraph at the end of this section is now the **core** system, not a side feature.

| Tier | Used for | Implementation |
|---|---|---|
| **T1 — goal fields** | Hordes chasing the player, factions converging, flee, food, noise, autoexplore | Integer Dijkstra maps over the active window. Multi-source, recomputed when goals move, incrementally if possible. Agents take the weighted-desire argmax over neighbouring cells. **CPU:** bucket-queue BFS. **GPU:** tiled relaxation (see §4). |
| **T2 — hierarchical (HPA\*)** *(external)* | Long trips, off-screen armies, city-to-city travel | Portal graph on chunk borders, cached, and invalidated per chunk when terrain changes |
| **T3 — individual A\*** | The player's click-to-move, bosses, special agents | A\* on the grid with a binary heap and path caching. Validate the next step; don't re-plan every turn. |
| **Crowd layer** | Everyone | Occupancy grid, idle-time cost (anti-jam), cell claiming during intent resolution, and the swap rule for friendly agents |

Scent, sound, "last known player position" heatmaps and herd density are all **diffusion
fields**: a splat step, then blur and decay. They use the same kernel infrastructure as T1.

### 3.5 FOV and lighting

- **Symmetric shadowcasting** for the player. Monsters in the player's FOV know the player is
  visible (it's symmetric).
- For monster-to-monster sight, check on demand with Bresenham line of sight, cached per turn.
- Light sources run shadowcasting from their own position into a light grid. With many lights,
  this can become a GPU kernel later.
- **Cone/arc FOV** (Spiral Path style) for facing-based vision, if the design wants stealth.

---

## 4. GPU compute plan

**Principle:** GPU is an *accelerator*, never the only implementation.

- Every kernel has a **CPU reference implementation**, and a test asserts the GPU output is
  identical.
- The game runs with `--no-gpu-compute`.
- Keeps us debuggable, and protects us from driver bugs (Asahi is young).

**Candidates, in order of expected payoff:**

1. **Batched Dijkstra fields.** Several goal fields computed in one dispatch (one per faction or
   goal).
   - Naive global Jacobi relaxation needs roughly one pass per step of the longest path, which is
     slow on big maps.
   - Instead: 16×16 tile blocks relaxed in shared memory until each is locally stable, repeated
     globally until no block changes (block Fast Iterative Method style *(external)*), or fast
     sweeping.
   - **Honest caveat:** a single field on a 512² map may be just as fast on CPU (bucket BFS takes
     a few ms). The GPU wins on *many fields at once* and on *large windows*. Phase 5 measures
     this before we commit.
2. **Diffusion fields**: scent, sound, heat, fire spread, gas, search heatmaps. Pure stencils,
   the best GPU fit.
3. **World generation stencils**: cellular-automata cave passes, noise post-processing, erosion
   and flow accumulation for rivers, template matching.
4. **Lighting** with many sources.

**Mechanics.** Dispatch at the end of turn N and consume the results at the start of turn N+1,
so the CPU never stalls on the GPU. Apple silicon's unified memory keeps readback cheap (measured:
0.9 ms for 4 MB including the dispatch).

**Not on GPU:** A\*, HPA\*, scheduling, AI decisions. They are branchy, sequential and small.

---

## 5. Roadmap for the base (each phase ends at a review gate)

> **Superseded in v0.2.** The owner will define the phase approach, with more steps up front and
> the framework designed system by system. The phases below are kept for reference, since their
> exit criteria may be reused.

### Phase 0 — Decisions and spikes *(next step)*

- **Deliverables:**
  - Answers to §6.
  - Two throwaway spikes under `spikes/`:
    - (a) Grid renderer prototype: CP437 PNG loaded, 3 layers, runtime tileset swap, frame time
      at a 4K window.
    - (b) Field benchmark: CPU bucket BFS vs GPU tiled relaxation on 512², 1024² and 2048²,
      with 1, 8 and 32 simultaneous fields.
- **Exit:** numbers in hand. Confirm or revise §2 and §4.

### Phase 1 — Skeleton

- **Deliverables:**
  - git repo with a CMake preset per platform
  - CPM-pinned dependencies
  - CI on Linux (aarch64 + x86_64) and Windows
  - app loop: event pump + simulation worker + render
  - input-to-command mapping
  - SDL paths for data, saves and config
  - spdlog and Tracy wired in
  - doctest and nanobench targets
- **Exit:** CI green, and the empty window profiles in Tracy.

### Phase 2 — Grid renderer and tilesets

- **Deliverables:**
  - tileset TOML format
  - logical-name registry
  - layers with independent cell sizes
  - tint vs full-colour tiles
  - camera, scrolling, integer zoom
  - hot-reload of tilesets
  - ImGui debug overlay
- **Exit:** swap two fonts and a sprite pack at runtime; 4 full-screen layers under 1 ms of GPU
  time per frame.

### Phase 3 — World core

- **Deliverables:**
  - chunks with structure-of-arrays layers
  - z-levels
  - tile and material definitions from TOML
  - active-window streaming
  - seeded chunk generation (a trivial noise generator for now)
  - delta saves with zstd
  - golden-seed tests
- **Exit:** walk a camera across a 16k×16k seeded world with flat memory use. Saving then
  reloading reproduces the world byte-for-byte.

### Phase 4 — Simulation core

- **Deliverables:**
  - flecs integration
  - timing-wheel scheduler
  - player entity, movement and collision
  - symmetric FOV and fog of war
  - message log
  - deterministic replay (record commands, replay them, compare state hashes)
- **Exit:** replays are bit-identical across Linux and Windows builds.

### Phase 5 — Pathfinding and crowds

- **Deliverables:**
  - T1 fields (CPU), T3 A\*, crowd layer
  - stress scene: **10k agents** chasing the player through a cave map
  - then the GPU field kernel with equivalence tests
  - then T2 HPA\*
- **Exit:** the performance targets agreed in §6 are met on the M1 8 GB machine, measured in
  Tracy.

### Phase 6 — Generation pipeline v1

- **Deliverables:**
  - macro layer: fBm height, moisture and temperature → biomes → rivers (flow accumulation)
  - regions: zone flood-fill → per-zone painters
  - caves: cellular automata on GPU plus a connectivity pass
  - city v1: site selection → road skeleton (arterials plus growth) → blocks → lots → buildings
    by template stamping
  - a **standalone map-gen viewer tool** (ImGui sliders, seed browser)
- **Exit:** regenerating any chunk from its seed is deterministic, and generation time per
  region is within budget.

### Phase 7 and beyond — Game layer (out of scope for "the base")

Effects system, items and inventory, combat, AI desires and behaviours, factions, UI screens, and
the Lua decision.

---

## 6. Decisions log (owner answers, 2026-09-27)

| # | Question | Decision | Record |
|---|---|---|---|
| 1 | Platforms | **Linux + Windows.** macOS dropped. It would need Metal: a second shader toolchain, a separate test matrix, and signing. | ADR-0001 |
| 2 | Time model | **Player moves in discrete steps. The world is not fully simulated every step**, and fidelity follows detail rings. | ADR-0005 |
| 3 | World | **Endless, generated regionally in stages** (identities first, then detail as the player approaches). Region context includes terrain and not-yet-generated neighbouring cities. Persistent down to items on each tile. Ranges are budget-driven. | ADR-0007, `design/world-generation.md` |
| 4 | Enemies and scale | **Blind scent-following mobs** are the core mechanic: sequential moves, weighted random choice, blocked mobs skip. | ADR-0006, `design/scent-mobs.md` |
| 5 | ECS | **Delegated to Claude,** to be decided once the full system picture is designed | Pending (Sim Core design) |
| 6 | Cell size | **One cell size everywhere** | ADR-0003 |
| 7 | License | **Open source, MIT.** Careful, verified dependency licensing. | ADR-0004, ADR-0008, `LICENSING.md` |
| 8 | Repo | **Private GitHub repo** on the owner's account | — |
| — | Save format | **Approved as proposed** | ADR-0002 |
| — | Process | **Framework first, system by system,** with enterprise-style internal APIs and early decisions that reduce later change impact | `SYSTEMS.md` |

Still open: the owner's phase approach, which is now the gameplay-model design walkthrough.

## 7. Risks I'm tracking

- **Asahi driver maturity.** A driver bug could block a GPU kernel. Mitigation: the CPU
  reference implementation of every kernel is always shippable.
- **8 GB of shared memory** on the dev machine. Mitigation: streaming, and memory budgets in
  Tracy from Phase 1.
- **Scope.** "DF-scale" has no end state. Mitigation: phase exit criteria are *measurable*
  (frame times, agent counts, memory), not feature lists.
- **Determinism across compilers** (floating-point differences, `std` library differences).
  Mitigation: integer simulation math, our own RNG and distributions, and cross-platform replay
  tests in CI from Phase 4.
