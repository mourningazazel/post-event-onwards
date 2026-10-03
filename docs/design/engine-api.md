# Engine API and scripting: intent (N023)

Status: **forward-looking.** Nothing here is build work yet; the owner will give more guidance.
It records how the owner plans to work with the engine, so today's code does not close it off.

## The intent

- The engine becomes its own project later; this game is one product built on it.
- The owner will write the RPG and roguelike logic himself through a **low-code scripting
  layer** over the engine's existing APIs: actions, skills, items, the Dead and other units,
  and on-screen elements.
- **One shape everywhere.** A player unit, one of the Dead and an item are all things (G01
  D1.2, D1.3); they are addressed, queried and hooked the same way. A UI field and an action's
  failure effect use the same paths.
- Fantasy features (magic and the like) belong to the engine later, not to this game.

## One addressing scheme

A **path** names a thing or one of its parts and properties:
`player.body.arm.hand.finger*`, `item.tool*[capability(cutting)]`,
`player.stats.health.current`. Rules:

- Dots walk the thing graph: parts, slots, contents, properties.
- `*` matches any instance; `[...]` filters by tag, capability, material or size.
- The same path works for any thing with that shape: `unit.body.arm.hand*` reaches a player's
  hand or a Dead's.
- Content IDs and tags already have this shape (`content/README.md`, content-model §2.6); the
  path is how a script reaches them at run time.

## Actions as data, in Lua

The owner's sketch, "Widdle Wood", maps directly onto Lua tables (Lua and sol2 are MIT,
ADR-0015). The game would read this the way it reads `content/` TOML today:

```lua
action "whittle_spoon" {
  desc     = { short = "Whittle Wood", long = "Use a sharp cutting instrument to carve an object" },
  skill    = "woodworking",
  requires = { tool = "item.tool*[capability(cutting), sharpness >= 'sharp']", quantity = 1 },
  uses     = { "material.wood*[size(small)]" },
  difficulty = 24,
  time       = "16m20s",          -- authored in game time, converted to substeps (ADR-0016)
  produces   = { "object.spoon[material(wood)]", quantity = 1 },
  on_fail    = injure { kind = "cut", damage = 10,
                        part = { "actor.body.arm.hand.finger*", "actor.body.arm.hand*" },
                        multi = false },
}
```

`actor` rather than `player`: the same action works for anyone (G01 D1.2).

## UI as data

Menus are described the same way and bound to paths, so adding, removing or editing an element
is a data change:

```lua
tab_menu "character" {
  page "Stats" {
    row { text { bind = "player.stats.health.desc" },
          bar  { filled = "#333333", empty = "#222222",
                 value = "player.stats.health.current", max = "player.stats.health.max" } },
  },
}
```

Menu kinds: **tab menus** (Tab cycles pages, anchored to a screen edge and expanding outward, one
page at a time), **pop-ups**, **context menus**. The owner may prefer an XML-like markup; the
model (elements bound to paths) is the same either way.

## What agents do now

Cheap rules that keep the door open, applied in briefs and reviews:

1. A new mechanic is data first: its numbers, durations and outcomes live in content or in
   tunable structs, not in branches on item IDs.
2. Core functions take a thing and a capability or tag, not a concrete item kind, so a script
   can call them with anything that qualifies.
3. Nothing in core assumes one player (ADR-0017).
4. Public headers stay one concept per file with plain value types: they are the future
   binding surface.

## The owner's authoring tool

The owner will build a small terminal GUI to author the game's text (descriptions and the like)
and much of the later game (D-042). Content stays in plain, documented data files so that tool
can read and write them without going through agents.

## Open, for the owner later

Lua tables or a custom markup; whether scripts can define new systems or only content and
hooks; sandboxing for shared mods; when the engine splits out. See also the save sync scopes in
ADR-0017.
