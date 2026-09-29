# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007 to D-012, D-014, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`. They still hold.

## 2026-09-29 · D-023 · Items are seeds until touched

User decision (N016). An unopened container is a seed; its contents exist as the calculation that
makes them until one is modified. An item reads its properties through its seed; the first change to
a property instantiates it as a unique record. Nothing ticks per turn: time-driven change (rot,
quality) resolves from the clock on access, name first, details after, unless refreshing a whole
container on view measures cheap at 500 items. A hidden per-container count cap, far above play,
refuses new adds but never blocks growth from an existing stack and never crashes. Why: a furnished
house is about 800 loot items and a store 13,000; only what differs from generation deserves memory.
Consequence: PEO-050; D-026 to D-029 refine it.

## 2026-09-29 · D-022 · The Dead take a probabilistic number of steps per update

User decision (N016, review option 3). Expected steps per update = kUpdatePeriodSeconds /
step_seconds, a real number: the integer part is guaranteed, the fraction is the chance of one more
step, drawn from a counter-based hash of (seed, update, Dead index), never a shared stream. Cap at
6 steps per update. Why: speed stays a gradient (D-015) with no per-second Dead pass, and cost scales
with steps taken. Consequence: replaces PEO-040's one step per update; the commit repatch radius
grows with the step cap; PEO-042.

## 2026-09-29 · D-021 · Fields stay on the CPU; the budget is the gap between inputs

Architect decision, delegated by the user (N016). No GPU work: D-002's bit identity and the commit
patch need one code path, and every field measured by the reviews is bounded by reach, not map size.
To keep a port possible, new fields are integer-valued stencils over flat arrays. Revisit when the
largest variant below has under 50% headroom. Budget, replacing the 16 ms target: commit under 1 ms;
a speculated update under 100 ms on the M1 Air, the floor machine, so most of the 333 ms input gap
(D-011) stays headroom. Measured at 200x120 with 5,000 Dead, 512x512 with 20,000 and 512x512 with
50,000, each with many emitters. Consequence: PEO-043; vision.md updated.

## 2026-09-29 · D-020 · Wind carries scent ahead; indoors no wind, and inner walls build up

User decision (N016). Outdoors, wind from behind the player carries scent ahead of them, further
than they walk when strong enough; D-008's "never ahead of the player" reading is dropped. Indoors
there is no wind, with an explicit stop in the calculation (per-cell openness 0). Wall and barrier
rules are house-specific and live outside the base transport. The owner's starting point, open to
expansion: inner walls build scent up instead of absorbing it, since there is no air above to vent
into; outdoor walls and the map edge still absorb. Why: scent must reach the room at the end of a
1x8 corridor, and long turning corridors exist only indoors. Consequence: the solver is D-024;
PEO-048; PEO-030 and PEO-035 are rebriefed after D-024.

## 2026-09-29 · D-019 · Scent is simulated only near the player; sealed buildings are skipped

User decision (N016). Detailed dispersion, z-level routes included, runs only within a radius of
the player; beyond it the aggregates (round 3; PEO-039's sections) stand in. A downward tile with a
route below is one more neighbour for scent; scent never travels in the air. A sealed building is
never simulated. Generation tags a building opened (broken window, open door); it joins the field
when the player approaches. Soak (D-013) already remembers long stays, so indoor dispersion far
away need not be exact. Why: a 512x512 map with many floors would otherwise multiply tiles for
nothing. Consequence: the reach radius is a setting; PEO-047, PEO-049; the opened tag is a
generation output.

## 2026-09-29 · D-018 · Sound kinds: human, mechanical, ambient, with an ambient noise floor

User decision (N016), closing the open classification in scent-mobs.md. Ambient sound is shown as
a descriptor when the player examines their surroundings (an action) and sets the area's ambient
noise floor. The floor matters only for whether hearing Dead can hear the player: human sound
against the floor, the inverse relation rot has to human scent (D-012). Consequence: PEO-010 keeps
human and mechanical; PEO-045 adds ambient and the floor.

## 2026-09-29 · D-017 · Numeric physical stats; everything else descriptive, skills in five bands

User decision (N016) closing G04 D4.5 and amending N014's percentages. Physical attributes are
numbers. All else, skills included, is prose: a skill shows as one of five level descriptions
tiered at 20 (1-20 up to 81-100). The sheet groups descriptors: appearance by trait (hair length,
colour and style; physique for strength; conditions as the ailment), preferences that give mood
boosts, and history as "who you are" (previous job, seeding skills and gear) and "where were you?"
at the Event. Consequence: G04 updated; PEO-046; the success model keeps its internal percentages.

## 2026-09-29 · D-016 · Character creation is fully random

User decision (N016) closing G04 D4.3: B. Each run rolls the whole character: stats, starting
items, background, physical makeup as flavour text with small telling details, and psychiatric,
medical and physical conditions. Previous job shapes generation and starting skills (a cop starts
with a gun and firearms skill); "where were you?" places the start near a fitting area (an office
worker near the urban core), never exactly. Unlucky characters are intended. Consequence: PEO-046
designs the generator behind PEO-008 and PEO-012.

## 2026-09-29 · D-015 · The world keeps a clock in seconds; actions take time

User decision, amending D-002 and D-014. The world counts game seconds. Heavy systems (scent spread,
the Dead) update on their own fixed cadence, every 6 s by default, whatever the player does. Each action
has a duration in seconds (a walking step 6, a running step 3, a bicycle tile 3, a thrown or falling body
a run of moves each with its own time) and lands between updates, so a runner moves twice per update
and a walker once. The player's scent is logged per second on the tile occupied and applied at the next
update. Rates (scent, soak, rot, Dead speed) are per second. Why: speed is a gradient, not a fixed count
of ticks, and compute per update does not depend on how the player moves, so running never doubles
scent or cost. Consequence: the world still waits for input (D-002); the next update is computed ahead
while idle and the player's moves only patch around the tiles they touched (PEO-007's model). Dead
speeds become seconds per step. PEO-040 reworks the clock; running is PEO-041.

## 2026-09-29 · D-013 · Buildup is a soak value on each tile, capped, fed by contact

User decision: A. It replaces D-010's fade rule, which the house probe showed does nothing indoors:
absorbing walls take about 6% of a room's scent a turn against a 1% fade, and the field settles in
minutes, so it cannot remember days. Each tile holds a soak value from 0 to a ceiling. The player standing
on it, or using the object on it, charges it by a share of the headroom left, so it never passes the
ceiling; it leaks slowly. Soak never spreads: the Dead read it on that tile only, added to the human
scent there. Object emitters are soak: a bed you slept in gives off its soak and fades over days, with
no separate timer. Calibration (per game second, D-014): camping on one floor tile holds about 30% after 10 days;
10 days of house life with a bed, table, sofa, bath and desk reach 75-82% on those tiles; camping in the
bed reaches 95%; half-life about 5 days; furniture soaks about 37 times faster than floor. The ceiling
leaves headroom for other humans. Why: dens and used objects should read as such, and a value per tile
is nearly free. Consequence: walls keep absorbing; the far layer returns to a single decay, so D-008's
IIR solve stands. PEO-038 builds soak. Soak rates and rot per body are the balance sliders: new emitters and emission values are tuned against them rather than by re-deriving the field.
