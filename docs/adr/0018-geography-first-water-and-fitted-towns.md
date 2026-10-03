# ADR-0018: Geography first; towns fit the land and water they sit on

- Status: Accepted (owner note N023). World extent beyond one geography square is D-041.
- Date: 2026-10-03
- Refines: ADR-0007 (hierarchical generation) and ADR-0010 (lazy rivers): rivers still refine
  lazily at fine detail, but their courses now come from the geography map.

## Context

Towns should read as places built where they are: a river town is built around its river,
with bridges and river trades; a lake or sea town has a shore. Most towns in the real world
have no river through or beside them, even with one nearby. Today L0 is noise evaluated
anywhere, and rivers are placed per area without towns knowing.

## Decision

1. **A geography square is generated whole before anything inside it:** 100 km x 100 km at
   coarse resolution (on the order of 100 m cells): elevation, mountains, valleys, forests,
   lakes, coast and sea, and river courses with their size class. It is a pure function of the
   seed and the square's coordinates, cheap to keep in memory, and stored only as its seed.
2. **Large bodies of water are first-class:** lakes and sea are polygons on that map. A town may
   border them; a shore gets shore content (docks, boat sheds, lakeside houses, bait shops,
   breakwaters, flood defences) through building and room tags (`world-catalog.md`).
3. **Settlements are placed on the geography, then shaped by it.** Site scoring already prefers
   landmarks (`world-generation.md`); the layout then follows the features under it: a river
   through a town gets bridges where streets cross it, riverside streets and river-associated
   buildings and items; a shore bends the street grid along it.
4. **Rivers are uncommon near towns.** A town only gets a river through or beside it when a
   course actually runs there, and the course density is tuned so that most towns have none
   within their limits (a data value; proposed start: about 1 town in 5).
5. **Rivers stay lazy at fine detail** (ADR-0010 points 4 and 5): the geography map gives each
   course's control points; the river generator refines meanders, bed and banks per area.
6. **Far geography is only words.** Beyond the detailed rings, the game names landmarks by
   direction and distance ("mountains to the north", "a lake two days west").

## Consequences

- World generation order: geography square → settlement sites → settlements → structures →
  tiles → contents. Children never change parents (ADR-0007 principle 1).
- PEO-095 builds the geography square; PEO-096 fits settlements to it; shore and river content
  joins the building census (PEO-014).
