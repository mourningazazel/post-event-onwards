# Content format (.peo): examples and rules (D-057)

Every kind of record in the proposed format, using real content. Approved by the owner
2026-10-13 (D-057), root tree included. The grammar the parsers build to is the last section.

## The rules in one screen

```
# a comment runs to the end of the line

item.tool.hammer.claw_hammer {                  # a record: its full path, then a block
    mass = 570 g,                               # key = value; commas go between fields
    dims { l = 330 mm, w = 130 mm, h = 30 mm },
    value = 45,                                 # gradients are integers, never words
    name.text = "claw hammer",                  # "..." is verbatim: no escapes, no formatting
    desc.text = text.item.tool.hammer.claw_hammer.desc,   # or a path to a text record
    tags = [def.tag.use.tool, def.tag.use.weapon]
}
```

- **The header path is the parent chain.** `item.tool.hammer.claw_hammer` means its parent is
  `hammer`, whose parent is `tool`, under the `item` root. There's no `parent =` line.
- **Commas separate items.** A comma goes between two fields or two list entries, never
  after the last one before `}` or `]`, and never after a whole record. Line breaks and
  indentation mean nothing.
- **Every reference is a full path.** `material = item.material.plastic.hdpe`, never `hdpe`.
  A query (`item.material.plastic.hdpe.**`) means "any of these". Short IDs are gone from the
  files. (This changes D-054 point 1. Saves still keep flat IDs internally.)
- **No word-valued scales.** Anything that is more or less (value, flammability, noise,
  balance) is an integer or a measured quantity. The dev tool's context menu shows what a
  number means ("45: valuable"). Categories that are real things stay as paths to records:
  `shape = def.shape.rod`, `skill = mechanic.skill.craft.woodworking`.
- **Units sit next to the number.** Lengths are metric only: `mm`, `cm`, `m`, `km`. Feet and
  inches are a load error. Mass, volume, time and temperature may use any unit (`g`, `kg`, `lb`,
  `oz`, `ml`, `l`, `gal`, `s`, `min`, `h`, `C`, `F`), and all are converted to one integer
  base unit at load. The game can show any value in metric or imperial. `%` and ranges
  (`1..3`) work everywhere.

## Strings and text

```
"verbatim"            # double quotes: shown exactly as typed. "<b>" shows the characters <b>
"""
formatted             # triple quotes: may span lines, and reads escapes and formatting
"""
```

- **In `"""..."""`:** `\n` is a new line, `\\` a backslash, `\"` a quote. `<b>...</b>` is bold
  and `<i>...</i>` italic. `${path}` drops in a value read from the thing being described.
  Indentation shared by every line is trimmed.
- **Text lives in its own records**, under the `text` root, so one description can be shared,
  translated or generated. An item's `name.text` and `desc.text` point at one. A short name
  may be written inline in quotes.
- **A text record is either a fixed string or a generator.** Reading `desc.text` on an item
  runs the generator, which builds the line from the item's own values. A variant three
  levels down describes itself without anyone writing its text by hand.

```
text.item.tool.hammer.claw_hammer.desc {
    text = """
        A steel-headed hammer with a split claw
        and a <i>hickory</i> handle.
    """
}
text.gen.item.tool {
    # A generator: picks the first line whose `if` holds, fills in values from the item.
    lines = [
        { if = $self{ condition <= 20% },
          text = """A <b>battered</b> ${$self.name.text}, its ${$self.composition.head.material.name.text} head pitted with rust.""" },
        { text = """A ${$self.name.text} with a ${$self.composition.handle.material.name.text} handle.""" }
    ]
}
```

## 1. Category roots and families

```
abstract item.tool {
    tags = [def.tag.use.tool],
    value = 20,
    brands = [brand.category.tools],
    desc.text = text.gen.item.tool         # every tool describes itself unless a child says otherwise
}
item.tool.hammer {
    name.text = "hammer",
    shape = def.shape.rod,
    dims { l = 330 mm, w = 130 mm, h = 30 mm },
    mass = 570 g,
    durability = 9,
    balance = 85%,                          # how far along its length the weight sits
    glyph = art.glyph.item.hammer,
    tags = [def.tag.use.weapon, def.tag.from.garage, def.tag.from.construction],

    composition {
        head   { material = item.material.metal.steel.carbon_steel, share = 70% },
        handle { material = item.material.wood.hardwood,            share = 30% }
    },
    features {
        striking_face { area = 700 mm2 },
        grip          { hands = 1, girth = 32 mm }
    }
}
item.tool.hammer.claw_hammer {
    name.text = "claw hammer",
    desc.text = text.item.tool.hammer.claw_hammer.desc,
    value = 45,
    capabilities { pry = 2 },
    composition.handle.material = item.material.wood.hardwood.hickory
}
item.tool.hammer.sledgehammer {
    name.text = "sledgehammer",
    dims { l = 900 mm, w = 200 mm, h = 60 mm },
    mass = 10 lb,                           # any mass unit; stored in grams
    features.grip.hands = 2
}
```

## 2. Materials are items, and have variants

```
abstract item.material {
    shape = def.shape.block,
    glyph = art.glyph.item.chunk
}
item.material.plastic.hdpe {
    name.text = "HDPE",
    density = 950 kg/m3,
    flammability = 55,                      # 0 to 100, no words
    melt = 130 C
}
item.material.plastic.hdpe.strands {
    # the same plastic, spun into strands (rope, tarp weave)
    structure = def.structure.strands,
    tensile = 40
}
```

A part names a material by full path, either one variant or any of them:

```
composition {
    body  { material = item.material.plastic.hdpe,         share = 90% },  # solid HDPE
    weave { material = item.material.plastic.hdpe.strands, share = 10% }  # the strand variant
}
```

## 3. Containers and compartments

```
item.container.jug.milk_jug {
    name.text = "milk jug",
    dims { l = 250 mm, w = 150 mm, h = 150 mm },
    mass = 60 g,
    composition {
        body { material = item.material.plastic.hdpe,          share = 90% },
        cap  { material = item.material.plastic.polypropylene, share = 10% }
    },
    features.container {
        capacity = 1 gal,
        opening = 38 mm,
        closable = yes,
        sealable = yes,
        holds = [substance.liquid.**, substance.granular.**]
    },
    contents { substance = substance.liquid.milk, fill = 0%..100% }
}
item.furniture.cabinet.kitchen_base_run {
    name.text = "kitchen base cabinets",
    dims { l = 2400 mm, w = 600 mm, h = 900 mm },
    compartments {
        top_drawer {
            name.text = "top drawer",
            capacity = 12 l,
            max_size = 500 mm,
            purposes = [
                { purpose = world.purpose.cutlery_drawer, w = 30 },
                { purpose = world.purpose.junk_drawer,    w = 25 }
            ]
        },
        cupboard {
            name.text = "cupboard",
            capacity = 150 l,
            purposes = [
                { purpose = world.purpose.dish_cupboard,     w = 25 },
                { purpose = world.purpose.cookware_cupboard, w = 25 }
            ]
        }
    }
}
```

## 4. Substances

```
substance.liquid.blood {
    name.text = "blood",
    density = 1060 kg/m3,
    stain = 90,
    scent = def.scent.blood
}
```

## 5. Units: people, the player, the Dead

```
abstract unit.human {
    body {
        head {
            hair { color = def.color.black, length = 40 mm },
            eyes { color = def.color.brown }
        },
        height = 175 cm
    },
    mind { sanity = 100, morale = 50 },
    hands { left = empty, right = empty }
}
unit.human.civilian.nurse {
    name.text = "nurse",
    outfit = [item.clothing.top.scrubs_top, item.clothing.bottom.scrubs_pants, item.clothing.footwear.sneakers],
    carried = [
        { item = item.personal.id.lanyard_badge },
        { item = item.medical.medicine.painkillers, chance = 40% }
    ]
}
unit.self {
    parent = unit.human.civilian           # the one record whose path is fixed by the game
}
unit.dead.walker {
    name.text = "the Dead",
    speed = 1,
    senses { smell = 60, sight = 6 m }
}
```

## 6. Actions and effects

```
mechanic.action.craft.carve_spoon {
    desc.text = "Carve a spoon from a block of wood.",
    skill = mechanic.skill.craft.woodworking,
    difficulty = 20,
    time = 30 min,
    noise = 35,                             # a gradient, not "quiet"

    roles {
        source = self.carried.**{
            material ~ item.material.wood.**,
            dims.l >= 150 mm
        },
        tool = self.carried.**{ capability.cut >= 3 }
    },

    effects = [
        item.kitchen.utensil.spoon{
            composition.body.material = $source.material
        }.make(self.hands),
        $source.use(),
        $tool.durability.reduce(1)
    ],

    on_fail = [
        { outcome = mechanic.outcome.failed,             w = 70 },
        { outcome = mechanic.outcome.damaged_components, w = 25 },
        { outcome = mechanic.outcome.bad_event,          w = 5 }
    ]
}
mechanic.effect.mental.shaken {
    desc.text = "Something you saw will not leave you.",
    effects = [
        $target.mind.sanity.reduce(25),
        $target.mind.morale.reduce(10)
    ],
    lasts = 2 h
}
```

## 7. Modifiers and detail tables

```
mechanic.modifier.state.charred {
    applies_to = item.**{ tags has def.tag.mat.textile | def.tag.mat.wood | def.tag.mat.paper },
    name_part { slot = def.slot.prefix, order = 10, text = "charred" },
    tags = [def.tag.state.charred],
    effects = [ $item.durability.multiply(50%) ]
}
mechanic.detail_table.mug_print {
    applies_to = [item.kitchen.tableware.mug],
    pick = 1,
    entries = [
        { w = 5, text = "printed with a faded company logo" },
        { w = 2, text = "chipped at the rim", effects = [ $item.durability.multiply(80%) ] },
        { w = 1, text = "bearing a hand-painted name", tags = [def.tag.flavor.sentimental] }
    ]
}
```

## 8. World generation: loot, rooms, extras

```
world.loot.smoker_evidence {
    rolls = 1,
    empty = 0%,
    entries = [
        { item = item.personal.tobacco.cigarettes, w = 10, count = 1..3 },
        { item = item.fire.lighter,                w = 6,  count = 1..2 },
        { item = item.fire.matchbook,              w = 5,  count = 1..3 }
    ]
}
world.room.barn_interior {
    desc.text = """
        Inside a working barn: hay, feed and fencing, the tractor,
        fuel drums, a workbench, a meat freezer.
    """,
    size = 80 m2..600 m2,
    floor = world.surface.ground,
    tags = [def.tag.from.farm],
    objects = [
        { item = item.farm.hay.hay_bale,             count = 10..200 },
        { item = item.furniture.storage.metal_shelving, count = 1..4, contains = [world.loot.farm_barn_stock] },
        { item = item.furniture.table.workbench,     count = 1, chance = 60%, contains = [world.loot.workbench_tools] },
        { item = item.vehicle.tractor,               count = 1, chance = 50% }
    ]
}
```

## 9. Small records

```
remap.item.tool.hammer_old = item.tool.hammer
def.tag.use.weapon { desc.text = "Usable as a weapon." }
mechanic.skill.craft.woodworking { name.text = "woodworking" }
```

## 10. Handlers: fields and actions that react (D-058)

Any number field can be a plain value or a block with handlers. Any record can also hook the
actions done with it. A handler is a list of effects, run in order, every time it is triggered.

```
item.weapon.firearm.handgun.pistol {
    durability {
        value = 9,
        on.reduce = [
            if ($input > 3) { $self.composition.frame.crack(), output($input) },
            else = { output($input * 50%) }
        ]
    },
    skill_required {
        value = 30,
        on.get = [
            if ($user.body.arm.**.hand.damage < 10) { output($value + 20) }
        ]
    },
    on.use.fire = [
        if ($user.body.arm.**.hand.damage < 10) {
            $self.fail_rate += 50%,
            $self.misfire_rate *= 2
        },
        if ($self.condition < 10%) {
            chance(&rate = 50%,
                $self.condition = 0,
                mechanic.effect.explosion(&size = 10 m, &heat = 900 F))
        }
    ]
}
```

- **`on.<operator>`** (`on.set`, `on.add`, `on.reduce`, `on.multiply`, `on.divide`) runs when
  that operator is called on the field, in place of the plain change. `$input` is what the
  caller passed and `$value` the field's current number; `output(x)`, used only inside an
  `on.` handler, says what the operator applies. With no `output`, the plain change runs with
  `$input`.
- **`on.get`** runs whenever the field is read, so a property can depend on anything outside
  the item (an injured hand raises the skill a gun needs). With no `output`, the stored value
  is read.
- **`on.<action path>`** (`on.use.fire`) runs whenever that action is done with the record,
  before the action's own effects. Several `if` blocks in one handler all run, in order.
- **`else = { effects }`**, the last entry of a handler list, runs only when no `if` in that
  list fired: a default without writing the opposite `if`. One per list.
- **`if (condition) { effects }`**: effects inside braces are separated by commas. A query in a
  condition (`$user.body.arm.**.hand.damage < 10`) holds if any match holds.
- **`x += n`, `x -= n`, `x *= n`, `x /= n`, `x = n`** are short for `x.add(n)`, `x.reduce(n)`,
  `x.multiply(n)`, `x.divide(n)`, `x.set(n)`, and go through the same handlers. `==`, `!=`,
  `<`, `<=`, `>`, `>=` compare.
- **`&name = value`** passes a named argument that sets that value on the called record for
  this call only: `explosion(&size = 10 m, &heat = 900 F)` explodes bigger and hotter than the
  explosion record's defaults, without changing the record.
- **Handlers inherit** like any field: a variant overrides just `durability.on.reduce` to wear
  faster or slower.
- **No loops**: inside a handler, the same operator on the same field changes it directly
  instead of running the handler again, and a chain of handlers stops at a fixed depth with a
  logged error.
- **Cost**: fields with no handlers read as fast as plain numbers. Every `on.get` is listed at
  load, so the dev tools can show which values are computed.

## The root tree

Every record sits under one of ten roots. Each root is an umbrella over related kinds, so the
first word of any path says what kind of thing it is.

```
item.*          every physical object: tool, clothing, furniture, container, vehicle, material, ...
unit.*          everything that acts: human (unit.self), dead, animal
substance.*     what fills or spills: liquid, granular, gas
world.*         places and how they fill: setting, building, room, purpose, loot, extras,
                outdoor_set, surface
mechanic.*      rules of play: action, effect, skill, outcome, modifier, detail_table, reaction
def.*           vocabulary the rest is built from: property, feature, capability, shape,
                structure, tag, color, slot, scent, measure (the units of measure)
text.*          every piece of prose: fixed strings and generators (text.gen.*)
art.*           how things look and sound: glyph, later sound and tile
brand.*         the fictional brand world: company, brand, store_chain, category
remap.*         renamed IDs, for old saves
```

- Tags become `def.tag.use.weapon`. It's longer, but it says exactly what a tag is.
- `item`, `unit` and `substance` could sit under one more umbrella (`thing.*`, everything
  physical). I've kept them as roots, since you named `item.*` and `unit.*` directly.
- Anything new goes under the umbrella it belongs to before it gets a root of its own.

## Settled (2026-10-13)

- **Dims:** `dims { l = …, w = …, h = …, }` only, no short form.
- **Quotes:** a `"` inside text uses `"""…"""`.
- **Generated text:** `${…}` inside `"""`.
- **Weighted entries:** always named blocks.
- **`unit.self`:** keeps its `parent =` line.
- **Roots:** shared umbrella roots, as abstract as the content allows: the tree above, approved.

## Grammar (for the parsers, PEO-135)

Both parsers (core and `tools/content/`) build to this, and one set of conformance cases proves
they agree. A file is UTF-8; `#` outside a string starts a comment to the end of the line;
whitespace and line breaks separate tokens and mean nothing else.

```
file    = { record }                                 # records need no comma between them
record  = [ "abstract" ] path block | path "=" value
block   = "{" [ field { "," field } ] "}"            # commas only between fields
field   = lhs "=" value | lhs block
lhs     = name { "." name }                          # a dotted lhs sets a nested value
list    = "[" [ value { "," value } ] "]"           # a comma before ] or } is an error
value   = quantity [ ".." quantity ] | "yes" | "no" | string | path | list | block | expr
quantity= number [ unit ]
path    = name { "." name }                          # a bare dotted name
name    = [a-z_][a-z0-9_]*
expr    = anything else, up to the comma that ends its field or list element, with (), [], {} balanced
          and strings skipped: queries, $roles, calls. Kept as text; PEO-132 parses it.
```

- **Strings.** `"..."`: verbatim, one line, no escapes, cannot hold `"`. `"""..."""`: may span
  lines; reads `\n`, `\t`, `\\` and `\"` (any other `\` is an error); `<b>`, `<i>` and `${...}`
  are kept as written for the text system; a line break right after the opening quotes and the
  whitespace-only line before the closing ones are dropped, then the indentation every line
  shares is trimmed. Each string records whether it was formatted.
- **Numbers and units.** A number without a unit is an integer. With a unit it may have a
  decimal point and is converted to the base unit of its dimension: length mm (`mm`, `cm`, `m`,
  `km`); area mm2 (`mm2`, `cm2`, `m2`); mass g (`g`, `kg`, `oz`, `lb`); volume ml (`ml`, `l`,
  `fl_oz`, `gal`, US); time s (`s`, `min`, `h`, `d`); temperature whole C (`C`, `F`); density
  kg/m3; percent `%`. The result rounds half away from zero, as derive.py does. `ft`, `in`, `yd`
  and `mi` are errors that say lengths are metric. A range takes the unit of either end, and
  both ends must be the same dimension.
- **Records.** The header path is kept whole; what its prefix means (the parent chain) is
  resolution, not parsing. The same path twice is an error naming both files.
- **Errors** name the file, line and column, and parsing goes on to the next record so one run
  lists every error.

