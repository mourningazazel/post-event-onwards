# Performance bottlenecks

The few passes that cost the most per update. Read before briefing, building or
reviewing work that runs per update, per second or per Dead, and add new ones here.
Capped at 900 words: a register, not a report.

## What counts

A bottleneck is one pass, or a small group of passes, that costs clearly more than
the rest: at least 20% of a speculated update, or 10% of commit, in any measured case.
The usual shapes are a pass over every stage cell, a loop over every Dead, a poll that
walks a large set in sequence, and work redone on the same objects when little changed.
A pass that grows with the stage or the whole horde, when it could grow with what
changed, is one in waiting even while small.

## Register

Cloud runner (Xeon 2.1 GHz, about 1.4x slower than the M1), `headless-release`, one
thread, median of 36 updates at steady state with 6 standing emitters per 1000 tiles.
Measured 2026-10-02 on `0fd2ffd` by timing `ScentWave::update` and `DesireField::build`
alone, then the whole `speculate`.

| | Pass | Cost | Grows with | Levers |
|---|---|---|---|---|
| B1 | **Desire field build** (`desire.cpp:66`): nine log-odds for every stage cell, every update, including the flat ones | 7.9 of 14.1 ms at 512² with 50k Dead (56%); 30 of 36 ms at 1024²; 123 of 149 ms at 2048², over D-021's 100 ms by itself. 30 ns per cell, 23 ns even when flat | stage area | Rebuild only cells within reach of scent or company and keep flat cells as built (PEO-089); threads already split it |
| B2 | **The Dead's slots** (`world.cpp:297` `dead_slot`, `poll` at `:55`): every unit draws at its slots since PEO-009, with scattered reads of desire, occupancy and reservations | 90 to 110 ns per Dead per update: 5.2 ms at 50k, 9.3 ms at 100k, 24 ms at 200k (1024²). M1: 512² with 50k went 2.6 to 7.7 ms with B1 | horde | Keep each decision a lookup and a draw (D-037); intents split across threads only for batches of 16,384 or more (PEO-080); PEO-074 |
| B3 | **Scent update** (`scent_wave.cpp:407`): the pull over active tiles | 4 ns per active cell: 1.1 ms at 512², 4.1 ms at 1024², 15 ms at 2048². Wind doubles it (2.1 ms at 512²). A standing emitter keeps its whole reach active | active area | Reach radius (PEO-047), standing emitters as a static layer (PEO-074). GPU: windy stages of 512² up run there (PEO-087; M1 with gust 2: 0.9 / 2.6 / 9.8 ms at 512² / 1024² / 2048² against 1.3 / 6.6 / 28 ms on 4 workers); calm stays on the CPU, where the GPU only matches one thread (PEO-081) |
| B4 | **Speculate's copies** (`world.cpp:442`, `:432`): the horde state with its map-sized grids, and the wave sync | 0.2 to 0.4 ms at 512², 1.6 ms at 1024² | stage, horde | Copy only dirty cells (PEO-074) |

Commit (`world.cpp:471`) is not one: under 0.35 ms at 50k Dead against its 1 ms budget.
It is the input path, so any work added to it is suspect.

**Multipliers.** Time passing multiplies every row: one slept game hour is 1200 updates (D-039, ADR-0016),
about 17 s here at 512² with 50k (PEO-082). Stages of 1024² to 2048² (D-036)
multiply every per-cell row by 4 to 16.

## Rules

1. **Brief.** A brief that adds or changes per-update, per-second or per-Dead work names
   the rows it touches and its expected cost per cell or per Dead. New work grows with
   what changed (reach, active cells, moving Dead), not with the stage or the whole
   horde. A new full-stage or full-horde pass per update counts as a new row.
2. **Build.** Run `python3 tools/verify.py --perf` before and after any change to a
   listed pass or a new per-update pass, and put both in the report. A change that adds
   more than 5% to a row, or 0.5 ms at 512² with 50k Dead, carries its lever in the same
   item, or a `[perf]` queue item says why not.
3. **Review.** The Architect checks rule 2 against the numbers. A row made worse without
   a lever or an item is a rework finding.
4. **Report.** Whoever measures or profiles a new bottleneck adds a row with its case,
   machine, numbers and `file:line`, and records a `[perf]` item. When a lever lands,
   update the row with its new numbers, or drop it once it falls under the threshold.
5. **Measure.** Measurements, never estimates. Cloud numbers compare only before and
   after on one runner; M1 figures come from the Builder. The `perf:` lines report
   best-of-10, an early cheap field; prefer steady-state medians (PEO-070).
