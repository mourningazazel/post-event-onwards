# N011 — Survival actions, item properties, and the world catalogue

- Date: 2026-09-27
- Source: owner, in session (a longer work assignment while the owner is away)

> Yes, things you've done stay the same after you come back. I need to walk away now, so i want to
> give you something to work on for a longer period. I want you to think over actions a player
> might do for the purpose of surviving a zombie apocalypse, and come up with desired properties
> and tags you'd like to track in items to perform those actions. I want you to imagine what a
> player can do, as being what a real person could do in those situations, (trimmed down to what
> would make sense in a video game). For an example of the amount of randomness that you should be
> able to do - "the player filled an milk jug that he had emptied out with gasoline that he
> siphoned out of a car using a hose he cut off of a referigation unit when he took it apart, he
> then threw it at the ground infront of zombies and threw a match he lit onto the ground tile and
> it caught fire, burning the zombies ontop of the tile", and all that should be the sort of thing
> possible. Decide on what mechanics, properties and tags you need for the items, and then i want
> you to go through a normal city, outskirts, suburbia and any other setting you'd find yourself
> in, in a typical western world resembeling the United States, and think of all the different
> buildings and types of buildings, the houses and types of houses people would have, the objects
> and items outside, the utilities buildings and all other things that would make up this world,
> and think of all the different objects and items you'd need to create object and item files for.
> I want you to create all of those, but i want you to do it smart. If you can do it where you
> attach further descriptions with the item relationship model, you can save having individual
> items,a nd instead, just further descriptions or modifiers attached to tags and properties you
> use to make additional items. These should either add on more details, or even modify others.
> The actual modification can be done by the systems reading the item tags later, and when you do
> tests on game functions that deal with items, make sure that all the properties and expected
> combinations or functions are tested for accuracy and further functions are tested as well.

Follow-up clarification, the same session:

> onlyd o these for things you think make sense to have in the game / add value, while using the
> further details to add flavor, and can be added via the generation,r ather than stored as actual
> files

## What this settles

- **N009 assumption confirmed:** player-made changes are preserved when an area catches up after
  a long absence.
- **Assignment:**
  - Analyse realistic survival actions.
  - Derive the mechanics, properties and tags items need to support them.
  - Catalogue the US-like world: settings, buildings, houses, outdoor objects, utilities, items.
  - Author the content.
  - Test properties and combinations.
- **Leanness rule:**
  - Author an archetype **only** where it adds gameplay value or distinct behaviour.
  - Everything else (flavour, cosmetic variety, small details) comes from **generation-time
    detail tables and modifiers** attached through tags and properties. Those details add to an
    item, or modify it.

## Acted on in

- [`docs/design/survival-actions.md`](../survival-actions.md)
- [`docs/design/world-catalog.md`](../world-catalog.md)
- `content/`: registry, materials, substances, archetypes, modifiers, detail tables, world data
- `tools/content/` and `tests/content/`: lint, resolver, chain feasibility tests
