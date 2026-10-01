# Threads and the GPU: what the hot paths could become

Architect research, 2026-10-01, for Ryan's question in the project thread (2026-10-01): what
of today's heavy work could move to other threads or the GPU, and what that would let the game do
at a much larger scale. This is a discussion document. It changes no code, no decision and no
queue item. The questions for Ryan are at the end.

Measured means run in the cloud container: a 4-core Xeon at 2.1 GHz, GCC, `-O3`, against the
real `peo_core` library. Every new variant was checked bit for bit against the game's own code on
every update. Estimated means worked out from public hardware figures, and is marked as such.
The benchmark is `spikes/parallel-fields/fieldbench.cpp`.

## The short answer

- **Today's real bottleneck is saturated scent, not the Dead.** With standing scent sources the
  field changes everywhere within reach on every update. Measured at steady state: 7-10 ms per
  update at 512x512 and about 200 ms at 2048x2048, which is already over the 100 ms budget.
- **The scent update does not care what order cells are visited in**, so it can be computed the
  other way round: each cell pulls from its neighbours instead of each changed cell pushing out.
  Same bits as today on every update tested, **20-46x faster on one core and 70-160x faster on
  four**. No GPU is needed for that.
- **The Dead can read a precomputed direction** instead of nine scent cells. Same moves as today,
  **3.5-6x cheaper per decision on one core, 7-9x on four**.
- **Speculate's full copy of the field disappears** with two buffers that swap.
- **Threads stay deterministic** because everything combines by maximum or minimum, and the
  Dead's draws already come from per-unit hashes. Core can use threads without owning them (Q1).
- **The GPU does not pay at today's sizes**, but the fields are integers now, so a GPU can match
  the CPU bit for bit. It pays when the game wants many fields, very large areas, or many
  updates at once (Q2).
- **That opens up:** scent that remembers a whole stage, many scent kinds, sound that goes round
  corners, fire and smoke, crowd pressure, richer Dead at the same horde size, and long actions
  (sleeping, searching every drawer) that run hours of game time at full detail in well under a
  second (section 6, Q3).

## 1. What the heavy work is, and what it uses

| hot path | what it computes | what limits it | today |
|---|---|---|---|
| Scent update (`ScentWave::update`) | Every cell that changed last round offers its value, less 8, to its open neighbours. A list of changed cells drives the next round. | Memory latency. Each offer writes to a scattered cell and checks a stamp; nothing is sequential enough for the CPU to prefetch or vectorise. | about 40 ns per active cell |
| Speculate's copy | `out.scent = scent_` copies the whole field, stamps and lists before each speculated update. | Memory bandwidth. Scales with the stage, not with what changed. | 0.13-0.35 ms at 512x512, 2 ms at 1024x1024, 10-11 ms at 2048x2048 |
| The Dead's poll and decisions (`strongest_neighbour`, `decide_move`) | Each unit reads 9 scent cells and 8 wall cells around it, then its target's occupancy and reservation. | Memory latency: every unit is a fresh, scattered cache miss once the map outgrows the cache. | 60 ns (map in cache) to 108 ns (2048x2048, 1M Dead) per decision |
| Commit | Replays the Dead's recorded moves and patches the player's deposit in. | Latency, 2-5 ns per Dead. | under 1 ms to about 200k Dead |

The CPU spends almost all of this time waiting for memory, not doing sums. That is why a faster
core helps little, and it is also why the biggest wins below come from changing how the data is
walked before adding any threads.

One finding matters more than the rest. `performance-targets.md` already noticed that the `perf:`
lines report an early, empty field. At steady state with standing scent sources (6 per 1000
tiles), the scent update alone measured here is 7-10 ms at 512x512 and 190-250 ms at
2048x2048. Saturated scent is today's real bottleneck, well ahead of the Dead. The reach radius
(PEO-047, D-019) will cap how much of a stage is active, but the cost per active cell stays, and
it is that cost which decides how large the radius can be.

## 2. The scent field can be computed the other way round

`ScentWave::round` already has the property that makes everything else here possible. A round is
a maximum over offers made from the values cells held when the round started, so the result does
not depend on the order cells are visited (the comment in `scent_wave.cpp` says so; it is what
makes `patch_deposit` exact). A maximum can be taken in any order, on any number of threads, or on
a GPU, and give the same integer.

So the same update can be written as a **pull**: every open cell looks at its eight neighbours in
the previous buffer, takes the best offer, keeps it if it beats its own value and the age line,
and writes the result to a second buffer. A cell that did not change last round has already made
every offer it can (values only rise), so pulling from every neighbour gives exactly what pushing
from the changed cells gives. The benchmark checks this on every update of every case below; all
of them match bit for bit.

Why it is faster: the pull reads rows in order and does the same branch-free work for every cell,
so the CPU streams memory and processes 8 cells per instruction with AVX2 (4 with SSE2 or the
M1's NEON). The push touches each cell through a random write.

The pull does the same work for a cell that is not changing as for one that is. That is solved
with 128x8 tiles: a tile is processed only if it or a neighbouring tile changed last round. With
two buffers, a tile nobody processes already holds the same values in both, so nothing is copied.

Scent update, microseconds, median of 40 updates after 80 warm-up updates (p95 in brackets):

| map | sources per 1000 tiles | sparse (today) | full pull | tiles, 1 thread | tiles, 4 threads | copy (today) | flow grid, 1 / 4 threads |
|---|---|---|---|---|---|---|---|
| open 512x512 | 0 (walker only) | 72 (94) | 253 | 39 (47) | 12 (29) | 131 | 49 / 14 |
| open 512x512 | 6 | 9,523 (11,155) | 287 | 398 (530) | 110 (141) | 349 | 567 / 147 |
| town 512x512 | 0 (walker only) | 3 (6) | 260 | 16 (22) | 7 (10) | 149 | 21 / 9 |
| town 512x512 | 6 | 6,994 (7,491) | 261 | 342 (459) | 102 (164) | 327 | 513 / 135 |
| open 1024x1024 | 6 | 44,447 (49,482) | 1,047 | 1,379 (1,665) | 377 (496) | 2,080 | 2,014 / 497 |
| town 1024x1024 | 6 | 33,520 (37,168) | 919 | 1,174 (1,382) | 372 (485) | 1,569 | 1,740 / 454 |
| open 2048x2048 | 0 (walker only) | 177 (208) | 3,602 | 88 (108) | 34 (54) | 2,309 | 112 / 36 |
| open 2048x2048 | 6 | 249,124 (265,325) | 4,527 | 5,404 (6,169) | 1,557 (2,797) | 11,236 | 7,821 / 2,094 |
| town 2048x2048 | 6 | 194,203 (204,541) | 4,402 | 5,115 (5,749) | 1,370 (1,568) | 10,377 | 7,346 / 1,939 |

- `sparse` is today's `ScentWave`. `full` pulls every cell of the stage. `tiles` pulls only the
  active tiles. `tiles, 4 threads` splits those tiles across 4 threads. `copy` is what
  `speculate()` pays today before it starts.
- With standing sources the pull is 20-46x faster on one thread, and 70-160x
  faster on four.
- With only the player walking (no standing sources), the field is small either way and both
  stay well under a millisecond.
- The copy disappears with two buffers: the speculation writes the new field into its own buffer
  and commit swaps it in, as `finish_from` already swaps today.

Threads and instruction sets:

- **Scaling.** Measured in a separate run with 64x16 tiles: 2 threads gave 1.75-2.0x over one,
  4 threads 3.0-3.7x. The occasional high p95 on 4 threads is the container's other work
  pre-empting a spinning worker.
- **The pull needs AVX2 or NEON to shine.** The same code built for plain x86-64 (SSE2, which has
  no 32-bit integer max) ran 3-5x slower, and for SSE4.2 about 3x slower, than the AVX2 build
  above; still 6-15x faster than today. Every CPU in the tier table has AVX2 and the M1 has
  NEON, so a shipped build would compile this kernel for AVX2 with a plain fallback. The M1's
  figure is not measured.
- **Tile shape matters.** At 1024x1024, 128x8 tiles ran the single-thread pull in 1.13-1.18 ms,
  64x16 in 1.63 ms. The table uses 128x8.

## 3. The Dead can read a direction instead of nine cells

Between updates the Dead read only the last update's scent, never the player (D-031). So where a
calm Dead wants to go is a pure function of the tile it stands on, and it is the same for every
unit on that tile. That makes it a **flow field**: one byte per cell naming the neighbour a unit
there would step to, computed once per update, in the same pass and over the same active tiles
as the scent.

The age line does not change which neighbour is strongest, only whether it is still worth
stepping to, so the byte is stored without it and a unit checks one value at decision time.
The decision then reads 4 bytes instead of 17 scattered cells.

Within one second's slot, units decide in ascending order and a calm Dead never retries
elsewhere, so the outcome is simple: each target tile goes to the lowest-numbered unit that
wants it, if it is free. That is a per-tile minimum, which threads can compute in any order and
still agree.

One batch of the Dead deciding, nanoseconds per decision (median of 7), with the field saturated:

| map | Dead | `decide_move` (today) | flow grid, 1 thread | flow grid, 4 threads | moves |
|---|---|---|---|---|---|
| open 512x512 | 50,000 | 59.6 | 10.1 | 8.3 | 34,522 |
| open 1024x1024 | 200,000 | 81.1 | 15.5 | 8.9 | 138,121 |
| open 2048x2048 | 1,000,000 | 108.6 | 29.6 | 12.0 | 627,085 |
| town 2048x2048 | 1,000,000 | 107.4 | 29.2 | 11.9 | 602,471 |

Threads help the Dead only for big batches: with 2 threads the 50,000 batch was slower (14.4 ns)
than one thread with the grid, because waking threads costs more than the work. A real batch is
one second's slot, about a ninth of the movers, so threads pay here from a few hundred thousand
Dead upwards.

Every variant gave exactly the same moves as `decide_move`. The flow grid costs 0.5-0.6 ms at 512x512 and 7-8 ms at 2048x2048 with every tile active on one core, a quarter of that on four, and well under 0.1 ms with only the player walking per
update (the last column of the table in section 2). It is paid once per update, however many Dead there are, so it pays for itself from roughly 20,000 Dead on a fully active field and at any horde size when only the player is moving.

**Built (PEO-079)**, one thread: `ScentWave::flow_target` reads the byte, the poll and
`decide_move` call it, and every move is unchanged (a pinned golden of the Dead's positions).
At 512x512 with 50,000 Dead and 6 sources per 1000 tiles the byte refresh adds 0.45 ms to the
saturated update (222 to 675 us), and the Dead's share of a speculated update falls from 4.3 ms
to 1.8 ms (2.3x); the whole speculate goes from 4.6 to 2.6 ms. See
`docs/design/scent-performance.md`.

## 4. Threads: what can be split, and how it stays deterministic

The current code already follows most of the rules threads need:

- **Order-independent combining.** Scent is a maximum; claims are a minimum. Both give the same
  answer in any order. Sums of integers also qualify. Anything appended to a shared list does
  not, so per-unit results go into per-unit slots and are read back in index order.
- **No shared random stream.** Slot plans already use a counter-based hash of (salt, cycle,
  index) (D-022, D-031), so any thread can compute any unit's draw.
- **Integers only** in anything that feeds back (D-021). Already true of the fields.
- **Read last, write next.** Each pass reads one buffer and writes another, so no thread sees
  another's half-finished work.

What threads can do, in order of value:

1. **Run independent parts of one update side by side.** In `speculate()` the scent update and
   the Dead's seconds up to the boundary are independent: the Dead read the *current* field, the
   update writes the *next*. Two threads, no change to any algorithm, no determinism risk.
2. **Split the field pull and the flow grid by tile** (measured above).
3. **Split the Dead's intents by unit**, then resolve claims by minimum (measured above). This
   pays off most once each decision gets more expensive (the weighted draw, PEO-009).
4. **Pipeline the frontend.** The frontend already owns a worker for speculation (ADR-0012).
   Further workers can generate the next stage, resolve container seeds near the player before
   they are opened (D-023), and write saves, all off the input path.

ADR-0012 says core spawns no threads, and that rule is worth keeping: tests stay simple and the
frontend controls the machine. Core can still use threads it does not own. The frontend hands
core a "run these N pieces" function (a parallel-for). Tests pass a serial one, and a test
checks that serial and threaded runs give the same world bit for bit, beside the existing
speculate/commit golden test. That is question Q1.

**Built (PEO-080)**, as above: the field's tiles, the direction bytes, speculate's two halves
and, past 16,384 a second, the Dead's decisions. M1 Air, `-O3`, performance cores, median of
40: serial is no slower than before the executor (saturated 512x512 update 680 -> 621 us, the
byte refresh no longer overlaps; 512x512/50,000 Dead speculate 2,640 -> 2,606 us). Saturated
update, 1 / 2 / 4 threads: 512x512 625 / 323 / 245 us (2.56x), 1024x1024 2,340 / 1,196 / 906 us
(2.58x); windy with gust 2, 1024x1024 14.7 / 8.5 / 6.6 ms (2.2x). Speculate at 512x512 with
50,000 Dead: 2,607 / 1,882 / 1,891 us (1.38x: the Dead's half bounds it, and its decisions run
serially inside that piece); windy 6.3 / 4.4 / 3.6 ms. Splitting the decisions gains 1.04x on
a whole step at 50,000 Dead and 1.2-1.5x past a million, never less than 0.97x.

## 5. The GPU: what it is good at here

What the GPU is: thousands of simple cores with 5-10x the memory bandwidth of the CPU, but a
long way away. A dispatch plus the wait to read results back costs a fraction of a millisecond
(the M1 spike measured 0.9 ms for a 1M-element dispatch and readback), so it pays only for large,
uniform, bandwidth-heavy work.

What changed since D-021 ruled it out: the 2026-09-29 review's objection was float fields, which
no GPU reproduces bit for bit. The fields are integers now (D-024), and the pull form needs no
atomics. Integer maximum and subtraction give the same bits on every GPU and CPU, so a GPU field
can match its CPU reference exactly, which GAMEPLAN section 4 and SYSTEMS rule R11 already require.

What it would not help today: at 512x512 the CPU pull takes about 0.26-0.29 ms for the whole stage on
one core. A GPU round trip costs more than that. For the current scent field and the current
Dead, the CPU work in sections 2 and 3 is the right move, and the GPU adds nothing.

Where it starts to pay (estimates, from bandwidth: the pull moves about 9 bytes per cell):

| work per update | CPU, 4 threads (measured or scaled) | GPU, estimated |
|---|---|---|
| 1 field, 1024x1024, all active | about 0.37 ms for the field plus 0.45-0.5 ms for the flow grid (measured) | 30 us on a GTX 1080 class card, 140 us on the M1, plus the round trip |
| 1 field, 4096x4096 (about 4 km square), all active | about 18 ms on one core (scaled), 5-8 ms on four, limited by memory bandwidth | about 0.5 ms (GTX 1080), 2.5 ms (M1) |
| 8 fields at 2048x2048, all active | about 11 ms (8 x the 4-thread row) | about 1-2 ms |
| a long action: 600 updates (one hour) of a 1024x1024 field | about 0.5 s (scaled from the row above); 20-27 s with today's sparse update | well under a second, with no readback until the end |

So the GPU becomes worth its complexity when the game wants **many fields, very large areas, or
many updates at once**. Those are exactly the things the game's design keeps reaching for.

The Dead themselves should stay on the CPU. Their seconds run in sequence (each second's landings
change the next second's choices), the batches are small, and the flow grid makes each decision
cheap. The GPU can feed them: it computes the fields and the flow grid, and the CPU reads a
byte per unit.

## 6. What this could let the game do

Each idea is a direction, not a plan. Costs are rough and say which resource they lean on.

1. **Scent that remembers a whole stage.** Today scent is bounded by a reach radius around the
   player (D-019) because a saturated field is expensive. With the pull on threads, a 1024x1024
   stage fully active costs about 0.37 ms for the field plus 0.45-0.5 ms for the flow grid (measured) per update on this machine. Trails could persist for
   hours across the stage, and the Dead far away would drift along the routes you used
   yesterday. On the GPU, a 4 km region.
2. **Many kinds of scent.** Blood (PEO-003), rot (D-025), food and cooking, smoke, chemical lures
   and repellents (N004) as separate fields with their own costs and lifetimes, in one pass. The
   pull reads the walls once for all of them. Gameplay: smear rot to hide, bleed and become a
   beacon, cook and draw a crowd to the wrong building.
3. **Sound that goes round corners.** The same geodesic kernel with different costs: walls and
   doors attenuate instead of blocking, open air carries further. A gunshot in an office is heard
   down the corridor, not through three walls. A generator raises the ambient floor (D-018) in a
   shape you can see on the map and use to mask footsteps.
4. **Fire, smoke and gas.** Cellular rules over materials, many rounds per update. Fire draws or
   destroys a crowd, smoke hides scent, gas clears a room. These are dense stencils, the best fit
   the GPU has.
5. **Crowd pressure.** A density field from the occupancy grid (the round 3 `company` term) and
   a pressure field pushed through packed tiles. A barricade or a door fails because of the crowd
   ten tiles deep behind it, not the three touching it. Crushes and spills through doorways come
   from the field, not from special cases.
6. **Richer Dead at the same horde size.** PEO-009's weighted draw has about seven terms per
   unit. Every term that reads only last update's state (scent, aggregate, company, footing,
   repellents) can be baked into a per-cell "desire" field of eight weights in the dense pass,
   leaving only the unit's own terms (its trigger, its attractor) per decision. The richer rule
   then costs close to what one lookup costs today.
7. **Long actions at full fidelity.** Searching each drawer (D-027), sleeping, barricading: many
   updates in a row with no input. At today's saturated cost an hour of game time is 600 updates
   and 4-6 s of scent work alone at 512x512. With the pull on threads it is about 0.15 s, field and flow grid together. Waiting stops being
   something the world has to fake.
8. **Hordes as a moving population far away.** Beyond the detailed radius the Dead are numbers
   per region cell (scent-mobs.md, population). On the GPU that region grid can be fine enough
   and wide enough that a migrating horde is a real shape on the map, which becomes individual
   Dead as it reaches the detailed area.
9. **Goal fields for the Dead that see and hear.** The risen player (PEO-021) and hearing units
   follow sounds and sight, not scent. One flow field per goal (the player, each recent sound),
   several at once, all from the same kernel.
10. **More z-levels near the player.** N016 noted that stairs multiply the cells near the player.
    Dense tiles per storey, with stairs as extra neighbours, keep a multi-storey town in budget.

## 7. Suggested order

Nothing here is queued. If Ryan wants to go ahead, this order keeps every step bit-identical and
measurable, and fits PEO-074 (perf levers) and PEO-070 (`peo_bench`):

1. **The pull field with two buffers and tiles, one thread.** No ADR change, no new dependency,
   the biggest single win. `patch_deposit` and commit's swap carry over.
2. **The flow grid.** The poll and every decision read it.
3. **The parallel-for hook (Q1)**, then the side-by-side update and the threaded tiles and
   intents.
4. **The GPU only for new large fields (Q2)**: the first one that does not fit on the CPU, with
   a CPU reference and an equivalence test, as GAMEPLAN section 4 already plans.

## 8. Questions for Ryan

**Answered 2026-10-01 (N020): Q1 B (D-035), Q2 C (D-036), Q3 C (D-037).** ADR-0014 records the
architecture; the work is PEO-078 to PEO-084.

Each has a recommendation; the reply in the thread asks the same.

- **Q1. Threads in core (ADR-0012 says core spawns no threads).**
  A: keep core single-threaded. **B (recommended): the frontend passes core a parallel-for; core
  still spawns nothing, tests run it serially, and a golden test checks serial and threaded runs
  match bit for bit.** C: core owns a thread pool.
- **Q2. The GPU (D-021 says CPU only, revisit near the budget).**
  A: CPU only, for good. **B (recommended): CPU first; the GPU is reserved for the first new
  large field that does not fit on the CPU, always with a CPU reference and an equivalence test.**
  C: start a GPU field backend now.
- **Q3. Which scale-up matters most to you first?**
  A: scent that remembers a whole stage. B: more kinds of field (blood, smoke, sound, fire).
  **C (recommended): richer Dead and crowd pressure, because hordes are the second pillar and
  PEO-009 is the next mechanic.** D: long actions at full detail.

## Method and limits

- Maps: the stage generator's open map (10% random walls) and the PEO-035 town (houses, offices,
  one-wide corridors) tiled to size. Standing sources: 6 per 1000 floor tiles, as the `perf:`
  cases. A walking player deposits every update.
- Dead: unique random floor tiles, all deciding in one batch; real batches are one second's
  slot, smaller, so thread overhead weighs more there.
- The container has 4 cores and a large shared cache; the tier PCs have 6 and the M1 has 4 fast
  plus 4 efficiency cores. Thread scaling there will differ; only the Builder's numbers count
  for `architecture.md` (perf skill).
- The GPU figures are estimates. There is no GPU in the container.
