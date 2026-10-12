# 4 · Time and turns

The world is turn-based (D-002): nothing moves until the player spends an action. Inside a
turn, though, time is fine-grained, so that walking, running, waiting and later sleeping or
crafting all cost honest amounts of game time. Decisions: D-015, D-031, D-032, D-039;
ADR-0005, ADR-0012, ADR-0016.

## Units of time

All in `src/core/include/peo/core/types.hpp`.

| Unit | Size | Constant |
|---|---|---|
| substep | 250 game ms | `kSubstepMs = 250` (`types.hpp:22`) |
| slot | 2 substeps (0.5 s): when the Dead may act | `kSlotSubsteps = 2` |
| walking step, wait | 12 substeps (3 s) | `kSubstepsPerWalk = 12`, `kStepSubsteps`, `kWaitSubsteps` (`action.hpp:15`) |
| running step | 6 substeps (1.5 s) | `kRunStepSubsteps` (`action.hpp:18`) |
| update | 12 substeps: one scent update and desire rebuild | `WorldParams::update_period` (`kUpdatePeriodSubsteps`) |
| the Dead's cycle | 18 substeps = 9 slots (4.5 s) | `WorldParams::dead_cycle` (`kDeadCycleSubsteps`, `dead.hpp:16`) |
| game hour | 14,400 substeps = 1,200 updates | `kSubstepsPerHour` |
| day since the Event | 345,600 substeps | `kSubstepsPerDay` (`aftermath.hpp`) |

Why 9 slots against 6: the cycle and the update are deliberately out of step (18 is not a
multiple of 12), so the Dead's moves do not lock to the field they climb (`dead.hpp:14`).

Real-time durations from content (sleep, rot, crafting) convert once with `to_substeps(s)`
or `ms_to_substeps(ms)` (rounds up, never to 0). The stage clock (`substeps()`) is 32 bits
and resets per stage; the clock since the Event (`since_event()`) is 64 bits and never
decreases.

## Actions

`Action` (`action.hpp:20`) is the only input core takes: `kind` (Step or Wait), `dir`, and
`substeps` it costs. `Action::step(dir)` is a walk, `Action::step(dir, kRunStepSubsteps)` a
run, `Action::wait()` a wait. A duration of 0 is treated as one slot. Odd durations are legal
(the between layer) but nothing makes one today. Adding a new verb (search, sleep, open)
means a new `ActionKind`, its effect in `World::apply_action`, and its cost in substeps.

## Scent cadence and the occupancy log

Scent and the Dead update on their own clock, every `update_period` substeps, **whatever the
player does** (D-015). While an action plays out, each substep is logged against the tile the
player stands on (`World::log_substeps`, `world.cpp:366`). At the boundary each logged tile
deposits:

```
logged_strength(held) = strength - age_cost * (period - min(held, period)) / period
```

(`world.cpp:381`). A tile held the whole period deposits exactly `strength` (960 by
default); a running step's tile, held 6 of 12 substeps, deposits 960 - 8 x 6 / 12 = 956: a
little less, never more, as if that scent were half an update older. So a runner covers two
cells per update and each reads slightly weaker than a walker's.

## The Dead's slots

Each of the Dead has `step_substeps`: how long one of its moves takes. Spawned units draw a
speed 1 to 3 (`world.cpp:24`), stored as speed x `update_period`, so 12, 24 or 36 substeps.

At the start of every cycle the **poll** (`world.cpp:62`) gives every unit its slots for the
cycle with `plan_slots` (`dead.cpp:9`):

```
step_slots = step_substeps / 2            (6, 12 or 18)
count      = floor(9 / step_slots), plus 1 with chance (9 mod step_slots) / step_slots
offset     = hash(salt, cycle, 2i) mod 9
slot j     = (offset + j * 9 / count) mod 9
```

The chance is `hash_unit(salt, cycle, 2i+1)`. So a speed-1 unit decides 1 or 2 times a cycle
(on average once per update, the same pace as a walking player); speed 2, 0 or 1 times (on
average every 2 updates); speed 3, every 3 updates. Hashes of (stage seed, cycle, index),
never a shared stream, so who moves when changes every cycle and does not depend on anyone
else.

Within one slot (`dead_slot`, `world.cpp:433`):

1. At a cycle start, poll.
2. The slot's units **decide**, in ascending index (skipping any with a move pending): each
   draws a target from the desire field ([The Dead](06-the-dead.md)). A decided move
   **reserves** its target tile.
3. Last slot's decided moves **land**: occupancy moves, the reservation clears.

So a tile vacated now is free only from the next slot, and a crowd files through a gap one
by one, jamming in a one-wide corridor (D-031).

## Computing the next turn while you think

The player presses at most three keys a second (D-011), so the frontend has a gap. ADR-0012
spends it: a worker thread runs `World::speculate(spec)` while the main thread waits for
input, and on input the main thread calls `World::commit(spec, action)`.

`speculate` (`world.cpp:544`) is `const`. It computes, as two independent pieces:

- **the scent half**: sync the speculation's wave from the World's (only the tiles either
  changed, PEO-078), then run the next update **with no deposit**;
- **the Dead's half**: copy the horde and run every slot up to and including the next
  boundary, recording each slot's decisions and any poll. Between updates the Dead read
  only the last update's desire field, never the player, so these slots are already fixed;

then, if the slots reached the boundary, builds the next desire field from both.

`commit` (`world.cpp:608`) applies the action, then advances the clock replaying recorded
slots instead of deciding live. At the boundary, `finish_from` patches the player's logged
deposits into the speculated field with `ScentWave::patch_deposit` and rebuilds the desire
field only round those tiles, then swaps the buffers in. The result is **bit-identical** to
`step`, which `test_world.cpp:391` checks. Rules:

- An action that crosses no boundary leaves the speculation valid for the next one.
- A stale speculation (another update, stage or epoch) falls back to `step`.
- `patch_deposit` is exact only for `speed == 1`; a faster wave always commits live.
- Buffers are reused: a frontend that keeps one `Speculation` allocates nothing per update.

Measured on the M1 (release, one walking step, PEO-030): at 200x120 with 5,000 Dead,
`commit` 0 to 1 µs and `speculate` 28 to 277 µs; at 512x512 with 50,000, `commit` 0 to 3 µs
and `speculate` 0.1 to 1.5 ms. `docs/bottlenecks.md` has the current per-pass costs.

## Input pacing (`TurnInput`)

`turn_input.hpp` turns key presses into turns (D-032, amending D-011). A distinct press is
taken at once if the cap allows, else waits in a queue of up to 3, else is dropped; the queue
plays out no faster than 3 turns a second. A held key's auto-repeat is taken only when the cap
allows and nothing waits, so it never builds lag and stops the moment you let go. Levers:
`TurnInputParams::max_waiting` (3) and `min_interval` (1/3 s). This is the only wall clock in
core, and it paces input, never the simulation.

## Long actions

Sleep, long waits and searching at full detail are designed (PEO-082) but not built. The
cost to know: a slept game hour is 1,200 updates (D-039), about 17 s at 512x512 with 50,000
Dead on the cloud runner, so fast time passing needs its own approach.
