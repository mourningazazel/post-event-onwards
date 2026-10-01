# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007 to D-023, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`. They still hold.

## 2026-10-01 · D-037 · Richer Dead and crowd pressure are the first scale-up

User decision: C (N020). Of the things threads and the GPU open up, the Dead's weighted draw
(PEO-009) and crowd pressure come first. Why: hordes are a pillar, and PEO-009 is the next
mechanic. Consequence: PEO-009 bakes every term that reads only the last update into a per-cell
desire field (research/parallel-and-gpu.md section 6); crowd pressure is PEO-084.

## 2026-10-01 · D-036 · GPU compute starts now, to go bigger

User decision: C (N020), superseding D-021's "no GPU work"; D-021's budget stands. The intent is
the biggest world the hardware allows. Three goals, and the GPU earns its place if it delivers
one: stages of 1024x1024 to 2048x2048 as the norm, so the Dead flow across a city instead of
spawning at a border; scent that reaches far and builds up, instead of aggregates adjusting
spawns; and fast time passing, because players sleep and pass time often. Every GPU kernel is
integer, has a CPU reference, and an equivalence test checks them bit for bit (SYSTEMS R11).
Consequence: ADR-0014; PEO-081 to PEO-083; stage size stays per machine (D-034).

## 2026-10-01 · D-035 · Core uses threads it does not own

User decision: B (N020), amending ADR-0012's "core spawns no threads". The frontend passes core a
parallel-for; core still creates no thread. Tests run it serially, and a golden test checks
serial and threaded runs give the same world bit for bit, beside speculate/commit's. Why: the
scent field, the flow grid and the Dead's intents split by tile or unit with results combined by
maximum or minimum, so threads cannot change them. Consequence: ADR-0014; PEO-080.

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
