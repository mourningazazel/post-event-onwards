# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007 to D-032, D-034, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`; D-033, D-035 to D-039, N023 and later ones as they age out, in `decisions-archive/2026-10.md`. They still hold.

## 2026-10-13 · D-054 · One reference path for everything; children inherit

Owner decision (PEO-116 thread, answers to Q1-Q7). (1) Flat IDs stay, and every record is
also addressable by its full path from its root (`item.tool.hammer.claw_hammer`), derived from
the parent chain; both forms are accepted. (2) A path follows references in one line:
`item.tool.hammer.claw_hammer.composition.handle.material.hardness` steps into the material.
(3) Children inherit: shared data lives on the generic parent and a child sets only what it
changes, so the add-nothing rule of PEO-116 is retired. (4) Abstract category roots: every
record falls under a generic root (`item.*`, `unit.*`, with `unit.self.*` the player, and
roots for world generation). (5) Units are content records that work like items. (6) The
reference, query and effect syntax is a proposal the owner and Architect settle together
(PEO-134). Consequences: PEO-130 becomes the lean-inheritance and roots pass; PEO-132 (paths),
PEO-133 (units) and PEO-134 (syntax) are queued; PEO-092 and PEO-121 keep flat IDs and are
unaffected.

## 2026-10-13 · N024 · Any tileset, not only the Dwarf Fortress grid

Owner direction (N024). The DF grid stays supported, but it is not the only way to play: ADR-0015
point 3 now reads as the default layout, not the required one. With the GUI configuration work, a
small tool grids any PNG (any columns, rows and cell size), names cells with system-agnostic game
IDs, and saves a shareable ini; unknown IDs are ignored, missing ones use the game's defaults.
Consequence: deferred PEO-131, which lists what ties the frontend to the DF grid today.

## 2026-10-12 · Item parent chains have no length limit; a save reads up to 512 ancestors

Owner decision. Content rejects inheritance cycles, so every parent chain ends at a root and no
fixed depth is needed: PEO-008 drops `kMaxItemAncestry` and stores each item's chain at load
(`Content::ancestry`). A save is untrusted input, so PEO-092 caps what one CDIC entry may claim
at `kMaxSavedAncestry = 512` (u16 count): far beyond any real family, and the data is tiny.

## 2026-10-12 · toml++, vendored

`third_party/toml++/toml.hpp` is committed (PEO-008), with its LICENSE. Why: no network
during configure, as doctest. Consequence: built with `TOML_EXCEPTIONS=0`, included only from
`src/core/src/content.cpp`; upgrade by replacing the header and noting the version here
(currently 3.4.0).

## 2026-10-08 · D-051 to D-053 · Seen terrain stays dimmed, Look pans, nothing named past 20 ft

User decisions (sight batch thread). D-051 A: cells once seen keep walls and floor drawn dimmed,
with no Dead, items or scent until seen again; a per-stage seen grid in core, saved with the stage.
D-052 B: the Look cursor reaches any seen cell; the view scrolls with it and snaps back to the
player when Look ends. D-053 A: past 20 ft nothing is named; walk closer (or binoculars, later).
Consequence: PEO-120 builds answer B, PEO-121 builds nothing beyond 6 cells, and map memory is a
new queue item after PEO-119.

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
