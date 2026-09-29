# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Older decisions (settled infrastructure, D-007, superseded D-010, N014-N015) live in
`decisions-archive/2026-09.md`. They still hold.

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

## 2026-09-29 · D-014 · A walking step is six seconds of game time

User decision, amended by D-015: this is a walking step's duration, not the size of a turn. Ten
walking steps make a game minute. The live scent field keeps its spread per game second. Soak is
calibrated per second to D-013's targets: charge about 8.5e-7 of the headroom a second on floor, 37
times that on furniture, leak time constant 7 days (604,800 s).

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

## 2026-09-29 · D-012 · Rot masks human scent

User decision. The Dead and corpses give off rot, a second, hidden scent. It only masks human scent,
1:1: a Dead's draw from a cell is max(0, human + soak - rot). Rot is not a field: it is a count of the
Dead and corpses in each 6x6 section, eased toward count times a per-body amount (14 by default), and a
Dead subtracts its section's rot, one extra operation per Dead. Why: a crowd outside a building should
dull the pull of a faint trail, not chase it as one; a field of rot mixed into human scent broke
hunting at every strength tried. Probe: about 10 us a turn for 5,000 Dead on 200x120; 4x4, 6x6 and 8x8
give the same behaviour. Consequence: PEO-039 builds the rot layer beside PEO-009.

## 2026-09-29 · D-011 · Suite budget 1 s; the player moves at most 3 steps a second

User decision. The headless suite's budget rises from 500 ms to 1 s (AGENTS.md, `docs/testing.md`):
the golden and reach tests outgrew it. Movement input is capped at 3 steps a second; presses faster
than that are dropped, not queued, so holding a key never builds up lag. Why: turn pacing should stay
readable. Consequence: the cap is a named tunable in the frontend (PEO-037); PEO-033 enforces 1 s.

## 2026-09-29 · The Architect merges its own pull requests

User decision. The Architect opens a pull request per unit of work, drives it green and merges it
once CI passes; the user is no longer the merge gate and spends their attention on gameplay and
direction instead. Why: waiting on a merge was the bottleneck. Consequence: the Architect's own
verification is the only gate, so a check must never share a code path with the thing it checks —
both defects caught that way this session, an unsatisfiable acceptance test and five silent
truncation paths in the brief parser, would otherwise have reached `main`. Gameplay, abstraction,
direction and big-rewrite calls still stop in `DECISIONS_NEEDED.md`. Merges are merge commits, as on
`main` already.

## 2026-09-29 · D-009 · Briefs live in their own files

User decision: A. A brief is now `docs/production/briefs/PEO-xxx.md`, exactly as a handoff is
`handoffs/<id>.md`; `WORK_QUEUE.json` keeps only id, order, type, title, status, severity, effort,
owner, notes, references and depends_on. Why: the queue hit its 4000-word cap the same day it was
raised from 2500, because a brief the Builder can finish without asking runs 300-500 words and 28 of
them do not fit one capped file — trimming briefs to satisfy a cap means briefing worse. Consequence:
`work_queue.py` reads and writes the brief files, so `show` and `brief` behave as before; the cap
returns to a level the metadata can actually hold; brief files are uncapped, like handoffs and the
design docs.

## 2026-09-29 · D-008 · Ship the scent retune, then build the two-layer field

User decision: C. Land PEO-026's retune first so the behaviour can be seen in game, then build the
two-layer field as its own item. The near layer keeps a small lambda and is stepped every turn for the
cloud around the player; the far layer is solved directly with a separable IIR every 8-16 turns for
scent pooled where the player lingered. Why: the Dead must never head off a moving player, and today
that only holds because the solver is too slow to project scent ahead — the near layer's lambda turns
it into a setting rather than an artefact. The staggered solve also amortises to roughly 0.02 ms per
turn, under the 0.134 ms a single FTCS sweep costs now, while being converged instead of perpetually
lagging. Consequence: the field stays linear, so D-002's speculate/commit patch survives; the
recurrence, timings and caveats are in `docs/design/scent-performance.md`; the doorway scenario (enter
a building, linger, return to your entry point and find the Dead there) is the acceptance test for the
second item.

## 2026-09-29 · Scent drifts into walls and dies there

User decision. A wall receives scent exactly as an open cell does but passes none on, so the value is
lost; the map edge behaves the same. Why: scent should not pile up against a wall — in reality it
vents upward — and the Dead navigate by scent alone, treating walls as impassable tiles, so the field
never has to model a path around one. Consequence: `ScentField::step` stops reflecting; `out` becomes
unconditional and only `in` tests the mask, dropping a branch from the inner loop. Mass is no longer
conserved, so the two tests asserting conservation state the new contract instead. Reach shortens, so
PEO-026's tuning and its measured test parameters must both be re-derived after this lands.
