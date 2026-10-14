# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007 to D-032, D-034, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`; D-033, D-035 to D-044, N023 and later ones as they age out, in `decisions-archive/2026-10.md`. They still hold.

## 2026-10-14 · D-060 · Real-size tests get their own tier

User decision: A (review thread). A third tier, `realsize*` suites, runs in release builds only
with its own 60 s budget, from `verify.py --full` and CI's release job; the unit and scenario
tiers keep 1 s and 5 s (D-033). Why: one update at 1024x1024 costs about 60 ms, so a real-size
walk cannot fit the scenario budget. Consequence: PEO-145 builds the tier and its first cases.

## 2026-10-14 · D-059 · One continuous map, tested at real size

Owner direction (review thread). The map is one continuous world the player walks with no
barrier between places; the stages with an exit are a test placeholder (ADR-0007, D-041).
From now on, work is proven at real map size, not on small staged areas: scenario tests, perf
and manual steps use a map of at least 1024x1024 cells with Dead at play density (the planning
size in performance-targets.md). Small fixtures stay only for unit tests that pin one rule. A
real-size case over the scenario budget under the sanitizers runs in release builds only.
Consequence: PEO-118 is briefed now; briefs and reviews check map size.

## 2026-10-13 · D-058 · Fields and actions can carry handlers

Owner design (PEO-116 thread). A number field may be a block with `value` and `on.<operator>`
handlers (`on.reduce`, `on.get`, ...), and a record may hook actions done with it
(`on.use.fire`). Handlers are effect lists using `$input`, `$value`, `output()` (only inside
`on.`), `if (...) { ... }` with a closing `else = { ... }`, `+=`-style shorthands and `&name = value` arguments that override
the called record for that call. Spec: docs/design/content-format.md section 10. Consequence:
PEO-132 runs handlers; PEO-135 already captures them as expressions.

## 2026-10-13 · D-057 · Content moves from TOML to our own format

Owner decision (PEO-116 thread). Content is written in the path-and-brace syntax itself
(docs/design/content-format.md): a record is its full path then a block; commas go only between
fields and list entries (owner, 2026-10-13: none before a closing bracket); references are full paths only (short IDs leave the files; saves keep them); `"..."` is
verbatim, `"""..."""` spans lines and reads escapes, `<b>`/`<i>` and `${...}`; prose lives in
`text.*` records and may be generated; numbers carry units, lengths metric only; scales are
integers, never words; `dims { l, w, h }`; shared umbrella roots. Supersedes D-054 point 1 and
D-056's triple-quoted queries. Consequence: PEO-135 builds it before PEO-130 and PEO-132.

## 2026-10-13 · D-056 · Queries are laid out one condition per line

Owner request. Inside a query's braces, line breaks and indentation are ignored, so filters are
written one condition per line, indented per nesting level, nested braces opening their own
block; a multi-line query is a TOML triple-quoted string. Short ones may stay on one line.
Consequence: PEO-132's parser treats whitespace inside braces as insignificant.

## 2026-10-13 · D-055 · Reference syntax settled: materials are items, calls end a path

Owner answers to PEO-134 (docs/design/reference-syntax.md section 6). A material is an item
(`item.material.wood.oak`, a hunk of it); substances keep their own root. Things in play are
reached through `self`, `$role`, `here`, `cell(x, y)`. Effects are calls at the end of a path
(`$target.mind.sanity.reduce(25)`), one form for every attribute. Filters use braces and `~`.
Paths stay strings in TOML: today's 930 KB loads in about 30 ms, so a 15 s startup budget holds
hundreds of times more. Consequence: PEO-130 moves materials under `item.material`; PEO-132 is
briefed on this syntax.

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
