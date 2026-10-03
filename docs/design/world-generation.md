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
| **L0 Macro** | A 100 km geography square, generated whole (ADR-0018) | Elevation, ranges, valleys, forests, lakes, coast and sea, river courses; landmarks named in game by direction | Stored as its seed; regenerable |
| **L1 Region** | e.g. 1024×1024 tiles | Biomes, rivers (lazy long features, ADR-0010), lakes, road network, **settlement sites and their identities** (size, culture, faction, era), points of interest | Small; cached widely |
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
- **Mountain ranges** come from the continuous L0 noise, so they are consistent everywhere.
- **Rivers:** ~~drainage simulation~~ replaced by a simple lazy long-feature generator (see
  "Rivers (N006)" below, and ADR-0010).

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
- **Rivers are no longer a risk** (N006, ADR-0010). They are lazy long features, with no drainage
  computation.

## Rivers (N006)

- **When:** created by the river generator as L1/L2 detail is generated, not in top-level
  geography.
- **Shape:** a source and a mouth far beyond the area being generated. The source is the end at
  higher macro elevation, and a mouth may be a lake. They are joined by a smooth, meandering path
  that is refined per area.
- **Spacing:** whether an area spawns a river is decided by a seeded roll that must beat
  neighbouring candidates within a spacing radius. Candidate paths near an existing river are
  suppressed. Spacing and length ranges are data, tuned for a realistic density.
- **Consistency:** every area checks the candidates within the maximum river length, so the same
  seed produces the same rivers **in any exploration order and at every stage**.
- **Terrain follows the river:** at tile detail the river carves its bed and banks. A river
  reaching another river joins it as a tributary.
- **Courses come from the geography map (ADR-0018):** the river generator refines a course the
  geography square already placed. Most towns have no river through or beside them.

## Aftermath model: the three stages ([N005](notes/N005-the-event-and-aftermath-stages.md), [ADR-0009](../adr/0009-baseline-plus-aftermath-generation.md))

Every level L0–L5 generates **baseline → event → aftermath(`elapsed_days`)**. The stages are
presets: **1 week = 7, 1 month = 30, 1 year = 365**. Aftermath effects are monotonic in time
(threshold sampling), so the three stages of a seed are consistent snapshots of one world.

| Aftermath effect | Driven by | 1 week | 1 month | 1 year |
|---|---|---|---|---|
| Dead leaving buildings | Openness of the building (doors forced during struggles, open entryways) | Many out wherever a struggle opened the building | More wander out | Mostly outside, except in sealed buildings |
| Dead attrition | Global curve (killed by survivors, other causes) | Near full population | Lower | Much lower |
| Looting by other survivors | Priority by building type (pharmacy, grocery, gun shop first) × elapsed time × local survivor density | Priority targets hit near dense areas | Most priority targets, many homes | Most accessible places searched |
| Spoilage | Shelf life per item, power loss | Fresh food turning, frozen food thawed | Most perishables gone | Only long-life goods usable |
| Damage and decay | Break curves per fitting (windows, doors), weather | Mostly intact, except struggle sites | Broken windows common | Widespread damage, decay |
| Traces | Event struggles, aging | Fresh remains, blood | Aged | Old or gone |

**Where Dead are at the event** follows real occupancy: a building's occupants at the moment
of turning. Buildings no struggle ever opened **stay sealed with their occupants inside**, at
every stage.

The table's three columns are **samples of one continuous clock**, not fixed modes (ADR-0011).
All six N005 follow-up questions were answered in N007; see below.

## Event parameters, clock, utilities, fires ([N007](notes/N007-event-parameters-clock-utilities-fires.md), [ADR-0011](../adr/0011-global-event-clock-and-world-event.md))

### Per-world event

- **Rolled per world seed:** the moment of the event (time of day, day of week) and the
  **turned fraction (80–99%)**. Both are shown on the opening splash screen.
- **Moment of the event:** decides where occupants were (home, work, school, commuting). That
  sets baseline occupancy, and therefore where Dead start.
- **Turned fraction** scales everything survivors left behind:

  | Effect of more survivors (lower turned %) | Why |
  |---|---|
  | More **dead bodies** | Survivors died; the Dead didn't |
  | More **struggle damage** and forced entries | More fights at the event |
  | More **looting and scarcity** over time | More people going through supplies |
  | More **barricades** | Survivors fortified before dying or leaving |
  | More **notes** and **encampments** | Survivors lived long enough to write and build |

- **Encampments hold concentrated useful supplies, but are deserted for a reason.** The reason
  is generated and visible in the evidence:
  - usually **overrun**: dead presence, breached barricades, remains
  - rarely **owner died elsewhere**: intact, untouched, "ripe for the taking"

### The global clock as a gradient

- **Every aftermath curve is a continuous, monotonic function of days-since-event** and
  saturates at long times. It must stay sensible at 5 years.
- **Overgrowth:** realistic growth by climate and biome. Cracks → weeds → brush → saplings.
  Changes terrain footing (streets slowly turn into underbrush) and blocks sight.
- **Environmental damage:** weathering, water damage through broken windows and roofs, rust,
  collapse risk rising with time.
- **Areas generate at the current clock.** Revisited stored areas catch up from the clock they
  were last updated at (ADR-0011 item 4, refined by ADR-0019; see "Revisits" below).

### Utilities

| Utility | Unit | Chance of working | Notes |
|---|---|---|---|
| **Power** | A **neighbourhood** (city generation's district/neighbourhood unit) | May or may not work at week 1; **falls with time**; **lower still as density drops** | A gradient, so there's no sudden city/wilderness break. Affects lights, fridges and freezers (spoilage), electric doors, pumps. |
| **Water** | Neighbourhood (mains) | **Much higher** than power, still falling slowly with time | |
| **Water, very sparse properties** | Single property | **High regardless of time** (wells) | Rural and isolated homesteads |

### Fires

- **World-generation events:** a single building or a cluster (spreading along adjacent
  buildings). Their probability and extent are data.
- **Intensity scales with the burned area.** Light damage means scorching and smoke damage.
  Severe damage means **missing walls and floors**.
- **Structural support rule (shared with digging).** A floor or roof cell needs support from below
  within a span limit. When support is lost, through fire or excavation, the unsupported part
  **collapses**. Its contents fall to the next supported level, using the falling calculation
  (P-WO-06).
- **Charred items:** each item's condition is rolled against the burn intensity and the item's
  material. Results: *working*, *barely working* or *broken* (G03 D3.5).
- **Smoke scent:** burned areas emit a **smoke channel** that decays with time since the fire. By
  the dominance matrix it **masks and modifies** other scents nearby (scent-mobs.md).

### Dead over time

- Damage output stays roughly constant.
- **Tissue fragility per body part rises with the clock** (in monthly steps), so limbs and
  appendages become easier to damage and sever (walkthrough G05).
- Combined with falling supplies, this gives the difficulty ↔ reward gradient: **early is harder
  and richer; late is easier and poorer.**

## Geography first ([N023](notes/N023-documentation-audit.md), [ADR-0018](../adr/0018-geography-first-water-and-fitted-towns.md))

- **A 100 km geography square is generated whole** before anything inside it, at about 100 m
  cells: elevation, ranges, valleys, forests, lakes, coast and sea, river courses. It replaces
  "L0 evaluated anywhere" as the parent of L1. The world is endless: neighbouring squares
  generate whole as the player nears an edge (D-041 A).
- **Water bodies:** lakes and sea are polygons; towns may border them, and shores get their own
  buildings, rooms and items (docks, boat sheds, bait shops, lakeside houses, breakwaters).
- **Towns are shaped by what they sit on:** a river through a town gets bridges where streets
  cross it, riverside streets and river trades; a shore bends the grid along it. Site scoring
  (above) still picks the landmark.
- **Far geography is words only:** landmarks named by direction and distance.

## Revisits ([N023](notes/N023-documentation-audit.md), [ADR-0019](../adr/0019-revisits-re-age-player-made-things.md))

- An area out of the active range is brought forward to the current clock **only when it is
  loaded again**, as a streaming job split across frames. There is no world-wide daily pass, so
  cost never grows with the number of places visited or the length of a run.
- Catch-up is closed form: per-object thresholds against the curves, the same work for a day
  away or ten years.
- **The horde thins** by each unit's attrition threshold. **Player-made and player-changed
  things** age on gentler curves from when they were last touched: barricades weaken and fail,
  seals get broken into, stashes get taken. Terrain, buildings and containers are not
  regenerated; unopened containers already resolve at the clock they are opened (D-023).
- **No gate** (D-040 A): any reload catches up, so a day away changes a little and a month a
  lot. N009's 30-day gate is retired.

## No animals, and the crows ([N013](notes/N013-bikes-guns-bites-locks-animals-words.md))

- **Only humans remain.** Every animal is inexplicably dead: no birds, squirrels, pets or
  livestock. This is deliberate and part of the story's strangeness. No animal simulation exists.
- **Pet food, cages, leashes and feed sacks still exist**; they are leftovers, which adds to the
  eeriness.
- **Ambience:** distant **crow calls** play in the background audio, but no crow is ever found or
  locatable.
