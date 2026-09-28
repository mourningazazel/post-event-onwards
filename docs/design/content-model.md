# Content model: archetypes, properties, features, reactions and tags (proposal)

Origin: owner note [N008](notes/N008-ai-authored-content-and-properties.md). Builds on G01 (thing
model), G02 (authored vs generated) and G03 (items and locations).

Status: **proposal.** Claude authors most content data; this document defines the structure that
content must fit, so objects combine logically **without being pre-programmed for each pairing**.

## 1. Principles

1. **Archetypes, not catalogues.**
   - Author *archetypal* items ("kitchen knife", "steel pipe", "business suit jacket").
   - Variation comes from generated instance detail: size within a range, condition, colour,
     wear, contents.
   - A few hundred well-built archetypes beat thousands of near-duplicates. RogueBasin's "What a
     RL should be" warned about exactly that combinatorial bloat.
2. **Behaviour comes from physical truth.** An object's behaviour is **derived** from its
   materials, form and features wherever possible, not hand-set per item. A steel pipe is a good
   club *because* it is heavy, hard, rigid and long, not because it has a "club" flag. Hand-set
   overrides exist for the exceptions.
3. **Detail budget.** Every property must earn its place in one of three ways, and the content
   validator flags any property that doesn't:
   - **(a) mechanics:** some system reads it
   - **(b) world-building:** it appears in descriptions or tells a story
   - **(c) aesthetic:** it pleases or rewards the player
4. **Things are products of their environment.** Mobs, rooms and containers are generated from
   *who and where they were* at the event moment: occupation, place, time of day, season and
   weather. The office worker wears a suit; at 3 a.m. most people wear sleepwear.
5. **Real-world lists, filtered.** Place inventories list what you'd really find, in realistic
   quantities, keeping only items with a use, flavour or aesthetic value.
   - **Clutter is aggregated:** "a stack of papers" or "assorted desk clutter" is one flavour
     object, not 300 items.
6. **Open vocabulary, registered.** Properties, features and tags are defined in a **registry**
   (type, unit, meaning, and which systems read it). Claude can add to it as content grows; the
   validator enforces the registry.

## 2. The layers of an object

| Layer | What it holds | Authored or derived | Example |
|---|---|---|---|
| **Materials** | Global table of physical materials | Authored once (~100–200 entries) | steel, glass, drywall, cotton, oak, flesh |
| **Form** | Dimensions, mass, shape class, composition by part | Authored per archetype; mass derivable | pipe: 1000 × 27 × 27 mm, `rod`, steel |
| **Features** | Named attach and use points with parameters | Authored per archetype | `grip`, `socket`, `edge`, `mouth`, `pocket` |
| **Capabilities** | What the object can *do*, with magnitudes | **Derived** from the layers above; override allowed | `hammer 6`, `pry 7`, `reach 1000 mm` |
| **Reactions** | What happens under impact, heat, water, cutting… | **Derived** from materials and form; override allowed | glass: `onImpact → shatter`, leaving sharp shards |
| **Tags** | Namespaced labels for generic queries | Authored and derived | `use.prying`, `from.office`, `emits.rot` |
| **Flavour** | Names, description fragments, glyph and colour (logical tile name) | Authored | "a heavy steel pipe, about a metre long" |

### 2.1 Materials (the physical truth)

Each material defines:

- **density** (g/cm³), used to derive mass from volume
- **hardness** (0–10): scratch and dent resistance, and how well it damages softer things
- **toughness:** brittle ↔ tough. Decides shattering vs denting vs bending.
- **rigidity:** flexible ↔ rigid. Decides pry and lever capability and whether it holds a shape.
- **edge-holding** (0–10): how sharp an edge it can take and keep
- **flammability:** ignition ease and burn rate; **charring** behaviour
- **water behaviour:** waterproof / absorbs / rusts / dissolves / ruined
- **conductivity:** thermal and electric (insulation, burns, future electrical systems)
- **sound when struck:** clang / thud / crack / crunch. This feeds **noise events**, which
  sound-capable zombies react to.
- **scent when burned or rotting:** links to **scent channels**
- **edible and nutrition** (for food materials)

### 2.2 Form

- **Dimensions:** length × width × height in mm. The longest dimension drives the capacity rule
  (G03 D3.1) and reach.
- **Mass:** in g. Derived from volume × density × fill factor, unless authored.
- **Shape class:** `rod`, `blade`, `sheet`, `block`, `sphere`, `container`, `bag`, `fabric`,
  `cord`, `granular`, `liquid`, `complex`.
- **Composition:** parts, each with a role and a material, e.g. knife = `blade: steel` + `grip:
  polymer`. Parts can be real sub-things (G03 `PartOf`) when they matter; otherwise they are just
  composition entries.
- **Balance:** where the mass sits along the long axis, derived from part positions. Used for
  swing damage and handling.

### 2.3 Features: the "handle / handled" idea, generalized

**Features** are typed points on an object where it can be *held*, *attached to*, *used* or
*filled*. Attachment is generic: **any object fits any compatible feature if it satisfies that
feature's limits.**

| Feature | Parameters | Enables |
|---|---|---|
| `grip` | hands (1 or 2), girth | Holding and wielding |
| `socket` (end of a haft or pole) | `maxAttachMass`, `maxAttachSize`, accepted shape classes, joint methods allowed | Attaching a head: knife + pole = spear; hammerhead + handle; brick + stick = crude maul |
| `edge` | length, sharpness | Cutting and chopping (derives `cut` and `chop`) |
| `point` | sharpness, width | Piercing |
| `striking_face` | area | Blunt impact (derives `hammer`) |
| `mouth` / `lid` | diameter, closable, sealable | Filling, pouring, sealing (liquids, granular) |
| `pocket` / `compartment` | volume, maxMass, maxDim | Containment (G03) |
| `strap` / `clip` / `hook` / `loop` | load limit | Carrying on body slots or on other objects (strapping to a backpack) |
| `slot` | accepted type, e.g. `battery.AA` | Powering devices, magazines, cartridges |
| `thread` / `mount` | standard | Precise assembly (lens on scope, nozzle on hose) |

**Joints.** Attaching creates a `PartOf` link plus a **joint record**:

- **method:** friction fit, tape, cord lashing, wire, screw, glue, weld
- **joint strength:** from the method, the amount of consumable used, and the fit
- **wear:** joints take stress on each use and can loosen or fail through their own reaction
  rules, e.g. a taped knife-spear loses its head after hard impacts

**The composite's capabilities are derived again from its parts:**

- reach = haft length + head length
- pierce from the head's `point`
- swing damage from total mass and balance
- durability limited by the weakest of joint and parts

### 2.4 Capabilities (what it can do, with magnitudes)

Capabilities are **derived** from the layers above; examples:

- `cut` / `chop` = edge sharpness × edge-holding, versus target hardness
- `hammer` = mass × striking-face hardness
- `pry` = rigidity × length
- `dig` = shape (blade or scoop) × rigidity
- `reach` = length

Proposed starting vocabulary:

- **Working:** `cut`, `chop`, `saw`, `pierce`, `hammer`, `pry`, `dig`, `scoop`, `drill`,
  `scrape`
- **Reach and holding:** `reach`, `grip`
- **Binding and joining:** `bind` (cordage), `adhere` (tape, glue), `fasten`
- **Containing:** `contain.liquid`, `contain.granular`, `contain.solid`, `seal`
- **Protection:** `insulate`, `waterproof`, `cushion`, `armor.blunt`, `armor.cut`,
  `armor.pierce`, `armor.bite`
- **Fire and light:** `light` (radius), `heat`, `ignite`, `fuel`
- **Cleaning:** `absorb`, `filter`
- **Signalling and records:** `noise` (e.g. a whistle, for lures; see P-SC-04), `record`,
  `read` (text), `time`
- **Consumables:** `nutrition`, `hydration`, `medical.*`

**Actions (walkthrough G06) check capabilities against materials:**

- **Sawing drywall** needs `saw ≥ drywall.hardness`. **Time** = volume ÷ (capability − hardness
  margin), plus a skill modifier.
- **Bare hands** are just the body's own capabilities (`hammer 1`, `pry 1`, `dig 1`…). "If
  you're even remotely capable" falls out naturally.

### 2.5 Reactions: what happens *to* it (the "ifImpacted" idea, generalized)

Each object resolves stimuli through reaction rules. Reactions are mostly derived from materials
and form, with authored overrides.

| Stimulus | Possible outcomes | Driven by |
|---|---|---|
| `onImpact` (energy) | none, dent, bend, crack, **shatter** (leaves `shards` with `pierce`), break at a joint, **deals blunt force** | toughness, hardness, rigidity, mass |
| `onHeat` (temperature and duration) | char, burn (emits smoke scent), melt, cook, explode (sealed containers or fuel) | flammability, melting point |
| `onWet` | soak, ruin (paper, electronics), rust over time, dissolve | water behaviour |
| `onCut` / `onPierce` | tear, sever part, leak (containers), bleed (flesh) | hardness vs capability |
| `onCrush` (weight from collapse or falls) | deform, break, burst | toughness, rigidity |
| `onAge` (the global clock) | spoil, rust, rot (emits `rot` scent), degrade | shelf life, material aging curves (ADR-0011) |

**Both sides of an impact are resolved with the same rules.** When a glass bottle (as the head
of a taped club) hits a zombie's arm:

- the bottle's `onImpact` decides whether it shatters (and becomes a jagged `pierce` weapon)
- the arm's `onImpact` decides the damage (walkthrough G05)

No item-pair logic is ever written.

### 2.6 Tags (namespaced, for generic queries)

| Namespace | Purpose | Examples |
|---|---|---|
| `use.*` | Functional intent, for UI hints and crafting queries | `use.fire_starter`, `use.first_aid`, `use.lockpick`, `use.lure` |
| `from.*` | Provenance, driving place and occupation generation | `from.office`, `from.kitchen`, `from.hospital`, `from.police` |
| `state.*` | Current condition (usually set by generation or reactions) | `state.charred`, `state.wet`, `state.bloody`, `state.rusted`, `state.spoiled` |
| `emits.*` | Scent and noise emission profiles | `emits.rot`, `emits.smoke`, `emits.human`, `emits.noise.loud` |
| `hazard.*` | Dangers | `hazard.toxic`, `hazard.flammable`, `hazard.sharp_debris` |
| `flavor.*` | World-building and aesthetic value | `flavor.readable`, `flavor.photo`, `flavor.sentimental`, `flavor.decorative` |
| `mat.*` | Auto-derived from composition | `mat.metal`, `mat.glass`, `mat.textile` |

**Crafting and process rules query capabilities and tags, never specific items.** For example:

- torch = `rod` + (`absorb` material) + `fuel` liquid, lit by `ignite`
- splint = rigid `rod` × 2 + `bind`
- water filter = `contain.liquid` + `filter` layers

Free-form **assembly** (features and joints) covers most "use things together" cases. **Process
rules** exist only for true transformations: cooking, boiling water, chemistry, sewing.

### 2.7 Units and representation

- **Integer SI internally:** g, mm, mL, °C × 10, seconds. Deterministic arithmetic across
  platforms (R8).
- **Qualities** are integer 0–10 scales, with the thresholds documented in the registry.

## 3. Environment-driven generation of mobs, rooms and containers

### 3.1 A mob is an output of its environment

- **Profile** = occupation/role (from building occupancy at the event moment) × time of day and
  day of week × season and weather × region.
- **Outfit slots:** underlayer (incl. socks, underwear), top, bottom, footwear, outerwear
  (coat), headwear, hands, accessories (glasses, watch, lanyard). Each is filled from the
  profile's weighted tables.
- **Carried and pocket contents:** phone, wallet (ID, cash, cards, **photo**), keys, work items
  (badge, pens, stethoscope, tools), bag or briefcase.
  - **Keys are real relationships.** They reference the mob's home or workplace doors through
    **generation addresses** (G01 D1.5), so a key found on a zombie can open a real door in the
    world.
- **Aftermath and clock effects:** the global clock (ADR-0011) degrades clothing (wear, blood,
  tears, a lost shoe) and rots contents (spoiled lunch).
- **Cheap until needed:** a mob record stores only its **profile ID and seed**. The full outfit
  and pockets are generated when the zombie is promoted: examined, killed or looted (G01 D1.3).
  Thousands of mobs cost nothing extra.

### 3.2 Places

- **Room programs** define, per room type:
  - furniture and fixtures (things in cells)
  - containers
  - **realistic item lists with quantity ranges** (e.g. kitchen: cutlery 20–40, pots 3–8,
    canned goods 0–30)
- **Lists are written with the detail budget in mind:** useful items, flavour, aesthetic, and
  aggregated clutter.
- **Aftermath passes** then modify everything: looting priority, spoilage, fire charring, damage
  (ADR-0009, ADR-0011).

## 4. How Claude authors content

1. **Registry first:** properties, features, capabilities, reactions and tags, with units and
   which systems read each.
2. **Materials table.**
3. **Archetypes by domain:** tools, kitchen, clothing, office, medical, construction, food,
   electronics, furniture, vehicles-as-objects…
4. **Profiles and room programs:** occupations, room types, building types, encampments.
5. **Process rules:** cooking, fire, water, medical.

Each batch passes:

- **Schema validation:** the registry is respected, no orphan properties.
- **Physical plausibility checks:**
  - mass consistent with volume × density (within tolerance)
  - edges only on materials that hold an edge
  - realistic dimension ranges
- **Purpose tests and playtests** (N001, N002).
- **Owner spot-checks** of samples from each batch.

Content has stable IDs (ADR-0002), and changes are logged.

## 5. Illustrative data (format sketch, not final)

```toml
[material.steel]
density = 7.85          # g/cm3
hardness = 6
toughness = "tough"     # dents/bends, rarely shatters
rigidity = 9
edge_holding = 7
water = "rusts"
struck_sound = "clang"

[archetype.steel_pipe]
shape = "rod"
dims_mm = [1000, 27, 27]          # instance variation: length 300-1500
composition = [{ part = "body", material = "steel", hollow = true }]
features = [
  { type = "grip",   hands = 2, girth_mm = 27 },
  { type = "socket", end = "far", maxAttachMass = 1500, maxAttachSize = 250,
    accepts = ["blade", "block", "sphere"], joints = ["tape", "cord", "wire", "weld"] },
]
tags = ["from.construction", "from.plumbing"]
# derived: hammer, pry, reach; onImpact: dent/bend at high energy

[archetype.kitchen_knife]
shape = "blade"
dims_mm = [300, 35, 2]
composition = [{ part = "blade", material = "steel" }, { part = "handle", material = "polymer" }]
features = [
  { type = "edge",  length_mm = 180, sharpness = 7 },
  { type = "point", sharpness = 7 },
  { type = "grip",  hands = 1, girth_mm = 25 },
]
tags = ["from.kitchen", "use.cutting"]

[profile.office_worker]
occupancy = ["office.weekday.day"]
outfit.top      = [["dress_shirt", 70], ["blouse", 30]]
outfit.outer    = [["suit_jacket", 50], ["blazer", 20], ["none", 30]]
outfit.bottom   = [["suit_trousers", 60], ["skirt", 20], ["chinos", 20]]
outfit.footwear = [["dress_shoes", 70], ["flats", 30]]
pockets         = ["phone", "wallet.office", "keys.home+car", "lanyard_badge"]
carried         = [["laptop_bag", 40], ["none", 60]]
```

Taping `kitchen_knife` into the `socket` of `steel_pipe` (tape joint) gives a spear:

- **reach:** about 1.2 m
- **pierce 7** from the knife's point
- **hammer** only if swung butt-first
- **joint strength** from the tape, which loosens with each hard `onImpact`

Nobody wrote a "spear" recipe.

## 6. Decisions for the owner

**All four were answered in [N010](notes/N010-name-attachment-joints-descriptions-brands.md):**

| Decision | Answer | Detailed design |
|---|---|---|
| D-CM1 | Physically plausible attachment, yes. The *attached* item carries an **attachment surface** and an **attachment strength** relative to its weight. | §7 |
| D-CM2 | Joints cost something and differ by method. Deep, but easy to understand at its base. | §7 |
| D-CM3 | Surface details generated from properties and their combinations, written as a **description**, not tag lists | §8 |
| D-CM4 | **No real brands.** Invented, believable brands with parent-company relationships; brand tags compose item names. | §9, [`brands.md`](brands.md) |

The original questions are kept below for the record.

- **D-CM1 — Free-form assembly.**
  - Question: may *any* physically plausible attachment be made (features plus limits), or only
    curated combinations?
  - **Recommendation: free-form**, with derived stats. Process rules only for true
    transformations.
- **D-CM2 — Joints cost something.**
  - Question: should attachments need consumables (tape, cord, screws) and sometimes tools, and
    can they wear out and fail in use?
  - **Recommendation: yes.** Realistic, and it makes duct tape and cord valuable.
- **D-CM3 — Seeing properties.**
  - Question: does the player see properties descriptively ("heavy", "keeps a sharp edge") and
    discover hidden ones by trying (does this charred radio work)?
  - **Recommendation: yes** (consistent with G04 D4.5).
- **D-CM4 — Real brands.**
  - Question: generic product names only ("cola", "painkillers"), or real-world brands?
  - **Recommendation: generic only.** Real brands raise trademark issues for an open-source
    release (ADR-0004 spirit) and date the setting. Generic, plausible fictional brands can add
    flavour.

## 7. Attachment and joints: "Load vs Hold"

### The base mechanic players learn

Every attachment has three things, shown plainly in the attach screen:

| Term | Meaning to the player | Shown as |
|---|---|---|
| **Load** | How hard the attached thing pulls on the joint. Heavier, longer, or swung hard means more load. | a number, or light / medium / heavy |
| **Hold** | How strongly the joint grips. It depends on the method, the surfaces, and how much fastener you use. | a number |
| **Set time** | How long until the joint reaches full Hold. Zero for tape and rope; minutes to hours for glue. | a duration |

The joint's **status** is simply Hold compared with Load:

- **Secure** (Hold ≥ 2 × Load)
- **Firm** (≥ 1.25 ×)
- **Wobbly** (≥ 1 ×)
- **Won't hold** (< 1 ×)

When attaching, the screen lists every method you have available, each with its predicted
status, how much it uses, and the time it takes. Everything deeper sits behind those three terms.

### Item side (the owner's two values)

These live on the *attached* item:

- **`attachSurface`:** what the item offers a joint.
  - *Kind:* smooth, rough, porous, fibrous, oily/wet, round, irregular.
  - *Contact area:* the size of the face that can be joined.
  - *Wrap girth:* whether cord can go around it.
  - Derived from material and form; can be overridden.
- **`attachStrength`:** the hold the item *requires*.
  - At rest: its weight.
  - In use: weight × lever distance from the joint × use intensity (carried < swung < striking).
  - Derived; can be overridden.
  - This sets **how much fastener** a joint needs, e.g. how much glue.

### Method side (fasteners are ordinary consumable items with a `fastener` feature)

| Method | Hold comes from | Works best on | Weak at | Set time | Needs | Wear |
|---|---|---|---|---|---|---|
| **Superglue** | Contact area | Small, smooth, hard parts | Porous or large parts; impact (brittle) | Seconds to minutes | — | Cracks under repeated impact |
| **Epoxy** | Contact area; fills gaps | Nearly anything rigid, irregular fits | — | 5 min to 24 h (by product) | Mixing | Very durable |
| **Wood glue** | Contact area | Porous and fibrous: wood, paper, cloth | Metal, plastic, glass | Hours | Clamping helps | Weak when wet |
| **Duct tape** | Wraps × tape width | Most dry surfaces, quick fixes | Wet, oily or hot; heavy loads | None | — | Loosens with impacts, heat and water; can be topped up |
| **Rope / cord** | Wraps × cord strength × knot | **Large, heavy, irregular** items | **Small items** (needs wrap girth); rigid precision | None | — | Forgiving under impact; rots slowly |
| **Wire** | Twists × wire gauge | Round and irregular, medium items | Very large loads | None | Pliers for full hold | Durable; can cut hands |
| **Zip ties** | Count × tie rating | Loopable shapes | Heavy impact | None | — | Snaps rather than loosening |
| **Screws / nails** | Count × material grip | Wood; drilled thin metal | Glass, stone; small or brittle items | None | Screwdriver, drill or hammer | Rigid and strong; may split wood |
| **Welding** | Weld length | Metal to metal only | Everything else | Minutes | Welder, power | Strongest; **noise, light, smoke scent** |
| **Sewing** | Stitch count | Cloth to cloth (patches, armour layers) | Rigid items | Time to sew | Needle and thread | Frays |

**Hold formula, conceptually:**

Hold = method strength × **surface match** × amount used, where:

- **surface match** comes from a method × surface-kind table (e.g. instant glue on porous wood is
  poor; epoxy on anything rigid is good)
- **amount used** is capped by what the geometry allows: glue and tape by contact area, rope by
  wrap girth and length, screws by material thickness

The game **suggests the amount needed** to reach *Secure* for the item's in-use `attachStrength`.
The player can use less to save supplies, and accept a weaker joint.

**Curing.** Glue joints start at about 10% Hold and rise to 100% over their set time. Swinging an
uncured spear risks losing its head.

**Wear.** Each use applies fatigue based on Load ÷ Hold and on the method's impact tolerance.
Environment matters: tape hates water and heat; wood glue hates water. Joints can be
**reinforced**, by adding more of the same or a second method, and their Holds combine.

**Removal.** Rope, wire, zip ties and screws come off, sometimes needing tools. Tape comes off and
is consumed. Glue and welds must be **broken apart**, using the reactions (§2.5), which may damage
the parts.

### Worked examples

| Build | Method | Result |
|---|---|---|
| Kitchen knife on a steel pipe (spear) | Duct tape, 6 wraps | Firm; Wobbly after a few hard thrusts; top up with more tape |
| Same spear | Cord | Wobbly: the knife tang is too small to wrap well (the owner's rope example) |
| Same spear | Epoxy, cured | Secure, but can't be undone, and ties you up for hours |
| Brick on a stick (crude maul) | Rope, many wraps | Firm: rope suits big, irregular items |
| Same maul | Superglue | Won't hold: too heavy, porous surface |

## 8. Descriptions generated from properties (D-CM3)

**Description rules** are content data: phrase rules keyed on property ranges and combinations,
grouped into sentence slots.

**Sentence slots, in order:**

1. what it is (the composed name, §9)
2. size and weight feel
3. material and feel
4. edges, points and handles
5. condition and state
6. assembly and joints
7. world-building (provenance, brand, notes)

Each slot has a maximum number of sentences, and rules have priorities, so the text stays short.

**Examples of rules:**

- density high for its size → "heavy for its size"
- `edge.sharpness` ≥ 7 and `edge_holding` ≥ 6 → "the edge is keen"
- joint status Wobbly → "the head shifts when you swing it"
- `state.charred` → "blackened and blistered by fire"

**Hidden properties** (does it still work, how sharp exactly) are described vaguely until
discovered, by using, testing or a skill check:

- before: "you're not sure it still works"
- after: "it crackles, but works"

**Numbers** stay optional in a detail view (G04 D4.5).

**Example output:**

> A kitchen knife bound to the end of a steel pipe. Heavy and long, awkward in tight spaces. The
> blade is keen, but the tape is peeling and the head shifts when you thrust. Soot streaks the
> pipe.

## 9. Brands and name composition (D-CM4)

- **No real brands.** A fictional brand universe of **parent companies → brands → product
  categories**, plus **store chains with own labels**. The starter set is in
  [`brands.md`](brands.md).
- **Brands are relationships, not strings.**
  - An item's `brand.*` tag links to a brand, and the brand links to its parent company.
  - Realistic association follows: the same parent makes the cereal and the bread.
  - Region and store context weight the choice: a supermarket chain stocks its own label; an area
    favours certain brands.
- **Brands can matter mechanically.** A brand has a **tier** (budget / standard / premium) that
  shifts relevant properties: premium tape has more Hold; budget batteries hold less charge.
  Brands also carry description flavour (slogans, packaging colours used in the tile palette).
- **Name-altering tags.** Any tag in the registry can declare a `name_part`: a slot, a template
  and a precedence. The display name is assembled from slots, so a tag appears in the name **just
  by existing**:

  | Slot | Examples |
  |---|---|
  | condition prefix | charred, wet, rusted, bloody |
  | **brand** | Fennick Hollow, Tenacor Pro |
  | variant | Honey Oat, Heavy-Duty |
  | base name | cereal, duct tape |
  | suffix | (half full), (cured), (wobbly) |

  Example: "charred Fennick Hollow Honey Oat cereal (half full)". Lists show a short form, and
  the full name appears on inspect.
- **Authoring rule:** when Claude authors an archetype that would realistically carry a brand, it
  adds a handful of brand options from categories that fit.
- **Every brand name gets a conflict check** (web and trademark search) before it ships.
