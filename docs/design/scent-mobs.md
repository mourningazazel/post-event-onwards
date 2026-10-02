# Scent-driven blind mobs — initial analysis (for review)

Status: **the mechanic is accepted (it's the core design). The owner's answers from 2026-09-27
are recorded at the end; the approaches built on them are proposals.** This
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
   - blocking by walls and closed doors: walls and the map edge absorb the share sent into
     them and pass none on, so scent never piles up against geometry (PEO-029)
   - deposits run on a 0-500 scale (`kPlayerScent`, D-007) with diffusion 0.5, decay 0.01,
     floor 1e-30. On a real 80x45 stage with absorbing walls a standing player's scent reaches
     41 cells by turn 50 and the whole stage (77) by turn 300; at floor 1e-6 it stopped at 29
     and distant Dead never moved. Measurements in `scent-performance.md`
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

## Owner answers (2026-09-27) and the design they lead to

### Scent physics

**Owner's answers:**

- Scent **diffuses** and moves **slowly**. Performance matters more than exact physics.
- **Wind** is regional (one direction plus an intensity over a wide area) and local (from game
  events). Stronger wind spreads scent further and loses some of it upward, in proportion to wind
  speed.
- Emitters **add** to the scent pool.
- The player can **mask or change** their scent.
- Strong scents **overpower** others.

**Proposed approach.** This trades exactness for speed, as you allowed. Every part is a tunable
data knob.

1. **Two layers per channel:**
   - A **fine trail layer** at tile resolution. Emitters deposit into it, it decays in place, and
     it has no diffusion, so it's very cheap.
   - A **coarse cloud layer**, e.g. one cell per 4×4 tiles. It diffuses, drifts with the wind and
     leaks upward.
   - Every few steps the trail "evaporates" part of its value into the cloud.
   - What a mob smells = trail + interpolated cloud.
   - Result: sharp trails near where things walked, and a soft spreading plume downwind. The
     expensive diffusion and wind work runs on 1/16 of the cells.
2. **Slow updates, amortized.**
   - The cloud updates every *K* steps. Alternatively, 1/*K* of the active blocks update each
     step, so the cost per step is flat and never spikes.
   - Only **active blocks** (non-zero scent) are processed. Most of an endless world has no scent
     and costs nothing.
3. **Wind as liquid-like drift.**
   - Each cloud cell pulls its new value from the upwind position, interpolated between cells.
     This stays stable at any wind speed; it's the standard trick from real-time fluid
     simulation, done in our own implementation.
   - Diffusion strength and upward loss both scale with wind speed.
   - Indoors, and behind walls, the wind is attenuated through a per-cell "openness" value, and
     walls and doors reduce how much scent passes between cells.
4. **Overpowering.**
   - Perceived strength of channel *c* = raw(*c*) × D ÷ (D + Σ dominance(*d*→*c*) × raw(*d*)).
   - The **dominance matrix** is data. For example, rotting-flesh scent strongly suppresses human
     scent; smoke suppresses everything a little.
5. **Masking.** An emitter has an emission profile per channel. Items and states change the
   profile. For example, "smeared in corpse gore" swaps *human* emission for *rotting*; rain scales
   emission down and speeds up decay.

### Mob behaviour

**Owner's answers:**

- With no scent at all, a mob makes a **random 8-direction step**.
- **Moving into the player is an attack**, whether or not the mob knows the player is there.
  Smelling is enough.
- Mobs don't push past each other unless they are **alerted or following**. Then a **contest**
  decides who wins, the winner **swaps** places, and side effects such as tripping are possible.

**Proposed approach:**

- **Bump-to-attack** is the same code path as moving. An unaware attack can have its own
  modifiers.
- **Contest resolution is a generic rules module.** It's data-driven: a score from strength,
  mass, momentum (consecutive moves in one direction) and state, plus a roll. Outcomes include
  swap, fail, trip (the loser falls prone for *N* steps and becomes an obstacle, possibly trampled
  by others) and more. The same module will later serve shoving doors, grappling and crowd
  crushes.

### Senses and alertness

**Owner's answers:**

- **Regular Dead:** scent only.
- **About 1 in 100:** also hear, but sound is processed only within some distance of the player.
- **Vision:** rarer still, and usually spawned by a trigger.
- **Alerting events:** physical contact, the player speaking, human sounds (grunts), being seen.
- **Follow cascade:** an alerted unit makes units within range **follow** it. Newly recruited
  units recruit others in turn.

**Proposed approach:**

- **Sound is an event, not a field.**
  - A sound event is *(position, loudness, kind: speech / grunt / gunshot / …)*.
  - Hearing-capable units within the audible radius react, with loudness reduced by distance and
    walls.
  - Only units near the player are checked, so the cost scales with the number of sounds made,
    not with the number of mobs.
- **Vision** units use the FOV system (symmetric shadowcasting). They are rare, so the cost is
  negligible.
- **Alert state machine.** Mob states: `Idle` (scent wandering) → `Alerted` (has a stimulus
  target) → `Following` (has a leader).
  - An alerted unit steers toward its stimulus: last known player position, or the source of a
    sound.
  - A follower steps toward its leader, or copies its leader's last heading.
  - Each step, alerted units and followers recruit idle units within radius *r*.
  - Knobs to decide in the walkthrough:
    - radius
    - chain-depth limit (so one grunt doesn't pull in a whole city)
    - probability of recruiting
    - how long the alert lasts
- **Processing order.** Alerted units and followers go first each step, then the scent-descending
  cascade for idle mobs.

### Population over time

**Owner's answers:** don't track individual mobs far away. Accumulated scent and enemy density
cause **migrations between regions**, which feed enemy generation at the edge of the detailed
area.

**Proposed approach:**

- Each world region cell holds an aggregate **population** and an **attraction** value (human
  scent, noise, corpses).
- Every so often, population flows along attraction gradients, and population also diffuses
  between cells.
- At the boundary of the detailed area, aggregate population **spawns in** as individual mobs,
  and **merges back** when mobs leave.
- The flow is deterministic at the aggregate level, so it is cheap, and it matches the aggregate
  tier in ADR-0005.

## Owner answers, round 2 (2026-09-27, [N004](notes/N004-realism-spirit-and-purpose-answers.md))

- **Scent exists only on ground tiles.**
  - "Upward drift" means removal from the field; there is no sky layer.
  - The coarse cloud layer is a 2D layer over the ground surface, following the terrain height.
    It is not a 3D volume.
- **Wind has two effects:** it spreads scent further and faster, **and** it loses scent upward
  faster. Rain washes scent out. Together they make outdoor travel much more feasible (purpose
  P-SC-05).
- **Lures, decoys and repellents are intended** (P-SC-04, P-SC-06).
  - Each archetype's weight table gets **negative weights** for repellent channels, so mobs step
    away from them. Mobs already choose between neighbouring cells, so this needs no new movement
    rule.
  - Repellent build-up also **reduces a region's attraction value**. That counters the slow crowd
    build-up of the population migration (see "Population over time").
- **Vision units are hunters** (P-EN-04).
  - They use real pathfinding (the tiers in `GAMEPLAN.md` §3.4) over the detailed area.
  - Their agitation raises the alert level around them, which pulls crowds along through the
    follow cascade. **Being swarmed is the main way to die** (P-EN-08).
- **Terrain changes crowd behaviour** (P-EN-06, P-WO-05). Each terrain material gets a **footing**
  value (movement time, trip chance) and an **obstruction** value (e.g. underbrush vs asphalt).
  Both feed the contest and trip rules. The player is subject to the same rules.

### Realistic population: first numbers

"As many Dead as a real outbreak":

- **Density.** Real urban densities run ~1,000–3,000 people/km² (suburbs) up to
  ~10,000–25,000/km² (dense cores).
- **Detailed area.** A 512 m × 512 m detailed area (0.26 km²) therefore holds about **260–800**
  Dead in suburbs and **2,600–6,500** in dense cores.
- **Headroom.** The benchmark ran 100k mobs in ~3 ms per step, so there is a lot of margin: even
  a packed downtown at full detail is fine.
- **Whole cities.** A city of 1M means about 1M Dead. Those live almost entirely in the
  **aggregate tier** (counts per region cell), which costs next to nothing.
- **Proposal:** a building's Dead come from its occupants (household size, office staff,
  shift). They are generated with the building's identity, and many are trapped behind closed
  doors (awaiting the owner's confirmation in `purposes.md`).

## Hearing units, gunshots and the kill rule ([N013](notes/N013-bikes-guns-bites-locks-animals-words.md))

**Kill rule.**

- The dead go down only when the **head is destroyed** or they are **totally disabled** (G05 body
  parts).
- Shots elsewhere mostly just make noise, and **firearms are common**, so noise is the real cost
  of using them.

**Hearing units in crowds** (about 1 in 100):

- **Any loud sound** (gunshot, glass, generator, alarm) makes nearby hearing units **move
  toward its source, even untriggered**.
  - They push through the crowd using the contest/swap rules.
  - Their movement drags followers along through the existing follow behaviour.
- **They only become *triggered*** (alerted: full pursuit and recruitment) when the sound is
  classified as **"human"**: speech, grunts, footsteps, a shot fired by a person. **Mechanical
  sounds** (alarms, engines, a thrown bottle breaking) only **attract**.
- Classification settled in round 4 (N016 / D-018): `human`, `mechanical`, `ambient`.
- **Pepper spray and stun guns do nothing** to the dead. The stun gun's crackle is itself a noise
  event.

## Owner answers, round 3 ([N014](notes/N014-the-dead-ai-content-skills-mood-drugs.md)) and the design they lead to

Round 3 replaces the **alert state machine and recruitment cascade** above with a single
weighted-draw model, adds **sound events**, **trample**, **speeds** and **aggregate scent
attraction**, and makes swarm behaviour a **reproducible test**. Where this section and the
earlier proposals disagree, this section wins.

### One movement rule for every one of the Dead

Each of the Dead picks its next cell by **one weighted random draw** over its free neighbours
plus "stay". The weight of a candidate cell is a sum of terms, each a data knob per archetype:

| Term | What it reads | Purpose |
|---|---|---|
| `scent` | fine trail layer + interpolated coarse cloud in that cell | the core mechanic |
| `aggregate` | the coarse **aggregate** (e.g. 8×8 tiles) scent total in the direction of that cell | pulls crowds toward the **higher-aggregate area**, so an even plain with one richer region drains the crowd that way |
| `company` | count of Dead within *r* in that direction | mild pull toward other Dead; with the draw's randomness this makes a crowd **fan out** yet **accumulate** where scent is strongest |
| `attractor` | pull from nearby **alerted or following** units, weighted by their state | how an alerted unit "brings" others: through the weights, never by handing out a state |
| `stimulus` | this unit's own **trigger target** (see below) | only non-zero for triggered units |
| `repellent` | negative for repellent channels (N004) | lures, decoys, repellents |
| `footing` | terrain footing and obstruction | terrain shapes crowds (P-WO-05) |

Idle Dead with no scent anywhere keep a random 8-direction step (round 1). "Stay" keeps its own
weight. All terms are integers and the draw comes from the unit's seeded stream, so the whole
crowd is deterministic and replayable.

**Built (PEO-009, D-038 B):** the `scent` and `company` terms and staying. A neighbour's log-odds
(sixteenths of an octave) are lean x its scent above the unit's / distance_cost, where the lean
rises from `lean_edge` (0) at the edge of reach to `lean_full` (3 octaves) at full strength, plus
`company_gain` (a quarter octave) scaled by how full of Dead its box is (half-size 3, four cells
out); staying weighs as a level neighbour. Walls and cut corners weigh nothing. Every term reads
only the last update, so they are baked into a `DesireField` after each update (9 log-odds a
cell, 16 bytes a cell with its counts, 16 MiB at 1024x1024; cells with no scent or company near
skip the maths) and patched at commit round the deposit. The draw is a hash of (salt, second,
unit); a drawn tile that is taken stays put (D-031). Measured (tests/core/test_swarm.cpp): of
the draws that move, 96% climb within 10 cells of a standing source, 55% beyond 45, falling band
by band between; a strong region's share of an even horde grows 4.5x; a packed cluster on flat
scent covers 5.4x its area with never two to a tile. Waiting: `aggregate` (PEO-051), `attractor`
and `stimulus` (PEO-010), `repellent`, `footing`; knobs per archetype.

### Triggers: general direction, not a path

A unit becomes **triggered** only by its own stimulus: it **touched** the player, **saw** the
player (vision units), or **heard** a human-class sound (hearing units). Triggered units:

- steer by **general direction** toward the trigger's last known position (the `stimulus`
  term is a direction cone, not a path), so a doorway or a wall can defeat a blind one;
- move faster: **alerted ×1.5**, **following ×2** steps per turn (data);
  - *alerted*: has a stimulus position;
  - *following*: has line of contact or fresh sound from the player and keeps updating the
    position; vision and hearing units are the ones that can follow.
- **do not spread their state.** Their `attractor` weight pulls idle Dead along; those stay idle.
  A pulled unit is triggered only by **its own** stimulus, for example the player's retaliation
  sound when a leading hearing unit attacks, or a following unit's contact.
- **expire** after moving a set distance (data) without a fresh stimulus, or after a set number
  of turns, and resume idle behaviour **where they now are**. That displacement is the point:
  the group is now somewhere else, which changes local attraction and where the next Dead
  spawn in. Being triggered by a scent-only unit is a **swarming** risk, not a chase: walk away
  and it cannot follow far, but the place you left is now a bigger group.

### Sound events

Kept deliberately simple:

- A sound event is `(position, kind, dB)`. **Kinds:** `human` (speech, grunts, footsteps of a
  running player, a shot fired by a person), `mechanical` (alarms, engines, glass, a thrown
  bottle), later others. **dB** sets the radius through one attenuation rule (distance, walls,
  doors); the classification never grows into a taxonomy.
- The event **queries the radius** for sound-reactive units (about 1 in 100) and applies the
  rules: any loud sound **attracts** hearing units toward the source (they push through crowds by
  the contest rules); a `human` sound **triggers** them.
- A hearing unit moves toward **that** source and follows only that or a **newer** sound, so it
  can be led, and so a running player is trackable by footsteps.
- Cost scales with sounds made, not with the Dead's count.

### Trips and trample

- **Tiles are shared (ADR-0013, N015).** Several units may stand on one tile up to its
  capacity; "free neighbour" means "below capacity". Every step into, out of or within a
  crowded tile rolls for **trip** or **knock-out** to an adjacent tile, so packed doorways
  churn and spill instead of freezing.
- **Push contests** (round 1) can end in a trip. A tripped unit is prone for *N* turns and is an
  obstacle. Units pushing through a crowd **roll to trip on every contested move**, so a frenzied
  horde trips itself and slows down. Trip chance rises with speed multiplier and bad footing.
- **Trample:** a unit that moves over a prone unit (including the player) rolls, with a weight
  from its mass and speed, to deal **blunt** damage to the one underneath. Body-part damage
  (G05) applies. A prone player in a crowd is in mortal danger, which is intended.

### Population and spawning

Confirmed direction (round 1 "Population over time", P-EN-07): beyond the detailed radius the
Dead are a **population number per region cell** that drifts along attraction, not units. At the
edge they **spawn in as units** and **merge back**. Round 3 adds:

- The weighted draw's accumulation behaviour is meant to **replace most spawning rules**: crowds
  form because units walk to strong scent, not because a generator places them there.
- Areas with high concentration **place more Dead** when adjacent areas are explored: the region
  population feeds the edge spawn count.
- The radius is a data value chosen for performance.

### Reproducible swarm tests

Every change to the movement rule or its knobs must keep these scenarios passing (harness §2):

| Scenario | Expectation |
|---|---|
| `swarm-accumulation` | An even field of Dead with one stronger scent region ends with a measurably higher share of units in that region after *T* turns. |
| `fan-out` | A tight starting cluster with flat scent spreads to a wider area over *T* turns (no clumping artifacts). |
| `trigger-displacement` | A scent-only unit touched by the player moves toward the player, expires, and ends closer to the player's old position than it started. |
| `no-state-spread` | After one unit is triggered in a crowd, no other unit's state changes unless it receives its own stimulus. |
| `sound-lure` | A `mechanical` sound draws hearing units without triggering; a `human` sound triggers them. |
| `frenzy-trips` | A horde forced through a doorway trips more often than the same horde on open ground and arrives later. |
| `trample-damage` | A prone unit under a moving crowd takes blunt damage at the configured rate. |

Each runs headlessly with the golden seed set and reports its metric, so a tuning change shows
as a number, not a feeling. The siege suite (PEO-088, `peo_siege`, docs/testing.md) runs the horde
round three buildings for 48 game hours on demand, against a committed baseline.

## Owner answers, round 4 ([N016](notes/N016-review-answers-wind-characters-sound-gpu-z-levels-speed-items.md)) and the design they lead to

Answers to the 2026-09-29 codebase reviews. Recorded as D-016 to D-023; the transport and rot
questions the reviews raised are open as D-024 and D-025.

### Wind, indoors and out (D-020)

- **Wind carries scent ahead** of a moving player when it blows from behind; strong enough, the
  front outruns the walker. The "never ahead of the player" reading in D-008 is withdrawn: the
  Dead may head a player off downwind. Round 1's regional wind stands; the transport must do it.
- **No wind indoors.** The calculation carries an explicit stop: a per-cell openness of 0.
- **Wall rules are house-specific** and live outside the base transport. The owner's starting
  point: **inner walls build scent up** rather than absorb it, since there is no air above to
  vent into; outdoor walls and the map edge still absorb (D-007's wall rule). The pathfinding
  review measured that buildup restores reach but flattens the gradient in rooms. **Not built:**
  D-024 A chose a transport that reaches through corridors and doors without it (below).
- **Built (PEO-048).** Each stage draws a wind (any whole degree, intensity up to
  `WorldParams::wind_max`, 0 by default: calm) from its seed; `Stage::openness` is 255 outdoors,
  0 indoors. A step costs `max(1, C - wind_d x openness / 255)`, `wind_d` from a fixed integer
  cosine table (`peo/core/wind.hpp`), and `gust` rounds a update then carry scent only downwind
  from outdoor cells. Measured (200x120, walker east, full wind): gust 0 reaches 0 cells ahead,
  gust 2 reaches 58, a walled indoor corridor 0; behind, 119 in all three. A standing source
  reaches 98 cells downwind, 39 upwind. Every round takes every offer it allows, so a cell a
  gust reached spreads sideways the next update, and `patch_deposit` stays exact (commit keeps
  the speculation). One difference from the probe: an indoor band with no walls lets scent out
  sideways, along the wind outside and back in ahead; the probe's push lost those offers (and
  with them almost all indoor spread once gust > 0). The game plays with full wind, gust 2.

### The transport: a geodesic field (D-024, [N017](notes/N017-scent-transport-rot-and-item-detail.md))

- A cell holds **strength − C × distance − K × age** as an `int32`, distance measured along
  walkable routes, so scent goes round corners and through doors the way feet do, and walls
  absorb. It replaces diffusion and D-008's IIR far layer.
- The front advances **`speed`** cells per update, plus up to **`gust`** more downwind. Step
  costs fall downwind and rise upwind, scaled by a per-cell **openness** (0 indoors), which is
  D-020's wind as one field. Stairs and downward tiles are routes (D-019).
- Work is bounded by a **reach radius** around the player; sealed buildings have no route in.
- **Rot scales, never zeroes (D-025):** the value is log strength, so a Dead subtracts rot × a
  scale from what it reads.
- Measurements and the probe code: the pathfinding review, sections 3, 4 and 8.

### Only near the player (D-019)

- **Dispersion runs within a radius of the player.** Beyond it the section aggregates (round 3,
  PEO-039) stand in. The radius is a setting and the budget knob.
- **Z-levels:** a downward tile with a route below is one more neighbour for scent. Scent never
  travels in the air above ground tiles.
- **Sealed buildings are never simulated.** Generation tags a building **opened** (broken window,
  open door); it joins the field when the player approaches. Soak (D-013) remembers long stays,
  so far-away indoor dispersion need not be exact.

### Sound kinds and the ambient floor (D-018)

- Kinds are `human`, `mechanical`, `ambient`. **Ambient** sound (weather, crows, a distant
  generator) shows as a descriptor when the player examines the surroundings, an action, and
  sets the area's **ambient noise floor**.
- The floor matters in one place: whether hearing Dead can hear the player, human sound
  against the floor, the inverse relation rot has to human scent (D-012).

### Speed of the Dead (D-022)

- Expected steps per update = update period / `step_seconds`, a real number. The integer part is
  guaranteed, the fraction is the chance of one more step, drawn from a counter-based hash per
  Dead. Cap: 6 steps per update. Faster Dead take two extra steps, not one, when the fraction
  says so twice.

### Budget (D-021)

- Fields stay on the CPU and integer-valued so a GPU port stays possible. Commit under 1 ms; a
  speculated update under 100 ms on the M1 Air. Measured at 200×120 with 5,000 Dead, 512×512 with
  20,000 and with 50,000.
