# Reference, query and effect syntax (proposal, PEO-134)

Status: **settled with the owner 2026-10-13** (D-054 point 6, D-055); section 5 records the
answers. Nothing here is built. Every
example is illustrative; names of real parts, stats and roots get settled item by item.

The aim, in the owner's words: everything in the game is referenced the same way, in a single
path, so there is little syntax to recall and every line reads plainly.

## 1. Paths: naming a record or a value

A **path** is lowercase `snake_case` segments joined by `.`:

```
item.tool.hammer.claw_hammer                       a record (an item archetype)
item.tool.hammer.claw_hammer.mass_g                a value on it
item.tool.hammer.claw_hammer.composition.handle.material            -> item.material.wood.oak
item.tool.hammer.claw_hammer.composition.handle.material.hardness   steps into oak
unit.self.body.head.hair.color                     a value on the player
```

- **Roots.** The first segment says what kind of thing it is: `item`, `unit`, `substance`,
  `action`, `effect`, `skill`, `world` (settings, buildings, rooms, loot) and `tag`. A material
  is an item (D-055): `item.material.wood.oak` is a hunk of oak, and a part made of oak points at
  it. There is no "25 wood", only 25 wooden blocks. A substance (water, a pool of blood) keeps
  its own root. Below a root, segments follow the parent chain: abstract category roots
  first (`item.tool`), then the family (`hammer`), then the variant (`claw_hammer`).
- **Short form.** The flat ID still works wherever the kind is known: `claw_hammer`,
  `material: oak`. Both forms name the same record (D-054 point 1). Files keep flat IDs, so
  moving an item to another family does not break anything that names it.
- **Following references.** When a value is itself a reference (a part's `material`), the next
  segment reads from the record it names. One path can cross any number of records.
- **Archetype or thing in the world.** `item.…` and `unit.…` name templates. A thing in play
  is reached from something that holds it: `self` (the player), the roles of an action
  (section 3), or a place (`here`, `cell(4, 2)`). So `self.hands.right.mass_g` is the mass of
  whatever the player holds, read through its archetype plus its own changes (wear, damage,
  contents). An instance only stores what differs from its archetype, the same rule as
  children overriding parents (D-054 point 3).

## 2. Queries: matching many things

A **query** is a path with wildcards and a filter in braces.

```
item.material.wood.*               any record directly under item.material.wood
item.tool.**                       anything at any depth under item.tool
self.carried.**{material ~ item.material.wood.**, dims_mm.0 > 100}
item.**{capability.cut >= 3, mass_g <= 2000}
```

- `*` matches one segment, `**` any number.
- A filter is a comma list, all of which must hold: `=`, `!=`, `<`, `<=`, `>`, `>=` on
  numbers and IDs; `~` "is under or matches" a path or query (`material ~ item.material.wood.**`);
  `has` for lists (`tags has from.kitchen`).
- A filter path is read from the thing being tested, so `mass_g` means its mass.
- Numbers are integers in the content units (mm, g, ml, seconds), as content already is.

This replaces today's predicate tables in `content/registry/actions.toml`
(`{ capability = "cut", min = 3 }` becomes `capability.cut >= 3`).

## 3. Actions and effects

An action names **roles** (what it needs, as queries), and **effects** (what it does), and
effects refer to roles with `$`.

```toml
[action.carve_spoon]
roles.source = "self.carried.**{material ~ item.material.wood.**, dims_mm.0 >= 150}"
roles.tool   = "self.carried.**{capability.cut >= 3}"
skill   = "skill.craft.woodworking"
time_s  = 1800
effects = [
  "item.kitchen.utensil.spoon{composition.body.material = $source.material}.make(self.hands)",
  "$source.use()",
  "$tool.durability.reduce(1)",
]
```

```toml
[effect.shaken]
effects = ["$target.mind.sanity.reduce(25)"]
```

- An effect is a **call at the end of a path** (D-055): the path says which thing or value, the
  call says what happens to it. Values take `set(x)`, `add(n)`, `reduce(n)`; things take verbs
  such as `make(where)`, `use()`, `move(where)`, `apply(effect.x)`. Every attribute is changed
  the same way, so there is no separate format for value effects and action effects. The call
  list stays short and fixed so lint can check every effect at load.

## 4. How the engine runs it (for the Builder, later)

- Every path and query in content is parsed **once at load** into a compiled form (segment IDs,
  not strings), and lint and the engine share one parser spec, as `derive.py` and core do today.
- Lookups at play time walk compiled segments and allocate nothing (`docs/bottlenecks.md`).
- A path that names nothing is a **load error** naming the file, so a typo never reaches play.
- Paths derive from the parent chain, so a full path changes when a record moves, while its
  flat ID does not. Saves keep flat IDs and remaps (PEO-092).

## 5. Parsing cost at startup

Measured 2026-10-13 (PEO-008 report): all of content/ today is 930 KB of TOML, 418 items, and
loads and resolves in about 30 ms in a release build, so about 30 MB a second. Path and query
strings parse at least as fast as TOML (short, no nesting), and resolving each segment is one
hash lookup (well under a microsecond). If every value in content were a path, startup would
roughly double, to 60 ms. A 15 s budget therefore holds content around **200 to 500 times
today's size** (hundreds of MB of TOML, around 100,000 items) before strings become the
problem, and file reading would be the limit before the parser. The real cost to watch is not
startup but play: so every path is compiled once at load (section 4) and never re-parsed per
update. Decision: strings (D-055); revisit only if a load-time measurement says so.

## 6. Settled with the owner (2026-10-13, D-055)

1. **Roots:** as proposed, except materials are items (`item.material.…`); substances keep
   their own root.
2. **Instances:** `self`, `$role`, `here`, `cell(x, y)`.
3. **Effects:** a call at the end of the path (`….sanity.reduce(25)`), the same form for every
   attribute.
4. **Queries:** braces for filters, `~` for "is under".
5. **Format:** paths and queries are strings in the TOML, parsed once at load.
