# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

Settled infrastructure decisions, now encoded in the build and tooling, live in
`decisions-archive/2026-09.md`. They still hold.

## 2026-09-29 · D-010 · Scent builds up where the player lingers and fades slower there

User decision, amending D-007 and D-008. Pooled scent, at a den or a path walked often, fades more
slowly and pulls the Dead harder than a fresh trail. A cell's fade rate falls as the total of the
sliding 3x3 box around it rises, to a nonzero minimum. Box totals are a reference map that neither
spreads nor decays, refreshed on a turn schedule: every turn near the player, staggered further out.
Why: dens and worn entryways should read as such to the Dead. Consequence: D-007's ban on
value-dependent decay is lifted for this rule only. Fade rates are fixed between refreshes, so `step`
stays linear and speculate/commit survives. A sliding box, not tiles, avoids seams that leave false
peaks. It lives in PEO-030's far layer, whose single-decay IIR solve must be re-examined.

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

## 2026-09-28 · D-007 · Scent carries far on a 0–500 scale; the Dead are drawn by a power law

User decision. The player deposits at a nominal full strength of 500 rather than 1, the field
spreads much further, and the Dead weight a candidate cell by its scent raised to a tunable
exponent, so a stronger cell pulls disproportionately harder while a wide, flat far field still
yields directional drift. Why: the ~8-cell reach in the PEO-001 playtest is the `floor` flush —
past it every cell is exactly `0.0F`, `strongest_neighbour` finds nothing strictly greater, and
distant Dead freeze for good; reach goes as `λ · ln(peak/floor)`, so a wide dynamic range over a
tiny floor is what carries a gradient across the map. Consequence: `floor` drops by orders of
magnitude and the 500 scale supplies the headroom (PEO-026); the field stays a linear operator so
D-002's `speculate/commit` patch survives — every nonlinearity lives in the read path, as
`pow(scent, k)` in the weighted draw, never `exp`, which saturates at these magnitudes (PEO-009);
storage stays `float`, because quantising cells to integers 0–500 would recreate the plateau that
froze the Dead; if long reach and fresh trails ever conflict, the sanctioned fix is two linear
layers (fast/near, slow/far) summed at read time, not value-dependent decay.

## 2026-09-28 · N015 · D-003 to D-006 answered; locks; books

User decisions, verbatim in `docs/design/notes/N015-*.md`. D-003 A: every G01–G03
recommendation accepted (walls are terrain, player is an ordinary creature, the Dead are the
same kind at a compact tier, template + components, generation addresses, on-demand hidden
contents, reference + delta instances, physical inventory, volume + weight + dimension
capacity, stacks, per-part condition). D-004 B: persistent world; the dead player rises as a
vision-tracking Dead with its stats at the start unless decapitated or brain-destroyed; new
character 1–2 miles away. D-005: real physical stats, real-world skills; books generated in
thousands with difficulty and read time, at most one point once, gates both ends. D-006: 1 m
tiles, one storey per z, shared occupancy with trip, knock-out and trample (ADR-0013). Locks:
tier bands by lock kind, appearance weighted by building, container and security markers.
Consequence: `teaches` is now {skill, difficulty, read_time_min, gain_chance_pct, window};
occupancy is a count per tile.

## 2026-09-28 · N014 · The Dead's AI, content depth, skills 1–100, Mood and Sanity, drugs

User direction, recorded verbatim in `docs/design/notes/N014-*.md` and worked into
`scent-mobs.md` round 3, `survival-actions.md` §2.15–2.16, `content-model.md` §10–11,
`world-catalog.md` §10, G04, ADR-0012 and `purposes.md`. Headlines: one weighted-draw movement
rule with aggregate and company pull replaces the recruitment cascade; triggers steer by
general direction and expire displaced; sound events are kind + dB; trips cascade and trample
hurts; alerted ×1.5, following ×2; skills are 1–100 with percentages and failure outcomes;
every item has a quality and a "broken" text; building census and category retail; Mood and
Sanity are main mechanics; drugs and explosives exist; maps are rare and mark general
locations; crows are text. Consequence: the registry skill scale changed to 1–100
(`content/registry/skills.toml`, `actions.toml` use `unlock`/`difficulty`).

## 2026-09-28 · The design corpus from the feature branches is the design of record

The `content/survival-catalog` branch (GAMEPLAN, SYSTEMS, ADR-0001–0011, gameplay model,
notes N001–N013, content model, survival actions, world catalogue, purposes, playtest harness,
403 authored archetypes) is merged. ADRs stay the record of architecture decisions; this file
stays the rolling log and points at them. `SYSTEMS.md` is the target map, reached one system at
a time through the queue, not by a framework-first rewrite (architecture.md).

## 2026-09-28 · D-002 · Turn-based, with the world computed while waiting

User decision. The world advances only when the player acts (a step or a
wait is a turn); it never runs on a clock. Modern twist: the idle time
between turns is used to compute ahead. The player's action changes only a
small neighbourhood, so the next turn is computed speculatively while
waiting and patched on input. Why: readable tactical play with no waiting
on the simulation, and the 5,000-Dead budget spent when the player is not
looking. Consequence: `World::step(action)` is the only way time moves; a
`speculate / commit` pair must produce bit-identical state to `step`
(golden test); rendering reads the last committed turn, never a
speculation buffer.
