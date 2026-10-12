# Glossary

| Term | Meaning |
|---|---|
| **the Dead** | The enemies: blind walking corpses that follow scent. One is "one of the Dead"; `Dead` in code. "Zombie" is banned everywhere |
| **horde** | Any group of the Dead; `horde` in code (`World::horde()`) |
| **the Event** | When nearly everyone turned. The world's long clock counts from it (`since_event()`) |
| **stage** | One playable map: random walls, an entry and an exit. Designed to give way to one continuous map (PEO-118) |
| **substep** | 250 game ms; the smallest unit of game time |
| **slot** | Two substeps; the moments the Dead may act (the even substeps) |
| **between layer** | The odd substeps, reserved for later use (ADR-0016) |
| **update** | Every 12 substeps (3 s): scent deposits and spreads, the desire field is rebuilt |
| **cycle** | 18 substeps (9 slots): the Dead's slots are reassigned each cycle |
| **poll** | The pass at a cycle start that gives every unit its slots for the cycle |
| **turn** | One player `Action`; the HUD's counter |
| **occupancy log** | The substeps the player spent on each tile since the last update; becomes scent at the update |
| **scent wave / geodesic field** | The scent field: best `strength - distance x route - age x age` over deposits (D-024) |
| **age line** | `age_cost x updates`: subtracted on read so stored values never need ageing |
| **round** | One pull step of the scent wave: every open cell takes its best neighbour offer |
| **gust round** | A round under wind that carries scent only downwind from outdoor cells |
| **active tile** | A 128x8 block of the field that changed last round (or borders one): the only ones pulled |
| **partner waves** | The World's wave and its Speculation's: they copy only tiles either changed |
| **direction byte / flow target** | Per cell, the strongest neighbour's direction. Unused by play since PEO-009 |
| **openness** | Per cell, how far wind reaches it: 255 outdoors, 0 indoors |
| **desire field** | Per cell, nine log-odds (eight moves and stay) the Dead draw from until the next update |
| **log-odds, octave** | Desire values in sixteenths of an octave: +16 doubles a choice's weight |
| **lean** | How strongly a unit favours higher scent; rises with the scent where it stands (D-038 B) |
| **company** | A mild pull toward other Dead, from a box of cells in each direction |
| **reserved tile** | A tile a decided move will land on next slot; nobody else may draw it |
| **speculate / commit** | Compute the next update ahead while the player thinks; apply the action using it, bit-identical to a live step (ADR-0012) |
| **patch / patch_deposit** | Adding the player's deposits after a speculated no-deposit update, exactly |
| **epoch** | A counter bumped by every load and restore, so an old speculation never commits |
| **executor** | The parallel-for core is lent by the frontend; null is serial |
| **field backend** | The seam a GPU implementation of the scent update plugs into |
| **fate** | A unit's attrition threshold; it is gone once the attrition curve passes it |
| **attrition curve** | The share of the Dead gone by each day since the Event (ADR-0009) |
| **area record** | A stored stage to resume later: index, clock, horde, map memory |
| **map memory** | Cells the player has seen on this stage: terrain stays known, nothing on it |
| **sight field** | The cells visible from the player now (symmetric shadowcasting, 152 cells) |
| **geography cell / square** | 100 m cell; a 1000x1000-cell (100 km) square of the endless map |
| **site cell / settlement site** | A 12.5 km block of the map holding at most one scored town site |
| **Q16** | Fixed point with 65,536 meaning 1.0 |
| **section / sync scope** | One system's part of a save; who owns it when networked (World, Character, Local) |
| **siege suite** | `peo_siege`: hordes round three buildings for 48 game hours, checked against a baseline |
| **unit tier / scenario tier** | Fast tests (all under 1 s) and slower `scenario*` suites (under 5 s), D-033 |
| **Architect** | The cloud Claude: direction, briefs, headless tests, tooling, review |
| **Builder** | The local Claude on the owner's machine: implementation, running the game, perf numbers |
| **brief** | An item's plan: goal, context, units, acceptance, tests, manual steps, out of scope |
| **unit (of work)** | One commit-sized step of a brief |
| **Validation** | Queue status: built and reported, waiting for review |
| **AwaitingUser** | Queue status: waiting on the owner's answer to a D-entry |
| **PEO-xxx / D-xxx / ADR-xxxx / Nxxx** | Queue item / owner decision / architecture decision / owner's verbatim note |
| **D-021** | The performance budget: speculate under 100 ms, commit under 1 ms on the M1 Air |
| **DF grid tileset** | A Dwarf Fortress-style PNG atlas: 16x16 glyphs in code page 437 order |
