# N022 — Siege scenarios for the horde

- Date: 2026-10-01
- Source: owner, in the project chat ("Horde siege scenario suite" thread)

> Hi, I would like to add a few more scenarios to your testing process to be sure that eventual
> gameplay elements are captured during design. I would like you to also simulate the following,
> if not already covering the same scenario; 1. three tests with three different building
> structures, each surrounded by a large amount of the dead. Measure dead's behavior and
> placement spread after 1 hour, 2 hours, 6 hours, 12 hours, 24 hours and 48 hours. Do only one
> building variation at a time. Do same test but with an open door and the player at the far end
> of the building from the open door. Then do the same test but with two openings, one at either
> side. Try a sparse and a heavy starting horde variation. The three building variances should be
> 1 5x5 enclosed building, no openings. 1 building that is two 4x4 enclosed squares connected by
> a 1x1 hallway that is 4 tiles long. And the third building would be the same as the second, but
> the hallway is 9 tiles long, and has two 90 degree turns within it. This might need to be its
> own testing suite, since that would be hard to fit in the current suite's budget. You're free
> to keep it in current, if it is not. But when we add non-scent or horde/dead related mechanics
> in the future, we do not need to simulate those in this, as they will be tested for function
> in other tests. This is primarily for testing mechanics that would affect horde swarm
> mechanics.

Follow-up: "5x5 not 15x5 enclosed buildings, sorry" (the first building is one 5x5 building).

## How it was taken

- Not covered today: the swarm scenarios (`tests/core/test_swarm.cpp`) run on open stages for a
  few dozen updates; nothing runs the horde against a building for game hours.
- Its own suite, `peo_siege`, outside the D-033 tiers: 3 buildings x 3 opening setups x 2
  densities, 48 game hours each, is minutes of CPU, not seconds. Queue PEO-088.
- Horde and scent mechanics only; others stay out of it, as asked.
