# 6 · The Dead

Thousands of cheap units following one weighted rule. Decisions: D-031 (slots), D-037
(bake the draw per cell), D-038 B (surer on a trail as scent strengthens), D-045 (the far
Dead's lean stays subtle), D-043 (the player's tile). Code: `dead.hpp`, `desire.hpp`,
`src/core/src/{dead,desire,world}.cpp`. Design: `docs/design/scent-mobs.md` round 3.

## One unit

```cpp
struct Dead {                 // dead.hpp:21, 12 bytes (static_assert)
    Vec2i pos;
    std::uint16_t step_substeps;   // how long one move takes: 12, 24 or 36
    std::uint16_t fate;            // attrition threshold, see Saves and revisits
};
```

Deliberately tiny; struct-of-arrays is a later option if the profiler asks. The only state
today is **calm** (`DeadState::Calm`); alerted and following come with PEO-010. Code that
counts states loops over `kDeadStateNames`, so a new state shows up there without changes.

## Spawning

`World::load_stage` (`world.cpp:134`): count = `initial_dead`, clamped to the open interior
cells other than the player's entry. Positions come from the stage's spawn `Rng`
(stage seed XOR `0xABCD`), redrawn until a free open cell, so at most one per tile. Each draws
a speed 1 to 3 uniformly (`kMinDeadSpeed`, `kMaxDeadSpeed`, `world.cpp:24`) stored as speed x
`update_period`. Then fates are drawn from a separate hash, so adding fates moved no other
draw. The game sets `initial_dead` from the stage area: 40 per 80x45 cells, so 266 on the
default 200x120 stage (`src/app/main.cpp:133`).

## Who moves when

See [Time and turns](04-time-and-turns.md#the-deads-slots): a poll per 9-slot cycle hands
each unit 0 to 2 slots; at a slot it decides; the move lands one slot later; a unit with a
move pending skips its slots.

## The weighted draw

At its slot a calm unit makes **one weighted draw over nine choices**: its eight neighbours
and staying (`decide_move`, `dead.cpp:45`). The weights come from the **desire field**, which
bakes every term that reads only the last update into nine log-odds per cell, once per
update (D-037), so a decision is a table lookup and a draw.

### Building the desire field (`desire.cpp:106`, `build_rows`)

For a cell with scent `here` (its `sample`), for each open move d to neighbour n:

```
lean  = lean_edge + (lean_full - lean_edge) x here / strength
e(d)  = lean x (scent(n) - here) / distance_cost                      (scent term)
      + company_gain x crowd(box d) / (2r + 1)^2                       (company term)
e(d)  clamped to [-127, 127]
e(stay) = stay
closed moves (wall, off map, cut corner) = kNoChoice
```

- Log-odds are in **sixteenths of an octave** (`kOctave = 16`): +16 doubles a weight.
- `crowd(box d)` is the number of Dead in a (2r+1)x(2r+1) box centred r+1 cells out in
  direction d, read from a summed-area table of the occupancy snapshot at the update.
  Default r = 3, so a 7x7 box centred 4 cells away.
- **Flat cells** (no scent on the cell or any open neighbour, and no company) skip the maths:
  every open choice is 0. That is most of a stage.
- Division is by a 16-bit reciprocal, so it rounds toward minus infinity; everything is
  integer.

### Turning log-odds into weights (`weight_of`, `desire.cpp:26`)

```
weight(e) = 65536 x 2^(e / 16)   from a 16-entry table, 0 for kNoChoice
```

A level neighbour weighs 65,536; e = +127 about 16 million; e = -127 about 267.

### The draw (`draw_choice`, `dead.cpp:27`)

`word = hash(stage seed XOR 0xD8A3, slot, unit index)`; `pick = word mod total weight`;
walk the nine weights in order (N, NE, E, SE, S, SW, W, NW, stay). All zero means stay.

### Then the crowd rule

If the drawn tile has a Dead on it, or a pending move reserved it, the unit **stays and does
not draw again** (D-031). A calm Dead never sidesteps and never climbs over another, so there
is at most one per tile. Shared occupancy, trips and trample are designed (ADR-0013,
PEO-022, PEO-011) but not built.

### What the numbers mean in play

With defaults (`lean_edge = 0`, `lean_full = 48`, `stay = 0`, `company_gain = 4`, r = 3):

| Where the unit stands | Lean | Neighbour one cell up a fresh trail (+16 scent) |
|---|---|---|
| On full-strength scent (960) | 48 | e = 96, weight x64 against a level neighbour |
| Half strength (480) | 24 | e = 48, x8 |
| Edge of reach (near 0) | 0 | e = 0, level: a free wander, 1 in 9 to stay |

A neighbour one `distance_cost` higher (+8) counts half as much: e = 48 at full strength, x8.
Company is a mild pull: a box packed with 49 Dead adds +4, about x1.19. It was made weak on
purpose: per Dead it was far too strong and a packed crowd never fanned out (`desire.hpp:36`).

So near a fresh trail the Dead climb almost every step; far out they wander with a lean. That
gradient is D-038 B, and D-045 keeps the far lean subtle.

## The swarm scenarios and the siege suite

`tests/core/test_swarm.cpp` (suite `scenario: swarm`) pins the behaviour with metrics printed
as `MESSAGE`, so a knob change shows as a number:

- **sure-footed gradient**: the climbing share near the source beats the share far out.
- **swarm-accumulation**: the Dead gather where scent is stronger.
- **fan-out**: a tight cluster on flat scent spreads, never two to a tile.

`peo_siege` (`tests/siege/`) is the long check: the horde round three buildings in several
setups for 48 game hours, with a committed baseline report per building. Run it after any
change to these numbers ([Build, run and test](02-build-run-test.md#on-demand-tools)).

## Levers

| Lever | Where | Default | Effect |
|---|---|---|---|
| `lean_full` | `DeadDrawParams` (`desire.hpp:33`) | 48 (3 octaves) | How sure the Dead are on strong scent |
| `lean_edge` | `DeadDrawParams` | 0 | How much the far Dead lean toward scent (D-045: subtle) |
| `stay` | `DeadDrawParams` | 0 | Log-odds of staying against a level neighbour; up makes them idle more |
| `company_gain` | `DeadDrawParams` | 4 (a quarter octave) | Pull toward other Dead |
| `company_radius` | `DeadDrawParams` | 3 | Size and distance of the company box |
| `initial_dead` | `WorldParams` | 40 core; area-scaled in game | Horde size |
| speed range | `kMinDeadSpeed`, `kMaxDeadSpeed` (`world.cpp:24`) | 1 to 3 | Slow versus fast Dead |
| `dead_cycle` | `WorldParams` | 18 substeps | Cycle length; changes every slot draw |
| `parallel_decide_min` | `WorldParams` | 16,384 | Batch size from which decisions split across threads (performance only) |

## Where the Dead are going

Triggers, sound events and speeds (PEO-010), trips, contests and trample (PEO-011, PEO-022),
crowd pressure on barricades (PEO-084), whether a Dead may step onto the player (PEO-102),
death and rising as a Dead that can see (PEO-021), population aggregates beyond the detailed
radius, and the performance levers (PEO-089 rebuild only changing desire cells, PEO-074,
PEO-100).
