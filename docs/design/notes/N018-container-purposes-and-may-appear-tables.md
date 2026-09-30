# N018 — Container purposes and "may randomly appear" tables (follow-up to D-027)

- Date: 2026-09-30
- Source: owner, in the project chat (a follow-up to D-027, search per compartment)

> On d-027, i realized, a good idea for this is to make the layer directly above the contained
> items, decide ahead of time what category of items would be in there, perhaps on examine. That
> helps keep things organized and predictable, also lets you say things like "Cutlery drawer" when
> you later identify the examined container. That does bring up a point tho, if we are applying
> archetypes to containers, and containers nested inside of containers, etc., we will need a
> secondary check that links types of containers to another set of loot-tables that hold items
> that might randomly be in there. This would be useful for placing rare or useful items, as well
> as dangerous or negative ones. We could simply link the tag of that container's 'type', to a
> "may randomly appear" table. I think this should be split into two tags however, one for the
> type of container, but also another that we can use as simply a modifier field, that we can use
> to insert other specific or environmental objects/loot tables into archetyped containers. For
> example, you might have a cabinet that is marked as "maintenance" cabinet, and then another
> modifier that relates it to the building's type, like an office building, and then you'd see
> another set of objects available to place like paper and pens, where the maintenance cabinet
> for a retail shop might have store supplies. Since it is not much of a cost to examine 3 tags
> instead of 2, i would like it if you could have a second modifier tag (3 tags total) that can
> be used to place specific items in the container. however you want to handle that, if you more
> just get what i am trying to do.

## What this settles

- **Each compartment has a purpose,** rolled from its seed before its contents: a cutlery
  drawer, a junk drawer, a maintenance cabinet. The purpose picks the main loot table and names
  the compartment once it is examined ("cutlery drawer"). Nested containers get one too.
- **Three tags per container, each linked to "may randomly appear" tables** that add rare,
  useful, dangerous or negative items on top of the main loot:
  1. **kind**: the purpose itself (`kind.maintenance_cabinet`);
  2. **context**: where it is, from the building type and room (`ctx.office`, `ctx.retail`), so
     the same maintenance cabinet holds paper and pens in an office and store supplies in a shop;
  3. **detail**: one more specific modifier for targeted placement (a persona's hobby, an
     aftermath trace, an authored story beat).

## Acted on in

- `docs/production/decisions.md` D-030
- Queue: PEO-057 (content: purposes, the three tags, may-appear tables); notes on PEO-053, PEO-054
