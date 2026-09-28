# Scent-driven blind mobs — initial analysis (for review)

Status: **the mechanic is accepted (it's the core design). The details below are proposals.** This
is an input to the formal system design for the Fields, Perception and Movement systems. It is not
that design.

## The mechanic as stated

- Every tile holds a scent value. There may be several scent **types** (channels).
- Mob-type enemies are **completely blind**. They sense only the scent on neighbouring tiles.
- Each mob maps the neighbouring scent values onto a number scale, possibly through a
  per-enemy-type table, then draws a random number and moves to the tile it lands on.
- Each mob moves at most once per step.
- The next mob to process is one that hasn't moved **and** has a free neighbouring space. A mob
  with no free space doesn't move.
- The packed interior of a large group therefore does no work.

## Verdict

**Sound, and cheaper than pathfinding by a wide margin.** Blind scent-following replaces per-agent
pathfinding for the largest population in the game. The expensive shared work moves into the
**scent field**, which is exactly the kind of grid stencil the GPU is good at.

## Measured

Spike: `research/spikes/scent-movement/scentbench.cpp`.

- Setup: Apple M1, one core, `-O2`, 1024×1024 map with 8% rubble, mobs packed into 16 dense hordes,
  a stand-in scent gradient, 50 steps per run.
- Each mob does a weighted random pick over its free neighbours plus "stay", using integer weights
  from a lookup table. Occupancy-aware.

| Mobs | A: all mobs, shuffled order | C: frontier only, scent order + wake-ups | D: persistent incremental frontier | E: all mobs, spatial order |
|---|---|---|---|---|
| 10,000 | 0.55–0.76 ms | 0.70 ms | 1.14 ms | 0.80 ms |
| 100,000 | 3.2 ms | 3.3 ms | 4.2 ms | **2.6 ms** |
| 300,000 | 8.7 ms | 6.6 ms | 6.0 ms | **5.6 ms** |

In dense hordes only 5–30% of mobs actually move each step. The frontier variants step only 18–37%
of mobs, yet they barely win.

### What that means

1. **The per-mob step is tiny (~25 ns),** so the step is **memory-bound**. The check "do I have a
   free neighbour?" reads the same 8 cells as the move itself. Skipping the interior therefore
   saves little until ~300k mobs.
2. **Memory locality matters more than skipping.** Processing mobs in spatial order (E) beats
   shuffled order (A) by ~1.6× at 300k, even though E scans all 1M cells to build its order.
3. **So we choose the processing order for *behaviour*, not speed.** Keeping the order a pluggable
   policy costs nothing.

## Recommendations

1. **Default order: highest scent first, with a wake-up cascade (variant C).**
   - The front of the horde moves first. When a mob vacates a cell, its unmoved neighbours are
     woken and processed next, so the rank behind fills the gap **in the same step**.
   - Hordes then *flow* like a fluid instead of "boiling" (random order) or showing sweep-direction
     artifacts (fixed order).
   - This is your "next enemy that hasn't moved and has a free space" rule, made precise and
     deterministic.
2. **Weights come from a per-mob-archetype lookup table.**
   - The table is indexed by *(scent channel, quantized scent difference vs. the current tile)*
     and returns integer weights.
   - "Stay" is an explicit candidate with its own weight.
   - The mob makes one RNG draw from its own seeded stream. Integer maths keeps it deterministic
     across platforms, and the table is data (TOML), not code.
3. **Make the heavier cost, the scent field itself, a first-class system (Fields).** It covers:
   - emission (player, blood, food, noise-as-scent?)
   - diffusion and decay per world step
   - blocking by walls and closed doors
   - several channels, e.g. 4 × u16 over 1024² = 8 MB

   This is the first GPU compute kernel, with a CPU reference implementation.
4. **Mobs outside the detailed simulation radius stay aggregates.** A distant horde is one object
   (count, composition, position) that drifts along a coarse regional scent/interest gradient. It
   breaks into individual mobs when it enters the detailed radius, and merges back when it leaves.
   This keeps "the whole world might not move every step" consistent.
5. **Parallelism, only if ever needed.** Mobs more than 2 cells apart can't interact within one
   step. Independent spatial clusters can therefore run on different cores with identical results.
   At the measured costs, single-threaded is fine for a long time.

## Proposed system boundary

It should feed the formal design.

```
Fields system        owns scent channels, emission, diffusion kernels (CPU/GPU), queries
Perception system    turns the fields into what an archetype can sense (blind mob = 8-neighbour scent)
MobMovement system   takes: FieldView, OccupancyView, ArchetypeTable, RngStream
                     returns: MoveBatch (deterministic list of moves) + events
                     policies: OrderingPolicy (scent-desc+cascade | spatial | shuffled)
                               WeightPolicy  (table-driven)
```

## Questions for the design session

- **Scent physics.** Diffuse (spreads out) or trail (deposited, decays in place, no spread)?
  Wind? Water breaking a trail? Does scent pass under doors or through thin walls?
- **Scent channels.** Which ones? For example: player, blood/wounds, food/corpses, the mobs' own
  "herd" scent (for cohesion or spreading out), lures or bait the player can use.
- **Mob behaviour at edges.**
  - With no scent at all: wander, stay, or follow the herd scent?
  - Adjacent to the player: an attack replaces the move?
  - Can mobs push, swap or trample each other?
- **Masking.** Can the player mask their scent (rain, water, strong smells)? That is a big
  gameplay lever and affects the field design.
- **Other enemies.** Are there non-mob enemy types that *do* see? They would use FOV and the
  pathfinding tiers in `GAMEPLAN.md` §3.4.
