# ADR-0013: One-metre tiles, one storey per z-level, shared tile occupancy

- Status: Accepted (owner decision D-006, N015)
- Date: 2026-09-28
- Refines: ADR-0006 (the "blocked mobs skip" rule now reads "tiles at capacity are not
  entered"); replaces the single-occupancy assumption in `docs/design/playtest-harness.md`.

## Decision

1. **One tile is one metre.** Real-world distances hold: a city block is ~100 tiles, a town
   20 km away is ~20,000 steps.
2. **One z-level is one storey** (about 3 m). Streets ramp between levels; houses are level.
3. **Tiles are shared.** More than one unit may stand on a tile, up to a per-tile **capacity**
   from the terrain and fittings (data; a doorway holds fewer than open ground). Movement is
   classic roguelike otherwise: one step per action, eight directions.
4. **Crowded tiles are dangerous.** Every step into, out of or within a tile above one occupant
   rolls, from mass, speed multiplier and footing, for **trip** (prone for *N* turns), **knock
   out** (displaced to an adjacent tile with room) or nothing. Anyone prone on a tile that
   units move through is **trampled** (blunt damage, body-part rolled).
5. **Bump-to-attack still holds** for the Dead moving onto the player's tile; sharing a tile
   with the Dead is the "being swarmed" state.

## Consequences

- The occupancy grid becomes a count per tile, not a bit. The movement rule's "free neighbour"
  test becomes "below capacity".
- The rule checker's law changes from "two units never occupy one cell" to "tile occupancy
  never exceeds capacity" and "a knocked-out unit always lands on a valid tile".
- The crowd behaviours the owner wants (hordes packing into doorways, tripping over each
  other, trampling the fallen) fall out of one contest roll rather than swap-only contests.
- Generation levels in `world-generation.md` re-base on 1 m tiles; the census numbers in
  `world-catalog.md` §10 already assume it.
