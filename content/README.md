# content/: game data (schema reference)

All game content is **data**, authored mostly by Claude (N008, N011) and checked by
`tools/content/lint.py`. Design rationale is in `docs/design/content-model.md` and
`docs/design/survival-actions.md`.

**Golden rules:**

1. **Only use vocabulary registered in `registry/`:** properties, features, capabilities, tags,
   stimuli, fasteners and scent channels. Need something new? Add it to the registry first, with a
   description and its readers (the mechanics that use it).
2. **Lean archetypes (N011).** Create an `item` only if it **behaves differently** or has
   **distinct gameplay value**. Colour, pattern, print, label, small wear, flavour text and minor
   variants belong in **modifiers and detail tables**, which generation attaches.
3. **Physically plausible numbers.** Real-world dimensions and masses. Materials from
   `materials/`. The linter checks mass against dimensions × density.
4. **Integers only**, SI units (see `registry/units.toml`). Qualities are 0–10.
5. **No real brands** (N010). Brand categories point at `brands/`.
6. **Original content only** (original-code policy). Nothing copied from other games' data.

## Directory layout

| Path | Holds | Record type |
|---|---|---|
| `registry/` | Vocabulary | `[property.X]`, `[feature.X]`, `[capability.X]`, `[stimulus.X]`, `[tag."ns.x"]`, `[namespace.X]`, `[fastener.X]`, `[scent.X]`, `[action.X]`, `[shape.X]` |
| `materials/` | Physical materials | `[material.X]` |
| `substances/` | Liquids, granular matter, gases | `[substance.X]` |
| `items/<domain>/` | Archetypes: portable items **and** world objects (furniture, fixtures, vehicles, street furniture) | `[item.X]` |
| `modifiers/` | Variants, states, contents presets, material swaps, and **detail tables** | `[modifier.X]`, `[detail_table.X]` |
| `brands/` | Fictional brand universe | `[company.X]`, `[brand.X]`, `[store_chain.X]` |
| `world/` | Settings, building types, room types, loot tables, outdoor object sets | `[setting.X]`, `[building.X]`, `[room.X]`, `[loot.X]`, `[outdoor_set.X]` |
| `profiles/` | Occupant profiles: outfits and pockets by occupation and time of event | `[profile.X]` |

IDs are `snake_case`, and unique across the whole content tree within their record type.

## Item record

```toml
[item.milk_jug]
name = "milk jug"                 # base name; tags and brands compose the display name
parent = "bottle_base"            # optional; deep-merge inheritance (tables merge, arrays replace)
abstract = false                  # true = template only, never spawned
tags = ["cat.container.bottle", "from.kitchen"]
shape = "container"               # registry/shapes
dims_mm = [250, 150, 150]         # longest first
mass_g = 60                       # empty/base mass; omit to have it estimated (lint warns)
desc = "A translucent plastic jug with a screw cap."
glyph = "item.jug"                # logical tile name (renderer)
why = "Liquid container; throwable; fuel-safe short term."   # detail budget justification (required)
brand_cats = ["dairy"]            # optional: brand categories that may apply
modifiers = ["size_half_gallon"]  # optional: variant/state modifiers this archetype accepts
details = ["plastic_label_print"] # optional: detail tables rolled at generation

[item.milk_jug.composition.body]  # parts keyed by name
material = "hdpe"
share = 90                        # % of mass (optional; used for derived hardness/edge etc.)
[item.milk_jug.composition.cap]
material = "polypropylene"
share = 10

[item.milk_jug.features.container]    # features keyed by registry feature id
capacity_ml = 3785
opening_mm = 38
closable = true
sealable = true
compat = ["liquid", "granular"]

[item.milk_jug.contents]          # optional generation preset
substance = "milk"
fill_pct = [0, 100]
```

Salvage parts (disassembly, M5):

```toml
[item.refrigerator.composition.coolant_line]
material = "copper"
share = 3
salvage = { item = "copper_tubing", count = 1, set = { dims_mm = [1500, 6, 6] }, tool = "fasten>=2", time_s = 600 }
```

`tool` is a requirement string: `"<capability>>=<n>"`, several joined by `&`, or `"none"`.

Compartments (PEO-054, D-027): furniture and fixtures list their drawers, shelves and cupboards,
each searched on its own. An item has `compartments` or `features.container`, never both.

```toml
[item.kitchen_base_run]
# ...
compartments = [
  { name = "top drawer", capacity_ml = 12000, max_dim_mm = 500, closable = true, loot = "kitchen_drawer" },
  { name = "second drawer", capacity_ml = 12000, max_dim_mm = 500, closable = true, loot = "kitchen_drawer" },
  { name = "cupboard", capacity_ml = 150000, max_dim_mm = 550, closable = true, loot = "kitchen_cabinet_contents" },
]
```

`name` is what the search menu shows; `lock` takes the lock feature's params. A room object
overrides a compartment's default loot with `loot = { "top drawer" = "junk_drawer" }`, or gives
the whole item one table with `contains`, rolled once and spread over the compartments by fit
(never both on one object). `upright = true` marks an item whose longest side is its height;
`rests_on = "<item>"` marks one that lies on another (a mattress on its bed frame).

## Modifier record (additive or modifying details, applied by generation)

```toml
[modifier.charred]
kind = "state"                    # variant | state | contents | material | detail
applies_to = { tags_any = ["mat.textile", "mat.wood", "mat.paper", "mat.polymer"] }
name_part = { slot = "prefix", text = "charred", order = 10 }
desc = "blackened and blistered by fire"
add_tags = ["state.charred"]
ops = [ { path = "durability", op = "mul", value = 0.5 } ]
```

Ops: `set`, `add`, `mul`, `append`, `remove`. Paths are dotted, e.g. `features.container.capacity_ml`
or `composition.body.material`.

## Detail table (flavour without files)

```toml
[detail_table.mug_print]
applies_to = { items = ["mug"] }
pick = 1                          # how many entries to roll
entries = [
  { w = 5, text = "printed with a faded company logo" },
  { w = 2, text = "chipped at the rim", ops = [ { path = "durability", op = "mul", value = 0.8 } ] },
  { w = 1, text = "bearing a hand-painted name", add_tags = ["flavor.sentimental"] },
]
```

## Checks

`python3 tools/content/lint.py` checks schema and vocabulary; `python3 tools/content/test.py` runs
the expectations and chain feasibility tests in `tests/content/`.

**Contents must fit their container (PEO-052).** A room object with `contains` needs a `container`
feature or compartments (checked as one box: capacities summed, the largest longest side); a
compartment's own loot is checked against that compartment. The table's mean fill (`derive.loot_expected`: over rolls, the empty chance, weights and
counts, through nested loot) must not exceed `capacity_ml`, nor `max_mass_g` when set, and no
entry's packed longest side may exceed `max_dim_mm`. Worst-case rolls may overflow: generation fills
until full, so capacity is the ceiling, not the table. Sizes are **packed** (`derive.packed_dims`):
rigid shapes keep `dims_mm`; `fabric` and `bag` fold (halve the longest side, double the smallest)
until the longest is at most 450 mm; `cord` coils into a square of side sqrt(length x girth x 0.8),
at least 150 mm. Long rigid things (tools with handles, lumber, long guns) go in a room's `loose`
list or a gun safe, never in a shelf or drawer table; do not raise `max_dim_mm` to make them fit.

**Furniture must fit its room (PEO-054).** Each blocking or fixed object covers its floor area in
m2: its two horizontal dimensions (for `upright` items, the two after its height); `blocks =
"none"` items cover none, and so does one whose `rests_on` item is in the same room. The sum at
the minimum counts of objects always placed must be at most 0.6 x the smallest `size_m2`, and the
sum at every maximum (chance objects included) at most 0.6 x the largest.
