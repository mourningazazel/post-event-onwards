# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007 to D-032, D-034, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`; D-035 onward as they age out, in `decisions-archive/2026-10.md`. They still hold.

## 2026-10-06 · D-047 to D-050 · The screen follows the player; sight bounds what shows

User decisions (camera thread). D-047 A: the view is as many whole cells as fit the resizable
window, letterboxed when it does not divide evenly. D-048 A: it jumps one cell a step, the player
always centred. D-049 (own answer): what shows is what the player can see; optical range is about
500 ft (152 tiles), binoculars extend it later; a Look key moves a cursor over the visible field;
within 20 ft (6 tiles) signs read and items show generic labels ("hammer"), containers only their
kind; details and contents need examining. D-050: no zoom yet (zooming out shrinks tiles), but
build for it. Consequence: PEO-117; sight, Look and range labels get queue items.

## 2026-10-06 · D-046 · A removed item's stand-in is its nearest parent kind

User decision: A (decision card, PEO-092 thread). On load, an item whose ID is gone becomes the
nearest spawnable ancestor on its `parent` chain that is still present; item families gain a plain
generic archetype (`hammer`) as parent. Consequence: PEO-092 is briefed on the parent chain and
needs a content pass adding generic parents; it still waits on PEO-008 (content registry).

## 2026-10-06 · D-045 · The far Dead's lean stays subtle

User decision (review thread, A, recommended). The D-038 knobs stay as they are: at the edge of
scent reach the Dead lean only slightly toward the trail, so in play at seed 1 density the lean
is not visible by eye. The headless swarm test shows it is there (55% of moving draws climb
beyond 45 cells). Why: raised by the PEO-101 playtest; a faint pull at the edge of reach is the
intended gradient. Consequence: no tuning item; the swarm test stays the check.

## 2026-10-05 · D-044 · Usage rules: metadata lands on main, one subagent per item, review on demand

User decision (thread "Context usage audit"; every recommended option). The Architect
fast-forwards queue, brief, decision and docs-only commits straight to `main`, as the Builder
does; pull requests remain for code, tests, tooling, CI and skills, and CI's build jobs skip
pushes that change none of those. A review or brief batch is one thread that runs each item in
its own subagent and keeps one line per item. Reviews start when the owner says "review" in the
project chat, with a 12 h safety poll behind it. Thread effort follows the task: high for
reviews and L/XL briefs, medium for recording decisions, promotions and small briefs. Why:
every model call re-reads the whole session, and nine of the twelve PRs before this carried no
code. Consequence: roles.md, CLAUDE.md, the review, plan, start-work and burndown skills,
`verify.py` tails, `next --by-effort`, `docs.yml`.

## 2026-10-04 · D-043 · The player's tile is taken; a Dead tripped onto it trips the player

User decision: A, with a note. One of the Dead never steps onto the player's tile by choice: the
tile counts as taken, so the Dead stop beside the player (seen in the PEO-088 siege maps, where
one stood on the player's tile with nothing happening). "Since [the Dead] can trip over each
other and be ejected into a different space, let that interplay and have [one] fall onto the
player, also trip the player." So the trip rule (PEO-011, PEO-022) may throw a falling Dead onto
the player's tile, and that knocks the player down too. Consequence: PEO-102 builds the taken
tile now; PEO-011/022 carry the fall-onto-player trip when they are briefed.

## 2026-10-03 · D-042 · The owner writes every player-facing line

User decision: B. "I don't want you doing the more creative work that i can do myself." Agents
author data only (properties, tags, numbers, structure) and write no new descriptions, flavour
text, notes or lore. Existing agent-written `desc` and flavour strings stay as placeholders
until the owner replaces them. The font, for now, is a free-use one: SDL3's built-in debug font
(Zlib), later the owner's own. Later the owner will build a small terminal GUI to author the
game's text and much of the later game; the content format should stay easy for a tool to
read and write. Consequence: ADR-0015 point 2; content-model §4; PEO-008.

## 2026-10-03 · D-041 · The world is endless, in 100 km geography squares

User decision: A. Neighbouring squares generate whole when the player nears an edge, with coast
and ranges continuous across them (ADR-0018, ADR-0007). Consequence: PEO-095.

## 2026-10-03 · D-040 · Revisits have no gate

User decision: A ("if that is easily accomplishable"; it is simpler than a gate, since the
catch-up is closed form). Any area out of the active range catches up when reloaded, so a day
away changes a little and a month a lot. Replaces N009's 30-day gate. Consequence: ADR-0019;
PEO-094.

## 2026-10-03 · D-039 · Long actions cost twice the updates under 3 s turns

User decision: A. Every turn behaves as today; sleeping and waiting keep their hours, so a time
skip runs twice today's updates per game hour, and fast time passing (PEO-082, threads and the
GPU) absorbs it. Consequence: ADR-0016; PEO-090.

## 2026-10-03 · N023 · Owner's documentation audit

User direction (N023). Dependencies must be MIT-compatible, and every asset is original; the
owner supplies art, the default tileset, detailed descriptions and lore (ADR-0015). A walking
turn becomes 12 substeps and reads as 3 game seconds, with today's behaviour on the even
substeps (ADR-0016, amending D-014, D-015, D-031). Saves keep two fallback copies, migrate
content to generic stand-ins or junk, and tag each section's sync scope for a future host and
clients (ADR-0017). Geography is generated first in 100 km squares with lakes and sea, and
towns fit it; most towns have no river (ADR-0018). Revisited areas catch up lazily, the horde
thinning and player-made things aging, with no global day pass (ADR-0019). Docs are loaded by
domain (`docs/domains/`). D-039 to D-042 answered below.

## 2026-10-01 · D-038 · The Dead are surer on a trail as the scent gets stronger

User decision: B ("good job on keeping to the gradient mindset"). The Dead's step is a weighted
draw over the neighbours and staying; how sharply it favours climbing scent grows with the
scent's strength, set by two knobs (sharpness at the edge of reach and at full strength) with an
integer gradient between. Near a fresh trail they climb almost every step; at the edge of reach
they wander with a lean. Why: the geodesic field (D-024) drops by the same step every cell, so a
fixed preference would make them as sure at the edge as at the source. Consequence: PEO-009.

## 2026-10-01 · D-033 · Two test tiers: unit under 1 s, scenarios under 5 s

User decision: B. Unit tests keep the 1 s budget so the edit-test loop stays fast. Town, scenario
and long golden-run tests form a second tier, the doctest suites named `scenario*`, run in the same
CI build under their own 5 s budget; each tier is enforced by its own CTest. Why: the town
scenarios keep growing and pushed one shared budget red twice. Consequence: PEO-066; a case over
about 50 ms belongs in the scenario tier rather than being skipped.

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
