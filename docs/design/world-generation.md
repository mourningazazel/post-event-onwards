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

## Questions for the design session

1. **Truly endless in every direction,** or endless along one axis? Any hard world features (oceans,
   poles)?
2. **Travel speed.** Is there fast travel, vehicles or a world map? Top travel speed sets the
   required generation throughput.
3. **Terrain modification.** Can the player alter terrain (dig, build, burn, flood)? That decides
   whether L4 needs full mutable persistence everywhere or only in buildings.
4. **Item depth.** How far does nesting go (a drawer containing a box containing letters with
   text)? That sets the Items/Containment design and L5 storage size.
5. **Z-levels.** How many floors, basements and underground layers? Is there a global depth
   dimension (caves, sewers, mines)?
6. **Change over time.** Does the world change while the player is away (decay, mobs moving
   between cities, factions)? If so, which layer simulates it, at what fidelity?
