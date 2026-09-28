# RogueBasin research notes

Source: every link on <https://roguebasin.com/index.php/Articles> (~200 unique articles in 26
sections), followed 1–2 levels deeper where the linked page was technical. 244 pages were read in
total. Page titles are cited in *italics*; each is at `https://roguebasin.com/index.php/<Title>`.

> **Attribution and copyright.** These are our own summaries, with short attributed quotes.
> RogueBasin's copyright status is unclear. [RogueBasin:Copyrights] asks that large blocks of
> text not be copied elsewhere, so **raw page text is not stored in this repo**. The maintainer
> keeps a local cache outside the repo.
>
> [RogueBasin:Copyrights]: https://roguebasin.com/index.php/RogueBasin:Copyrights

Date: 2026-09-27. These notes are condensed for a performance-heavy C++/SDL3 grid roguelike with
mass pathfinding, huge generated terrain and cities, and GPU offload.

> **Coverage gaps on RogueBasin:** it has **no** article on city generation and **nothing** on
> mass pathfinding (flow fields at scale, HPA\*, JPS). Those parts of the gameplan draw on outside
> sources. They are marked *(external)* in `../GAMEPLAN.md`.

---

## 1. AI and pathfinding

**Dijkstra maps (flow fields) are the key technique.** Sources: *The Incredible Power of Dijkstra
Maps* (Brian Walker, Brogue), *Dijkstra Maps Visualized*, *A Simple Algorithm for Generating a
Dijkstra Map*, *Quick Pathfinding in a Dungeon*.

- How it works: set goal cells to 0 and every other cell to ∞, then relax each cell to
  1 + min(neighbours) until nothing changes. Agents then just step "downhill".
  - *"a Dijkstra map needs to be updated only when the underlying goal points change... adding
    more monsters to the map is cheap"*
  - *"You can have as many fleeing monsters as you like, and they can all share this same map."*
- **Desire-driven AI:** keep one map per goal category (player, food, treasure, noise,
  unexplored, stealth, herd leader). Each monster carries a weight per category, positive to
  attract and negative to flee. It moves to the neighbouring cell with the best weighted sum, so
  per-monster state is a handful of floats.
- **Flee maps:** multiply the approach map by roughly −1.2 to −1.6 and re-relax. Too large a
  factor collapses all fleeing monsters onto one far cell.
- **Other uses of the same machinery:**
  - Autoexplore: the goals are the unexplored cells.
  - Ranged standoff: the goal is the ring of cells at the desired range, optionally filtered by
    line of sight.
  - Lava/levitation budget check: compare turns-to-safety against the turns of effect remaining.
  - Cursor path preview.
- The article's algorithm (repeat full scans until stable) is the *naive* version. On CPU, a
  bucket-queue (Dial) BFS is O(cells). On GPU, ping-pong relaxation is a textbook stencil pass.
- **Scaling issue:** re-relaxing the whole grid every turn doesn't scale to DF-sized maps. Update
  only dirty regions, and bound each field to a region.

**Anti-jam trick** (*Simple way to prevent jams of monsters with A\**). Each monster keeps an
"idle time" counter. A cell occupied by a monster costs extra in proportion to that monster's
idle time, which produces traffic flow around blockages for free. Fold it into the cost grid as
one more layer.

**Per-agent A\*** (*Pathfinding*, *Python pathfinder*). Every source treats one A\* per agent per
turn as the anti-pattern. Mitigations, in rough order of payoff:

1. Cache paths and validate only the next step (the biggest win).
2. Disabling diagonal movement measured >40% faster.
3. Accepting non-optimal paths measured <10% faster.
4. Limit the search radius.
5. Search bidirectionally.
6. Move searches to worker threads.

Even the A\* article ends up recommending precomputed Dijkstra maps.

**Other techniques from the AI section:**

| Technique | Source | Scaling note |
|---|---|---|
| Dense scent diffusion (blur + decay per turn) | *Tracking by Scent and Sound* | *"10 iterations/turn on 80×50 is noticeably slow"* (Pentium 133). Ideal GPU stencil. |
| Sparse scent objects `{x, y, intensity, radius}` that age each turn | *An Alternative Scent Implementation* | CPU with a spatial hash. Poor GPU fit. |
| Sound and herding floods from each creature | *Denizen Herding Behavior* | Self-described as "processor hungry". Recast as per-species density grid: splat, blur, sample. |
| Shared search heatmap: spreads from last-known position, cleared by any guard's FOV | *Smart searching / heatmap* | Same wavefront shape, so GPU-friendly |
| Ratio AI (weighted random choice between behaviours) | *Ratio AI* | O(1). Good as the decision layer on top of the fields. |
| Plug-in AI strategy objects, shareable by a pack | *Plug-In Monster AI*, *Generic way* | Keep the idea but make it data-oriented: one system per behaviour archetype, not per-object virtual calls |
| Fear/love/hate vector sum over entities in FOV | *A Better Monster AI*, *Need driven AI* | Cost bounded by FOV size, but no memory once the target is out of sight and no obstacle routing |
| Scripted time-of-day movement for townsfolk | *Implementing interesting townsfolk AI* | Effectively free. This is AI level-of-detail for background city crowds. |
| Wall-following "anticipating bug" pathfinder | *Anticipating wall-following pathfinder* | Uses a few integers of memory and costs the same whatever the map size. Fallback for agents outside maintained fields. Cap iterations at 4× sight range. |

- *"The closer the target is, the more often the path should be recalculated."*
- Javelin's changelog: *"AI cache disabled by default (can slow down some systems)"*. Caches need
  tuning per machine.

## 2. Field of view, line of sight, time and combat

**FOV comparison.** The key source is *Comparative study of FOV algorithms for 2D grid based
worlds*, a quantitative benchmark by libtcod's author.

| Algorithm | Symmetry error (indoor / outdoor) | Speed | Notes |
|---|---|---|---|
| Recursive shadowcasting | 0.93% / 6.9% | Fastest overall, especially indoors | Artifacts at room corners and thick shadows behind diagonal walls |
| Basic raycasting + cleanup pass | 0.75% / 4.4% | Fastest outdoors | Surprisingly competitive |
| Diamond raycasting | 0.82% / 6.5% | Noticeably slower | Blocks sight through diagonal walls |
| Permissive (levels 0–8) | 2.1%/11% down to **0/0** at level 8 | About the same as Basic | Level 8 = Precise Permissive FOV: symmetric |
| Digital | 0 / 0 | **10–100× slower** | Avoid at scale |

- **Restrictive Precise Angle Shadowcasting** fixes the pillar-shadow artifacts.
- **Milazzo's algorithm** has a symmetric/non-symmetric toggle in the same code.
- **Spiral Path FOV** uses a bounded queue and natively supports cones and arcs (flashlights,
  breath weapons).
- **FastLOS** precomputes a bitmask per cell, so "can A see B" becomes one bitwise AND. It suits
  static geometry and is the most SIMD/GPU-friendly primitive found.
- ***An Efficient Observation Algorithm*** keeps a sight matrix and shifts it when the actor
  moves, recomputing only the edge. Refresh cost drops from O(area) to O(perimeter).
- **Lighting:** run the same FOV machinery from each light source and merge the results into a
  light grid. Each light is independent, which makes this a natural GPU/parallel task.

**Time systems.** *Time Systems*, *An elegant time-management system*, *Priority queue based
scheduling*, *Simple turn scheduling (Python)*.

- A naive energy system touches every actor every tick. The priority-queue reference
  implementation does O(n) inserts plus an O(n) "adjust everyone" step. **Neither scales to
  thousands of actors.**
- **Absolute-time buckets** (timing wheel, `schedule[tick] = [actors]`) touch only the actors
  that act. They scale well.
- **Hakl's version** uses a circular list with a travelling sentinel: O(1) amortized, no
  allocation. It also puts buffs, damage-over-time effects and regeneration in the same scheduler
  as actor turns.
- **Split preparation from effect** for wind-up actions.
- Batch actors by speed tier, and give off-screen actors cheaper resolution.

**Combat and magic.**

- *Programming Roguelike Magic* evolves from a switch per spell to a pipeline:
  target selector → area → effect.
- *Representing Magic Skills* describes spells as data: an `EffectType` enum plus generic integer
  params, with **zero new code per spell**.
- *Monster attacks* supplies a ready taxonomy of attack types and status effects.
- *Interesting Critical Hits*: crits trigger arbitrary effects (stun, disarm, knockback, extra
  action) rather than just multiplying damage.
- *Implicit Facing*: derive facing from the last move. Front, flank and rear come from modulo-8
  arithmetic.
- *Simple Combat*: pre-aggregate attack rating and defence rating, then roll `AR + d100 > DR` and
  apply XdY damage.
- *Two-Key Targeting*: a targeting UI for dense crowds.
- **Takeaway:** one data-driven Effect system shared by melee, ranged attacks, crits, monster
  specials and spells.

## 3. Maps, terrain, world and city generation

**Map data structures** (*Data structures for the map*):

- A dense per-tile terrain array.
- A dense per-tile **flag bitfield**: blocks movement, blocks light, has item or monster, seen,
  mapped.
- **Sparse lists** of entities, each storing its own position.

Hot queries (FOV, movement, pathing) touch only the dense arrays. That is the structure-of-arrays
layout, and it maps directly onto GPU buffers.

**Unbounded and streamed maps.** *Dynamically Sized Maze/Cave* stores cells in a hash map keyed by
(x, y) with a running bounding box. Generalized to whole chunks, that gives a streamed world.

**Persistence** (*Dungeon persistence*). There are four models: non-persistent, persistent, mostly
persistent (terrain persists, monsters slowly repopulate) and one-way. The key technique is:

`chunk_seed = hash(world_seed, cx, cy, z)`

Terrain regenerates from the seed, so only deltas (items, creatures, edits) are saved.
*Compression* suggests RLE for terrain runs and bit-packed flags.

**Z-levels** (*Third dimension in an ASCII roguelike*). Add a z coordinate from day one. A cheap
3D distance is `max(D,H) + min(D,H)/2`. FOV becomes volumetric, and displaying multiple levels is
the hard UI problem.

**Generators:**

| Algorithm | Produces | GPU/parallel fit |
|---|---|---|
| Cellular automata: 40–45% fill; rule "≥5 of 8 neighbours" or two-radius R1≥5 ‖ R2≤2; 3–5 passes | Caves (Brogue uses it) | **Excellent.** Double-buffered stencil. Fix connectivity with a flood fill or union-find. |
| Double-layer CA (XOR-fuse two CA maps) | Connected caves without a flood-fill pass | Excellent |
| Diffusion-limited aggregation | Connected, vein-like tunnels | Poor: sequential random walkers |
| Delving (growing-tree with neighbour limits and loop chance) | Anything from maze-like to open caverns; always connected | Sequential but O(1) per cell |
| Drunkard's walk | Connected caves | Sequential; many walkers can run in parallel |
| BSP split | Rooms and corridors (building interiors) | Leaves are independent |
| Grid / Circle based | Room graphs; the circle version guarantees a loop | Circle topology ≈ ring road plus side streets |
| **Abstract Dungeons** | Topology graph of areas, then pluggable per-area "painters" | **Architecture to copy for cities.** Painters are independent jobs. |
| Template theming (tokenize NxN tiles, match templates with `?` wildcards, stamp replacements, rotate ×4) | Features, building stamps; lightweight wave function collapse | Per-cell matching is a stencil, so GPU-able |
| Random Zone Generation: 3 noise channels → snap to palette → flood-fill regions → merge small ones | Biomes and districts. Claims ~1000×1000 tiles/s. | Noise and snapping are per-pixel |
| Fractals: midpoint displacement / diamond-square, fBm | Heightmaps | fBm samples are fully independent: ideal for GPU |
| Winding ways (Bresenham → subsample waypoints → perturb within angle limits) + Catmull-Rom smoothing | Rivers and roads | Independent routes run in parallel; junctions resolved serially |
| Directional dungeon (row walk with a variable-width band) | Rivers, canyons, direction-biased caves | Cheap |

**City generation.** RogueBasin has nothing specific; *City of the Damned* and *City of the
Condemned* are game stubs only. The reusable pieces are:

- Abstract Dungeons' area/painter split.
- Random Zone Generation for districts.
- Grid/Circle topology for road skeletons.
- Template stamping for buildings.

Road-network growth and lot subdivision come from external literature.

## 4. Architecture and engine

- **ECS is recommended:** *"probably complex enough that you should use one"*. Entities are
  integer IDs and components are data.
  - Use the **same ID scheme for save-file references**, and never serialize pointers. This
    avoids the dangling-reference bug class that *Code design basics* warns about.
- **Rule/event systems:** rules decide how the world evolves over time, whereas ECS decides what
  things are made of. Explicit event objects (queued, logged, interceptable) beat generic
  forward-chaining rule engines on performance and debuggability.
- **Info files** (Angband precedent) follow three rules: human-editable, parseable in one fast
  pass, and no logic inside.
  - *Lua makes info files obsolete*: content can be scripts returning tables.
  - T-Engine, DoomRL and Qud all use a data + script split.
- **Save files** (the richest page):
  - Save the RNG seed and state rather than generated output.
  - Stable object IDs.
  - Version-stable content IDs with remap tables across patches.
  - Validate every loaded value and fail gracefully.
  - Warn before a lossy conversion.
  - *Our deviation:* the wiki prefers delimited text. We'll use versioned, checksummed binary
    chunks (zstd); corruption detection comes from the checksums.
- **RNG:**
  - Never use libc `rand()`, which differs across platforms.
  - Keep separate streams for world generation and runtime rolls.
  - The wiki suggests Mersenne Twister. *Our deviation:* PCG/xoshiro plus counter-based hashes,
    which are smaller, faster and seekable.
- **Output libraries:**
  - curses: too limited (16 colours, no tilesets).
  - libtcod (now on SDL3, 2.2.2 as of Jan 2026): true-colour console, custom font mapping, and
    FOV/path/noise/BSP algorithms.
  - BearLibTerminal: bitmap and TrueType fonts, tile composition; effectively unmaintained.
  - Consensus for our needs: a thin custom grid renderer modeled on their APIs.
- ***Things which are hard to code*:**
  - invisibility, polymorph, charm
  - item stacks and inventories, throwing
  - pets and allies
  - persistent levels with monsters crossing between them
  - monster FOV
  - quests and overworld generation
  - liquids and fire
  - running/auto-move interruption
  - stealth
  - economy
  - balance and content volume (called harder than any algorithm)
- ***Code design basics*** lists four things to design properly up front:
  1. The turn and scheduling model, including absolute vs relative time.
  2. Map representation, together with save, area effects, pathfinding and FOV.
  3. **One** item-lifecycle API.
  4. One entity-reference scheme.
- **UI conventions:**
  - Show contextual info only; details go on separate screens.
  - Generic verbs plus a filterable inventory.
  - Support vi-keys **and** numpad, with rebinding.
  - High contrast for threats and loot; muted colours for floor and junk.
  - Don't rely on red/green alone (colour blindness).
  - *Icons in Roguelikes*: every glyph and tile must be recognizable, distinct and consistent.
  - The Dart render-tree article: compose ASCII UI panels as a widget tree.
- **Portability:**
  - Only 7-bit ASCII is safe, so index glyphs by Unicode or tile ID.
  - Never rely on filename case.
  - Use forward slashes internally.
  - Keep install data, user saves and config separate (`SDL_GetBasePath` / `SDL_GetPrefPath`).

## 5. Design

- **Berlin Interpretation.** High-value factors:
  - random generation
  - permadeath
  - turn-based
  - grid-based
  - non-modal
  - complexity
  - resource management
  - hack'n'slash
  - exploration

  Treat it as a set of tensions to decide on explicitly, not a checklist.
  - *Berlin Reloaded* reframes it as freedom, immersion, a fresh experience every run, and
    decision density (no busywork; auto-explore).
- **Power curve:** fights must stay risky. Hordes must not turn into XP piñatas; keep elites,
  terrain and sheer numbers as threats.
- **Hunger and other timers** need **one** purpose each: resource, clock, or gate on actions.
  Micromanagement with no decisions is the worst case.
- **Permadeath alternatives:**
  - tiered difficulty
  - resurrection that costs something
  - temple checkpoints
  - an afterlife mini-game
  - **heir/succession**, which fits a DF-style world

  *"You cannot design around save-scummers."* Save policy and the difficulty curve are one design
  problem.
- **Quests:** archetypes are adventurer, hero (lead a force), warrior, merchant, race, protect.
  Deliver them organically rather than through a menu, and interlock them.
- **Story:** goal → twist → pitfalls and rewards → roaming content → climax twist → setting twist.
- **Mood:** aim for "Fear" (pacing, withheld information), not gore and not "True Horror".
  Weather contrast is cheap and effective.
- **Horde and city idea pages:**
  - *OrcRL*: an escalation ladder of settlements.
  - *GalaxyRL*: squad and officer delegation, and being one soldier inside a big battle.
  - *World of Rogue*: fixed damage to a random body part, which resolves fast in mass combat.
  - *Fire Brigade RL*: the horde as an environmental hazard.
  - *Magical Dungeon*: stairs as biome transitions.
- **Project management:**
  - A feedback drought is normal.
  - Use external deadlines (7DRL-style milestones).
  - Only go open source if you want contributors and can review their patches.
  - *"Design what players want to play, not what you want to code."*
