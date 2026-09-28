# N015 — Lock gradients, group movement confirmed, D-003 to D-006: gameplay model, death and rising, stats and books, tiles and occupancy

- Date: 2026-09-28
- Source: owner, in session (answers to `DECISIONS_NEEDED.md` D-003 to D-006 and to the
  Architect's flagged calls)

> As long as locks are on a gradient for completion with tiered steps that modify the
> difficulty, it's correct. Tie the tiered difficulty to lock types, and weight chance to appear
> by type of building or container, and by any generative element denoting the locked element
> should be higher security. Correct on group movement. D-003: a, accept all. D-004: b, as
> previous players, unless decapitated or destroyed brain (encouraging suicide near death), the
> player rises as a visual tracking zombie with the same stats in the same starting location.
> Players are spawned within the same 2 mile distance, but not within 1 mile. D-005: stats
> should resemble physical stats of a real person, with skills roughly matching real world
> skills. Skill progression will be done both by skill usage and by reading texts for skills.
> Libraries are rare but useful locations, but I want thousands of randomly generated books,
> broken up by sections of a library, that take time to read, have only a small chance of
> gaining a skill, and will be hard to grind skills with due to time spent reading (passing
> gametime and horde buildup), and with book complexity. More complex books take longer to
> read, but have higher chance at skill gain. Once read, the book offers no further points.
> This should be categorized by difficulty and time to read. Harder difficulty or longer time to
> read, result in higher chance of skill point. No book may give over 1, and there will be
> tiered difficulty gates that you either can't understand the text, or it's so simple it
> doesn't give xp. D-006: One tile is one meter and one z tile is one story. Classic roguelike
> mechanics. More than one unit can be on one tile, but each movement step, there is the chance
> to trip or be knocked out of the tile. Trampled if on ground.

## What this settles

- **Locks** are a gradient: each **lock kind** (latch, knob, deadbolt, padlock, combination,
  electronic, cam, bar) sets a tier band that modifies picking and bashing difficulty. The
  **chance a higher-security lock appears** is weighted by building type, container type, and
  any generative element marking the thing as high security (armory, pharmacy, safe, gun store).
- **Group movement** (scent-mobs round 3, the weighted draw with aggregate and company pull) is
  confirmed as written.
- **D-003 A:** all gameplay-model recommendations in G01–G03 are accepted.
- **D-004 B:** the world persists across deaths. A dead player character **rises as a
  vision-tracking Dead with the same stats at the run's starting location**, unless the body
  was decapitated or the brain destroyed (a deliberate reason to end yourself near death). The
  next character spawns **between 1 and 2 miles** from the same start.
- **D-005:** attributes are the **physical stats of a real person**; skills are real-world
  skills. Progression is by **use and by reading**.
  - **Libraries** are rare and valuable. **Thousands of generated books**, organised by library
    section, each with a **difficulty** and a **reading time**. Reading passes game time (and the
    horde builds up), gives only a **small chance** of a skill point, and a book gives at most
    **one point, once**. Harder or longer books give a higher chance. **Gates:** too hard and the
    text cannot be understood; too simple and it gives nothing.
- **D-006:** **one tile is one metre, one z-level is one storey.** Classic roguelike mechanics.
  **More than one unit may occupy a tile**; every movement step into or within a crowded tile
  carries a chance to trip or be knocked out of the tile, and anyone on the ground is trampled.

## Acted on in

- `docs/adr/0013-tile-scale-and-shared-occupancy.md`
- `docs/design/gameplay-model/G01`–`G04` (owner decisions recorded)
- `docs/design/survival-actions.md` §2.10 locks, §2.15 books
- `docs/design/scent-mobs.md` round 3 (shared occupancy)
- `docs/design/playtest-harness.md` rule list
- `docs/design/purposes.md`; `docs/production/vision.md`
- `content/registry/properties.toml` (`teaches` model), `content/modifiers/furniture_electronics.toml`
- `WORK_QUEUE.json` items
