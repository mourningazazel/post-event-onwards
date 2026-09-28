# Endless, staged, persistent world — initial analysis (for review)

Status: **direction accepted (your decision). The structure below is a proposal** for the formal
design of the Generation, Streaming/LOD and Persistence systems.

## The requirement as stated

- **An endless world, generated only regionally.**
- **Staged generation.** About a city's worth of buildings exists around the player at a time,
  starting as *identities only*. Detail is added as the player gets closer.
- **Ranges are tied to budgets.** Generation ranges follow computation and memory thresholds so
  the game never bogs down.
- **Layers of context.** The city layer sits inside a larger regional layer: mountains, rivers,
  and neighbouring cities that exist even before they are generated.
- **Depth and persistence.** Detail goes all the way down to items on each square, and details
  of those items, on every level (z), and all of it is **persistent**.

## Proposed model: hierarchical generation levels

| Level | Scale (example) | Contents | Lifetime |
|---|---|---|---|
| **L0 Macro** | Evaluated anywhere, on demand | Continuous fields from world-coordinate noise (elevation, moisture, temperature), mountain ranges, drainage basins | Never stored; always regenerable |
| **L1 Region** | e.g. 1024×1024 tiles | Biomes, rivers, lakes, road network, **settlement sites and their identities** (size, culture, faction, era), points of interest | Small; cached widely |
| **L2 Settlement** | One city | Outline, districts, street graph, blocks, lots, **building identities** (type, footprint, floors, owner, condition) | Medium; kept for the cities near the player |
| **L3 Structure** | One building or site | Floor plans, rooms, doors, stairs, basements, z-levels | Materialized near the player |
| **L4 Tile** | Chunk (32×32 × z) | Terrain, material, furniture, fixtures | Materialized near the player |
| **L5 Contents** | Per tile or container | Items, item details (condition, contents, inscriptions), inhabitants | Materialized when first needed (entering range, opening a container) |

### Principle 1: each level is a pure function of its parent

`generate(level, coords, seed, parent_data, generator_version) → data`

- Children never modify parents. Constraints flow downward only.
- Any piece can therefore be generated independently, in any order, on any thread, with identical
  results. That is what makes streaming, parallel jobs and determinism possible together.

### Principle 2: knowing about neighbours without generating them

Anything that crosses a boundary must be decided at a level whose cell covers both sides.

- **Settlement sites.** Place them by a hashed jittered grid or Poisson process *at the L1 cell
  level*. Any region can list the cities in the neighbouring regions by evaluating only those
  cells' site function (microseconds). No neighbour detail is generated.
- **Roads between cities.** Plan them on the settlement-site graph, which is cheap, before any
  city is detailed.
- **Rivers and mountain ranges are the hard case in an endless world.**
  - Drainage is inherently non-local, because a river's path depends on terrain far upstream.
  - Standard mitigation: compute coarse drainage on a *very* coarse grid with overlapping margins,
    so the result for a cell doesn't depend on where evaluation started, then refine locally.
  - **This is the biggest technical risk in world generation.** It deserves its own spike before
    the design is locked.

### Principle 3: persistence means overrides on a generated baseline

Two options:

- **(a) Store deltas only.** Never store what can be regenerated; store only changes.
  - Minimal disk use.
  - But any change to generator code silently changes existing worlds under the stored deltas,
    corrupting saves.
- **(b) Freeze on materialization (recommended).**
  - L0–L2 stay regenerable, protected by versioned generators.
  - L3–L5 are written to the world database the first time they materialize near the player, then
    loaded from there forever.
  - Unvisited areas may pick up improved generators after a game update. Visited areas never
    change.
  - Disk grows with *explored* area only, compressed with zstd.

Either way, every stored record carries the **generator version** that produced it, and items and
entities carry **stable IDs** (see ADR-0002).

### Principle 4: budget-driven detail rings

- The "radius" per level is **not a constant**. A **Budget Manager** holds:
  - memory ceilings per level
  - time slices per frame and per step for generation jobs
- A **Streaming/LOD Manager** requests promotions (more detail) and demotions (unload or
  aggregate). Requests are prioritized by distance, direction of travel and the player's likely
  next moves.
- Rings expand when budget is spare and contract under pressure. Hysteresis stops thrashing at
  ring edges.
- Generation jobs run on worker threads and never block the player's step. If the player outruns
  generation, the fallback is a loading indicator, not a hitch.
- The same rings define **simulation detail** (see `scent-mobs.md` §4): a region simulates at the
  fidelity of its generated detail.

### Coordinates

- The world is endless, so world tile coordinates are **64-bit integers**.
- Rendering and simulation use positions relative to a moving local origin, which avoids precision
  problems far from spawn.

## Owner answers (2026-09-27) and the design they lead to

| Topic | Owner's answer | Design consequence (proposal) |
|---|---|---|
| Extent and scale | Endless, with **real-world size and distances**. Long empty stretches; lakes and rivers are large on screen. | The level scales above need re-basing. If 1 tile ≈ 1 m (to be decided in the walkthrough): L0 macro cells are tens of km, L1 regions ~8–16 km, a city 2–30 km across. At walking pace a step is ≈ 1 m, so 20 km between towns is ≈ 20,000 steps. |
| Settlement siting | The player starts in cities, and cities **prefer visual landmarks** (a lake, a mountain) | Site selection is a **scoring function over L0/L1 features**: lake shore, river confluence, coast, foothills, crossroads. It's evaluated at hashed candidate points per L1 cell, and the best-scoring point wins. Each city records its landmark, which makes the landmark part of the city's identity. |
| Travel | **Walking only**; cars exist but don't run (setting) | Good news for streaming: the player can never outrun generation. Budgets can favour depth (detail) over reach. Cars are world objects: containers, obstacles, landmarks. |
| Terrain modification | **Dig and saw anything a person physically could.** Record **only changed tiles**. | Store the terrain **baseline** as a procedural function plus column data. Store player changes as **sparse per-chunk modification records**, so untouched land costs nothing on disk. Materials carry physical properties (hardness, cuttable, diggable) that feed the actions system. |
| Items | **Unlimited nesting and detail**; storage must not care about depth | A graph / database-style item model. This is the first topic of the gameplay-model walkthrough (`docs/design/gameplay-model/`). |
| Buildings and height | Physical representations of **real building types**; terrain **climbs in height**. Streets can slope across z-levels, but houses are level. | **Column terrain model:** height, strata and materials per column, never stored as full 3D except where needed. Explicit 3D chunks exist only where structure exists (buildings, caves, dug holes). Buildings get a level foundation, with cut and fill against the terrain. Streets follow the terrain with ramps. **Vertical resolution** (z-unit vs storey height) is a gameplay decision for the walkthrough. |
| Change over time | **Mob population migration** between regions, driven by accumulated scent and density. More enemies spawn where crowds accumulate. | Aggregate population and attraction fields per region cell, spawning in and merging out at the detailed-area edge. See `scent-mobs.md`, "Population over time". |

### Memory implications of real-world scale (first estimate)

- **Building identities are small.** About 64 bytes each, so ≈ 100k buildings (a large city) is
  ≈ 6.4 MB. "A city's worth of identities around the player" is cheap.
- **Detailed terrain is the expensive part.** It has to be bounded by the rings and stored as
  columns, not voxels: a 512×512-tile detailed window in column form is a few MB.
  Full-3D chunks exist only inside structures and excavations.
- **The rivers risk is unchanged, and scale makes it more visible.** Large rivers crossing many
  regions need the coarse-drainage-with-margins approach. That spike comes first.

## Aftermath model: the three stages ([N005](notes/N005-the-event-and-aftermath-stages.md), [ADR-0009](../adr/0009-baseline-plus-aftermath-generation.md))

Every level L0–L5 generates **baseline → event → aftermath(`elapsed_days`)**. The stages are
presets: **1 week = 7, 1 month = 30, 1 year = 365**. Aftermath effects are monotonic in time
(threshold sampling), so the three stages of a seed are consistent snapshots of one world.

| Aftermath effect | Driven by | 1 week | 1 month | 1 year |
|---|---|---|---|---|
| Zombies leaving buildings | Openness of the building (doors forced during struggles, open entryways) | Many out wherever a struggle opened the building | More wander out | Mostly outside, except in sealed buildings |
| Zombie attrition | Global curve (killed by survivors, other causes) | Near full population | Lower | Much lower |
| Looting by other survivors | Priority by building type (pharmacy, grocery, gun shop first) × elapsed time × local survivor density | Priority targets hit near dense areas | Most priority targets, many homes | Most accessible places searched |
| Spoilage | Shelf life per item, power loss | Fresh food turning, frozen food thawed | Most perishables gone | Only long-life goods usable |
| Damage and decay | Break curves per fitting (windows, doors), weather | Mostly intact, except struggle sites | Broken windows common | Widespread damage, decay |
| Traces | Event struggles, aging | Fresh remains, blood | Aged | Old or gone |

**Where zombies are at the event** follows real occupancy: a building's occupants at the moment
of turning. Buildings no struggle ever opened **stay sealed with their occupants inside**, at
every stage.

### Open questions (N005 follow-ups)

1. **When did the event happen?** Time of day and day of week decide where people were: homes at
   night, offices and schools by day. Is it fixed in the lore, or rolled per world seed?
2. **Traces of other survivors.** Since NPCs are never met, can the player find signs of them:
   barricades, notes, abandoned camps, bodies of later casualties?
3. **Zombie decay.** Over a year, do zombies physically deteriorate (slower, weaker), or are there
   just fewer of them?
4. **Nature at one year.** Overgrowth in streets, wildlife? This changes terrain footing (streets
   become underbrush) and therefore crowd movement.
5. **Utilities.** Does power and water fail instantly, or over the first days or weeks? This
   affects spoilage (fridges and freezers) and lighting at 1 week.
6. **Fires.** Did the event start fires (stoves, crashes) that burned areas, more visible at later
   stages?
