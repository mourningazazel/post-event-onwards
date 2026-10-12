# Sight and memory

What the player can see now, what they remember, and looking at a cell. All in core, all
integer, none of it spends a turn. Decisions: D-049 (sight bounds what shows), D-051 A (seen
terrain stays, dimmed), D-052 (Look pans), D-053 (nothing named past 20 ft), D-042 (the owner
writes player-facing text).

## Sight (`sight.hpp`, `sight.cpp`)

`SightField::compute(opaque, origin, params)` marks every cell within the optical range that
walls do not hide, by **symmetric shadowcasting**: if an open cell sees another, it is seen
back, so the Dead that can see (later) can reuse the same pass. Integer slopes (fractions
compared by cross-multiplying), a disc test on `dx² + dy² ≤ r²`, no floats.

- Range: `kOpticalRangeCells = 152` (500 ft, D-049). `SightParams::range_cells` is the one
  hook binoculars will use. Capped at `kMaxSightRangeCells = 2048`.
- `opaque` is `stage.blocked`; doors, windows and glass will need a separate sight layer.
- Cost grows with the open area in range. It runs once per player move, in the frontend
  (`refresh_sight`), never per update. Buffers are reused.

## Map memory (`memory.hpp`, `memory.cpp`)

`MapMemory` is one byte per stage cell: walls and floor once seen stay known out of sight,
but nothing on them (no Dead, no exit, no scent). `World::remember(sight)` folds a sight field
in, walking only the sight window; a field computed from anywhere but the player's current
cell is ignored. The memory resets on every stage load, travels with `store_area` /
`resume_area`, and is saved (section SEEN). A second character later is simply a second
`MapMemory`.

## Look (`look.hpp`, `look.cpp`)

`LookCursor` only ever rests on a cell the player sees now, so looking can never tell more
than sight already has, and it never spends a turn. `look_at(sight, stage, horde, player,
cell)` returns a `LookReport` (data only, no words: distance squared, player, opaque, exit,
how many Dead stand there) or nothing for an unseen cell. The HUD line for it is empty on
purpose: the owner writes that text (D-042), and PEO-121 decides what can be made out at each
distance.

## Tunables

| Tunable | Where | Default |
|---|---|---|
| Optical range | `kOpticalRangeCells` (`sight.hpp`), per call `SightParams::range_cells` | 152 cells (500 ft) |
| Range ceiling | `kMaxSightRangeCells` | 2048 |
| Look jump | `kLookJumpCells` (`look.hpp`) | 8 cells |

## Designed, not built

- What can be made out at each distance, and the HUD's Look text (PEO-121, D-049, D-053).
- A sight layer separate from walls for doors, windows and glass.
- The Dead that can see (a dead player's character, PEO-021) reading the same symmetric pass.
- Binoculars through `SightParams::range_cells`.
