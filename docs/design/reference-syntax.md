# Reference, query and effect syntax (proposal, PEO-134)

Status: **draft for the owner to review** (D-054 point 6). Nothing here is built. Every
example is illustrative; names of real parts, stats and roots get settled item by item.

The aim, in the owner's words: everything in the game is referenced the same way, in a single
path, so there is little syntax to recall and every line reads plainly.

## 1. Paths: naming a record or a value

A **path** is lowercase `snake_case` segments joined by `.`:

```
item.tool.hammer.claw_hammer                       a record (an item archetype)
item.tool.hammer.claw_hammer.mass_g                a value on it
item.tool.hammer.claw_hammer.composition.handle.material            -> material.wood.oak
item.tool.hammer.claw_hammer.composition.handle.material.hardness   steps into oak
unit.self.body.head.hair.color                     a value on the player
```

- **Roots.** The first segment says what kind of thing it is. Proposed roots: `item`, `unit`,
  `material`, `substance`, `action`, `effect`, `skill`, `world` (settings, buildings, rooms,
  loot) and `tag`. Below a root, segments follow the parent chain: abstract category roots
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
material.wood.*                    any record directly under material.wood
item.tool.**                       anything at any depth under item.tool
self.carried.**{material ~ material.wood.**, dims_mm.0 > 100}
item.**{capability.cut >= 3, mass_g <= 2000}
```

- `*` matches one segment, `**` any number.
- A filter is a comma list, all of which must hold: `=`, `!=`, `<`, `<=`, `>`, `>=` on
  numbers and IDs; `~` "is under or matches" a path or query (`material ~ material.wood.**`);
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
roles.source = "self.carried.**{material ~ material.wood.**, dims_mm.0 >= 150}"
roles.tool   = "self.carried.**{capability.cut >= 3}"
skill   = "skill.craft.woodworking"
time_s  = 1800
effects = [
  "make item.kitchen.utensil.spoon{composition.body.material = $source.material} in self.hands",
  "use $source",
  "wear $tool 1",
]
```

```toml
[effect.shaken]
effects = ["$target.mind.sanity -= 25"]
```

- An effect is a **verb** and its arguments: `make`, `use` (consume), `wear`, `move`, `apply
  effect.x to $target`, or an assignment on a value (`-=`, `+=`, `=`). A short fixed verb list
  keeps effects checkable by lint and cheap to run.
- Your form `action.effect.status_effect.mental.sanity.reduce(25)` maps to
  `$target.mind.sanity -= 25`. The path says which value, the operator says what happens to it.
  Writing it as `reduce(25)` at the end of the path also works if you prefer it; that's
  question 3 below.

## 4. How the engine runs it (for the Builder, later)

- Every path and query in content is parsed **once at load** into a compiled form (segment IDs,
  not strings), and lint and the engine share one parser spec, as `derive.py` and core do today.
- Lookups at play time walk compiled segments and allocate nothing (`docs/bottlenecks.md`).
- A path that names nothing is a **load error** naming the file, so a typo never reaches play.
- Paths derive from the parent chain, so a full path changes when a record moves, while its
  flat ID does not. Saves keep flat IDs and remaps (PEO-092).

## 5. Questions for the owner

1. **Roots.** Are `item`, `unit`, `material`, `substance`, `action`, `effect`, `skill`, `world`
   and `tag` the right top level? Should materials sit under `item` (`item.material.wood.oak`, as
   in your example) or have their own root?
2. **Instances.** Is `self`, `$role`, `here` the right way to reach things in play, as opposed to
   archetype paths?
3. **Effects.** `$target.mind.sanity -= 25` (operator), or `….sanity.reduce(25)` (call at the end
   of the path)?
4. **Queries.** Braces for filters and `~` for "is under", or another notation you find easier to
   read?
5. **Strings in TOML.** Paths and queries as plain strings in the content files (shown above), or
   as TOML tables (`{ path = "…", min = 3 }`)? Strings read better; tables need no parser.
