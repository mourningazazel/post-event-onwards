# N017 — D-024 to D-029: geodesic scent, rot by scaling, loot detail, search per compartment, soak on things, stack quality

- Date: 2026-09-30
- Source: owner, in the "Full codebase review" project thread (answers to `DECISIONS_NEEDED.md`
  D-024 to D-029, raised from the pathfinding and item-system reviews)

> D-024: A
> D-025: A
> D-026: For this, i would say logically, when the item is part of a quantity of other items, it
> would be a single kind. "box of nails", but when you take one out, that single one obtains
> further details if applicable, instantiating the detailed item at that point. If the item would
> be re-combined with the nails, you could include it as an added item aside from the nameless
> quantity, but inside the same "listing" menu of what is in the container. Instead of giving it a
> unique name, perhaps just highlight it different or visually mark that it has not been examined
> yet. When examined, removed, modified etc, then it can gain further details.
> D-027: B - it sounds tedious, and it is meant to be. While there are certain liberties taken in
> making it a good game and not too overly complex, the general point is to replicate the
> struggle for survival in a real-world scenario (with the fantasy Dead spin). Doing certain
> things will be painfully slow process, just like it would be in real life. But when designing
> the menus for this, we will want it able to be traversed easy, mainly just replicating the
> tedium of opening all the individual drawers searching.
> D-028: B
> D-029: B

## What this settles

- **D-024 A:** scent becomes an integer geodesic field that propagates along walkable routes:
  value = strength − C·distance − K·age, a front of `speed` cells per update and up to `gust`
  downwind, per-cell openness (0 indoors), stairs as routes, computed only within a reach radius
  of the player, walls absorbing, a min-plus patch in commit. It replaces diffusion and D-008's
  IIR far layer, and needs no separate indoor rule, so N016's inner-wall buildup is not built.
- **D-025 A:** rot dulls the pull by scaling, never blinds it: in the integer (log-valued)
  field, a Dead subtracts rot × a scale from the value it reads.
- **D-026, the owner's own rule:** a quantity inside a container is **one kind** ("box of
  nails"). A single item taken out **instantiates with its details** at that moment. Put back,
  it stays **its own entry beside the nameless quantity** in the same listing, not merged.
  Entries are not given unique names; an item not yet examined is **visually marked** as such,
  and gains details when examined, removed or modified.
- **D-027 B:** each compartment (drawer, shelf) is searched on its own, with time from its
  volume. The tedium is intended, as in real survival; the menus must make it quick to
  traverse, so only the in-game time is slow.
- **D-028 B:** portable absorbers (a mattress, a sofa cushion, clothing) carry their soak as a
  value on the thing and take it with them when moved.
- **D-029 B:** a stack's quality is a mean plus a spread, sampled when an item is taken.

## Acted on in

- `docs/production/decisions.md` D-024 to D-029; `DECISIONS_NEEDED.md` emptied
- `docs/design/gameplay-model/G03-items-and-belongings.md` D3.2
- `docs/design/scent-mobs.md` round 4
- Queue: PEO-030 and PEO-035 rebriefed; PEO-047 to PEO-049 unblocked; notes on PEO-008, 013,
  038, 039, 050; PEO-052 to PEO-056 from the item-system review
