# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007 to D-020, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`. They still hold.

## 2026-09-30 · D-034 · Horde size and active map size are set per machine, continuously

User decision (N019), extending D-021. Beside the M1 Air, one target PC per tier: low (i5-8400
class, GTX 1080), medium (i5-12400F or Ryzen 5 5600X, RTX 3060/4060), high (i7-14700K, RTX
4070 Super). Horde size and active map size are two separate limits (latency and core speed;
cache and bandwidth). An auto-config calibrates each axis on the machine and sets both as real
numbers inside D-021's budget; no presets, and a PC may run hordes past what fits on the Mac.
Why: the game is gradients, and the horde should grow as far as a typical PC allows.
Consequence: design/performance-targets.md; PEO-070 to PEO-074; the benchmarks stay outside the
unit suite.

## 2026-09-30 · D-032 · Quick taps are kept; a held key stays capped

User decision (C), amending D-011. Each distinct press of a direction key is kept, up to 3 waiting
(one second's worth at 3 steps a second), and played out at the cap. A held key's auto-repeat stays
capped at 3 a second, is never queued, and stops the moment the key is released. Why: playtesting
lost quick taps under D-011's drop rule; a bounded tap queue keeps them without letting a hold build
lag. Ryan will judge the feel in the next playtest. Consequence: PEO-065; the queue depth is a
tunable.

## 2026-09-30 · D-031 · The Dead move in staggered slots over a 9 s cycle

User decision, amending D-022 and refining ADR-0013. Every 9 game seconds the world polls which
of the Dead can move and gives each mover a slot (a second within the cycle) from a hash of
(seed, cycle, index), so who moves when differs every cycle. At its slot a unit re-checks its
target and decides; the move lands one slot later. A tile a unit leaves stays blocked until then,
so the next unit in line cannot step into it. A calm Dead whose chosen tile holds another Dead,
or one about to leave it, does not move and does not pick another direction; only an alerted
Dead pushes past, which is where ADR-0013's shared tiles, trips and trampling apply. Speed is
slots per cycle: 9 / step_seconds, the integer part guaranteed and the fraction a hashed chance
(D-022's rule, per cycle). Why: 9 s against the 6 s scent update keeps moves out of step with the
player; the one-slot delay makes a crowd fan out and file through, and jams a one-wide hallway.
Consequence: each Dead decides once per cycle, not once per update; replaces PEO-042's burst of
steps at each update (held). Between updates the Dead read only the last update's scent, so they
run live and exact; speculation keeps only the scent sweep. Calm Dead never share a tile. PEO-058.

## 2026-09-30 · D-030 · Every container has a purpose and three tags

User decision (N018), refining D-027. A compartment's purpose is rolled from its seed before its
contents: it picks the main loot table and names the compartment once examined ("cutlery drawer").
Each container carries three tags. Kind (the purpose) and context (building type and room: a
maintenance cabinet holds paper and pens in an office, store supplies in a shop) link to "may
randomly appear" tables of rare, useful, dangerous or negative extras. Detail (a rare modifier: a
hobby, a smoker, a trace) replaces items instead: its own table is blended into the main fill, one
pick from each in turn, so the container shows clear evidence of it. Nested containers inherit
context. Why: predictable contents; unique finds placed precisely. Consequence: PEO-057 authors it;
PEO-053 generates purpose, then the main fill, then extras, all from the compartment seed.

## 2026-09-30 · D-029 · A stack's quality is a mean plus a spread

User decision: B (N017). A line holds a mean quality and a spread; an item taken from it samples
its quality from them with a counter-based hash of its address. Consequence: PEO-013.

## 2026-09-30 · D-028 · Soak travels with portable absorbers

User decision: B (N017). A portable absorber (mattress, cushion, clothing) keeps its soak as a value
on the thing and takes it along when moved; floors and fixed furniture keep soak on the tile.
Consequence: PEO-038 reads tile soak plus the soak of absorbers on the tile; PEO-055 derives rates.

## 2026-09-30 · D-027 · Each compartment is searched on its own

User decision: B (N017). A drawer or shelf costs its own search time, from its volume. The tedium is
intended; the menus must make it quick to move through, so only game time is slow. Consequence:
contents are generated per compartment (PEO-053).

## 2026-09-30 · D-026 · A quantity is one kind; an item taken out gains its detail

User decision, the owner's own rule (N017). A line in a container is one kind ("box of nails"). An
item taken out instantiates with its details at that moment; put back, it lists as its own entry
beside the nameless quantity, never merged. Entries get no unique names; an unexamined item is
marked visually and gains details when examined, removed or modified. Consequence: amends D3.2's
merge rule for instantiated items; PEO-050, PEO-053.

## 2026-09-30 · D-025 · Rot scales the pull down, never zeroes it

User decision: A (N017), amending D-012. In the integer field (D-024), whose values are log
strength, a Dead subtracts rot times a scale from what it reads, which divides the linear
pull. Why: a 1:1 subtraction zeroed every cell past about 12 cells. Consequence: PEO-039.

## 2026-09-30 · D-024 · Scent is an integer geodesic field that propagates

User decision: A (N017), superseding D-008's two layers and D-020's inner-wall buildup. A cell holds
strength − C·distance − K·age along walkable routes as an int32. The front advances `speed` cells
per update and up to `gust` more downwind; step costs fall downwind and rise upwind, scaled by a
per-cell openness that is 0 indoors. Stairs are routes; walls and closed buildings absorb; work is
bounded by a reach radius around the player. Commit patches by min-plus from the logged tiles.
Why: diffusion reached 18% of a town map and parked the Dead at crossings, and the IIR layer
cannot see through doors (pathfinding review). Consequence: PEO-035 then PEO-030; PEO-047 to 049.

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
