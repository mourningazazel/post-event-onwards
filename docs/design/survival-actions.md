# Survival actions: what a real person would do, and what items need to support it

Origin: owner note [N011](notes/N011-survival-actions-and-world-catalog.md). Builds on
[`content-model.md`](content-model.md): materials → form → features and joints → capabilities →
reactions → tags. The vocabulary defined here is formalized in `content/registry/`.

**Method:**

1. List what a real survivor would plausibly do, trimmed to what makes sense in a turn-based grid
   game.
2. Name the **engine mechanics** each action needs, and the **item data** those mechanics read.
3. Prove coverage with **worked chains** (§4), which also become permanent feasibility tests in
   `tests/content/chains/`.

**Leanness rule (N011):**

- An item needs its own archetype only if it **behaves differently** or has **distinct gameplay
  value**.
- Flavour and cosmetic variety come from generation-time **modifiers**: variants, states, details,
  contents and brands, attached through tags.

## 1. Cross-cutting mechanics the engine must provide

Every survival action below is built from these systems. Items never carry bespoke logic; they
carry data these systems read.

| # | Mechanic | What it does | Main item data it reads |
|---|---|---|---|
| M1 | **Substances and containers** | Liquids, granular matter and gases held by containers. Pouring, filling and transferring. | substance properties; `container` feature (capacity, opening, seal, compatibility) |
| M2 | **Tile surface layer** | Substances and debris lying on a tile: puddles, spills, shards, ash. Spread, soak-in and evaporation. | substance viscosity and volatility; terrain porosity |
| M3 | **Fire** | Ignition, burning tiles, items and creatures, spread by fuel and wind, smoke scent, extinguishing | material and substance flammability, fuel value, ignition temperature class |
| M4 | **Throwing** | Range, accuracy, landing tile, impact energy, breakage on landing | mass, dims, shape aerodynamics, reactions (`onImpact`) |
| M5 | **Disassembly and salvage** | Taking objects apart into useful parts | `salvage` lists on composition parts: yields, tools, time |
| M6 | **Divide and cut to size** | Cutting a length off hose, rope, pipe or planks; tearing cloth into strips | `divisible` property: mode, minimum piece, required capability |
| M7 | **Assembly and joints** | Attaching items (Load vs Hold, §7 of content-model) | features, `attach_surface`, `attach_strength`, fasteners |
| M8 | **Tool actions** | Cut, saw, pry, dig, drill, hammer… as capability vs material resistance → success and time | capabilities; material hardness and toughness |
| M9 | **Power** | Batteries, generators, grid power per neighbourhood; devices draw power | `power_source`, `power_use`, battery charge, voltage class |
| M10 | **Access and locks** | Locked doors and containers; keys, lockpicking, forcing, breaking glass | `lock` feature (tier, key link); openings' materials |
| M11 | **Light** | Emitted light radius and duration; darkness for the player | `light` capability, fuel or battery use |
| M12 | **Temperature, wetness, weather** | Body heat, clothing insulation, getting soaked, drying | `insulation`, `waterproof`, `windproof`, `absorbs_water` |
| M13 | **Noise events** | Actions and objects emit sounds that hearing Dead react to | material `struck_sound`, `use_noise`, `alarm` feature |
| M14 | **Scent** | Emission, absorption and masking (scent-mobs.md) | `emits.*` profiles, `absorbs_scent`, substances' scent |
| M15 | **Consumption** | Eating, drinking, medicine; spoilage over the clock | nutrition, hydration, perishability, medical effects |
| M16 | **Body coverage and armour** | Worn layers protect body parts (bite matters most) | `wearable` feature: slots, layer, coverage; armour per damage type |
| M17 | **Blocking and barricades** | Blocking openings with furniture, boards, cars | mass, dims, `boardable` openings, joint Hold |
| M18 | **Traps and triggers** | Tripwires, pits, alarm triggers, snares | `trigger` feature; composition of cord + object |
| M19 | **Climbing and anchoring** | Fences, ladders, trees, ropes from windows | fixtures' `climbable`; `anchor` features; rope strength |
| M20 | **Carrying and hauling** | Encumbrance, bags, carts, dragging | container features, `wheels`, mass |
| M21 | **Knowledge objects** | Maps, notes, manuals, labels | `readable` feature: kind, coverage, text source |
| M22 | **Transformation processes** | Cooking, boiling, drying, sharpening, mixing (limited and safe), charging | process rules keyed on capabilities and tags |

## 2. The action catalogue

Each action lists the **mechanics** it uses and the **item data** it reads.

- **Features** are *italic*, e.g. *tube*.
- **Capabilities** are `code`, e.g. `cut`.
- **Tags** are `ns.name`.

### 2.1 Getting around

| Action | Mechanics | Item and world data |
|---|---|---|
| Walk, run, sneak, crawl | M13, M14 | footwear: `use_noise` modifier, grip on terrain footing |
| Climb a fence, wall, tree or ladder | M19 | fixture *climbable* (difficulty, height); gloves: `grip`; load carried |
| Drop down or jump | falling (P-WO-06) | height; footwear cushioning |
| Descend by rope | M19, M7 | *anchor* (bed frame, radiator, pipe; max load); rope `bind` strength, length |
| Squeeze through a gap, crawl through a vent | size check | creature size vs opening dims |
| Push, pull or drag furniture and bodies | M20 | mass, footing, *handle* |
| Break through a door, window or wall | M8, M10, M13 | opening material; `pry`, `hammer`, `chop`, `saw`; loud outcomes |
| Dig | M8 | `dig` and `scoop` capability vs soil hardness; volume |
| Wade or swim | M12 | depth; clothing `absorbs_water` (weight when wet) |

### 2.2 Staying hidden (scent and noise discipline)

| Action | Mechanics | Item data |
|---|---|---|
| Mask your scent: gore, smoke, strong perfume, bleach | M14, M1 | substances with `emits.*` profiles and dominance; `applicable_to_body` |
| Wash off scent | M1, M14 | water + `use.soap`; clothing `absorbs_scent`, washable |
| Change into clean clothes | M16, M14 | worn items carry absorbed scent |
| Wait for rain or wind | weather | — |
| Muffle gear | M13, M7 | cloth or foam wrapped on noisy items (`cushion`) |
| Close doors behind you | M14, M17 | doors reduce scent flow |

### 2.3 Distraction and lures

| Action | Mechanics | Item data |
|---|---|---|
| Throw a bottle or can to make noise elsewhere | M4, M13 | throwable; `onImpact` breaks with `struck_sound` |
| Set an alarm clock, phone or radio to go off | M9, M13 | *alarm* feature (loudness, timer), power |
| Trigger a car alarm | M13 | vehicle *alarm*, car battery charge |
| Fireworks or flares (US holidays) | M3, M13 | `hazard.flammable`, very loud noise, light |
| Scent lure: meat, blood, gore | M14 | `emits.flesh`, `emits.blood` |
| Repellent scent (N004) | M14 | `emits.*` repellent channels (e.g. smoke, strong chemical, dead-dead rot) |

### 2.4 Fighting and defending

| Action | Mechanics | Item data |
|---|---|---|
| Melee: bash, cut, chop, stab | M8 (vs body), M4 | derived `hammer`, `cut`, `chop`, `pierce`; `reach`; balance; *grip* hands; durability |
| Improvise weapons | M7 | *socket* + head; joints (P-IT-01, P-IT-07) |
| Throw weapons (bricks, knives, firebombs) | M4 | throwable, aerodynamics |
| Firearms (US setting) | ammo slots, M13 | *slot* ammo type, magazine capacity; `use_noise` **extreme**; scarce |
| Bows and crossbows (quiet ranged) | M4-like projectiles | draw weight, projectile *slot*; quiet |
| Wear armour (bite protection is critical) | M16 | coverage per body part; `armor.bite`, `armor.cut`, `armor.blunt`; encumbrance; noise |
| Improvise armour (taped magazines, leather, carpet) | M7, M16 | sheet-shaped items + `bind`/`adhere` → *wearable* |
| Barricade doors and windows | M17, M7 | heavy furniture mass; planks + nails + `hammer`; *boardable* |
| Traps: pits, spikes, tripwire alarms, caltrops | M18, M8 | cord + cans (noise trigger); sharp debris (`hazard.sharp_debris`) |
| Use terrain: tripping hazards, chokepoints, fire lines | footing, M3 | rubble, debris, fuel spills |

### 2.5 Fire and heat

| Action | Mechanics | Item data |
|---|---|---|
| Light a fire | M3 | *ignition* source (matches: strike, uses, **fails when wet**; lighter: fuel charges; ferro rod; battery + steel wool) |
| Tinder, kindling, fuel | M3 | flammability class, fuel value; `state.wet` suppresses |
| Contain fire (pit, grill, stove, barrel) | M3 | `heat_resistant`; fire-safe container |
| Cook and boil | M22, M3 | cookware: `heat_resistant`, `contain.liquid`; food `cookable` |
| Warm up and dry off | M12, M3 | — |
| Firebomb (improvised incendiary) | M1, M4, M2, M3 | breakable container + flammable liquid; ignition (§4.1) |
| Burn a barricade, corpse pile or nest | M3 | flammability; smoke scent |
| Put out fires | M3, M1 | water (not for grease or fuel fires), extinguisher, sand, smothering blanket |
| Fuel explosions (propane tanks, aerosol cans in fire) | M3 reactions | `pressurized` + `onHeat → burst`; propane substance |

### 2.6 Water

| Action | Mechanics | Item data |
|---|---|---|
| Find water: taps (if water works), **water heater tank**, toilet cistern, bottled, rain, rivers | M1, utilities | fixture containers with `contents` presets; utility state |
| Carry and store water | M1 | `contain.liquid`, sealable, capacity |
| Purify: boil, filter, tablets, bleach | M22 | `heat_resistant` pot + fire; `filter`; `purify` doses; water quality states |
| Collect rain | M1, weather | open container, tarp (sheet) + container |

### 2.7 Food

| Action | Mechanics | Item data |
|---|---|---|
| Scavenge, judge spoilage | M15 | perishability class × storage × clock (power matters for fridges) |
| Open cans and packaging | M8 | `needs_opening` + opener capability (`can_open`, or `pry`/`pierce` slowly and messily) |
| Cook | M22 | `cookable` raw → cooked; heat source |
| Ration, carry | M20 | kcal density, mass |

### 2.8 Medical

| Action | Mechanics | Item data |
|---|---|---|
| Stop bleeding, bandage | M15 | `bleed_stop`, sterile; improvised: cloth strips (M6) |
| Disinfect | M15 | `disinfect` (alcohol, antiseptic, boiled water) |
| Splint a fracture | M15, M7 | rigid rod ×2 + `bind` (chain §4.6) |
| Painkillers, antibiotics | M15 | medical effect, doses, expiry over the clock |
| Treat burns, suture | M15 | `use.burn_care`; needle + thread (`sew`) |

### 2.9 Tools, crafting and repair

| Action | Mechanics | Item data |
|---|---|---|
| Take apart appliances, furniture, vehicles | M5 | parts with **salvage yields** (hose, copper tube, wire, motor, sheet metal, screws, springs) and required capability |
| Cut to length, tear into strips | M6 | `divisible` (length, sheet, count) |
| Saw, drill, pry, hammer, weld, sew | M8 | capability magnitudes |
| Sharpen blades (whetstone, or concrete kerb) | M22 | `sharpen` capability on stone surfaces |
| Repair with tape, glue, wire | M7 | joints on damaged parts |
| Siphon liquids | M1 | *tube* (length, inner diameter) + source above destination (gravity) |
| Make containers (cut bottle tops, line a box) | M6, M1 | — |

### 2.10 Access and scavenging

| Action | Mechanics | Item data |
|---|---|---|
| Search containers, rooms, bodies | G02 detail levels | containers, promoted Dead |
| Unlock with found keys | M10 | keys linked to doors by generation address (P-IT-06) |
| Pick locks | M10 | *lock* kind → tier band → picking `unlock`/`difficulty` (security); `use.lockpick` tool gated by `tool_min_skill`; a gradient with tiered steps (N015) |
| Force doors, windows, lockers, vending machines | M8, M10, M13 | material vs `pry`/`hammer`; noise |
| Break glass | M8, M13 | glass `onImpact → shatter`, very loud; shards on tile |
| Drain a vehicle fuel tank (siphon, or puncture the tank) | M1, M8 | vehicle *fuel tank* container (gasoline or diesel); `pierce`/`drill` |
| Pull a car battery | M5, M9 | 12 V battery: charge, mass (heavy) |

### 2.11 Power, light and devices

| Action | Mechanics | Item data |
|---|---|---|
| Run devices on batteries | M9 | *slot* battery types; charge |
| Run a generator | M9, M13, M14 | fuel use, output, **loud**, exhaust (smoke scent; indoor danger) |
| Car battery + inverter or 12 V devices | M9 | voltage class compatibility |
| Flashlights, headlamps, lanterns, candles, glow sticks | M11 | light radius, duration, fuel or battery |
| Radios (emergency broadcasts as flavour; no NPCs) | M9, M21 | — |

### 2.12 Rest, shelter, weather

| Action | Mechanics | Item data |
|---|---|---|
| Sleep safely | needs (G10), M17 | bed or sleeping bag comfort, insulation |
| Keep warm and dry | M12 | clothing insulation, waterproof, wind; blankets; fire |
| Set up a shelter | M17, M7 | tarp (sheet) + cord + anchors |

### 2.13 Carrying

| Action | Mechanics | Item data |
|---|---|---|
| Pack bags, wear load-bearing gear | M20, G03 | containers, straps, capacity (G03 D3.1) |
| Haul with carts, wagons, wheelbarrows | M20 | `wheels` feature: capacity, terrain limits, **rattle noise** |
| Bicycles | ❓ (§5) | exist as objects; rideability to be decided |

### 2.14 Knowledge and navigation

| Action | Mechanics | Item data |
|---|---|---|
| Read maps, atlases, transit maps | M21 | *readable* kind `map`, coverage area |
| Read survivors' notes | M21 | note text from generation fragments (P-WO-14) |
| Mark your own map, write notes | M21 | pen or pencil + paper |
| Compass, watch | M21 | `navigate`, `time` |

### 2.15 Skills, success chance and failure ([N012](notes/N012-trainable-expert-skills.md), [N014](notes/N014-the-dead-ai-content-skills-mood-drugs.md))

Skills run **1–100** (`content/registry/skills.toml`). Anchors: **1** never heard of it ·
**10** can use it fine · **30** skilled · **50** done it for ten years · **51–100** advanced
through savant, beyond any real person. They train faster than reality, through use and through
**reading** manuals and books (`teaches`).

**Every action shows a percentage chance to succeed**, and what failure costs. The model is
data on the action, never a preset range:

| Field (per skill requirement) | Meaning |
|---|---|
| `skill` | which skill |
| `unlock` | below this level the chance is **0%** ("you would not know where to start") |
| `difficulty` | at this level and above the chance reaches the action's `ceiling` |
| `ceiling` | the best possible chance (default 95; **never 100**) |

Between `unlock` and `difficulty` the chance rises **linearly** from 0 to `ceiling`. An action
may list **several skills**; each shows its own percentage and the aggregate is their product.
Example: *make pharmaceuticals* needs chemistry {unlock 60, difficulty 90} and medicine
{unlock 30, difficulty 50}; a chemist 75 / medic 45 sees `chemistry 48% · medicine 71% · overall
34%`.

**Failure outcomes** are weighted entries on the action (`on_fail`): `failed` (time and maybe a
consumable lost), `damaged_components`, `bad_event` (a fire, a cut, a misfire, a noise). Being
far below `difficulty` shifts weight toward the worse outcomes; above `difficulty` a failure is
always plain `failed`.

**Beyond the ceiling, skill still pays:** each point above `difficulty` makes the action
faster (time ×0.985 per point), quieter (one noise step per 20 points) and lets skill
**substitute for tool quality** (`tool_substitution`, capability points per skill point). A
savant metalworker cuts a padlock with a file and patience; a real person needs the hacksaw.

**Tools gated by skill.** An item's capability may carry `min_skill`, e.g. the lockpick set's
`lockpick` needs security 10: below it the tool cannot be used at all, not merely badly.

**Expert-only actions** (`content/registry/actions.toml`) are simply actions with a high
`unlock`: `friction_fire` survival 45, `climb_sheer_wall` athletics 60, `crack_safe_by_feel`
security 75, `field_surgery` medicine 50, `weld_join` metalworking 15. The `expert` lists in
`skills.toml` describe what each band makes possible so content and the UI speak the same
language.

**Design consequence:** content records the *physical* requirement (capability vs material);
skill is a separate axis that can partly pay it, and the percentage the player sees is the
honest combination of both.

**Locks as a gradient (N015).** Each **lock kind** carries a tier band (latch 1–2, knob 2–4,
deadbolt 4–7, padlock 3–8, cam 2–5, combination 8–12, electronic 6–10, bar/extra locks add
resistance) and the tier sets both the picking difficulty (security `unlock = tier × 5`,
`difficulty = tier × 5 + 20`) and the bashing resistance. **Which lock a thing gets is
weighted** by its building type, its container type, and any generative marker that says it
should be secure (armory, pharmacy, gun store, safe, evidence locker, server room), so a
suburban interior door is a latch and a pharmacy back door is a deadbolt with a bar.

**Books (N015).** Libraries are rare and valuable. Books are **generated in thousands**,
organised by library **section** (trades, sciences, medicine, cooking, outdoors, fiction…), each
with:

| Field | Meaning |
|---|---|
| `teaches.skill` | which skill, or none (fiction, flavour) |
| `teaches.difficulty` | 1–100; the level the book is written for |
| `teaches.read_time_min` | game minutes to finish; time passes and the horde builds |
| `teaches.gain_chance_pct` | chance of **one** point on finishing; rises with difficulty and length |
| `teaches.window` | gate width: below `difficulty − window` you cannot follow the text; at or above `difficulty + window` it is too simple to teach |

A book gives **at most one point, once**; the instance records that it has been read. Grinding
is expensive by design: time, danger, and diminishing books at your level.

### 2.16 Drugs, maps, guns, bikes, chemistry (N014)

| Action | Mechanics | Item and world data |
|---|---|---|
| Take pharmaceuticals, narcotics, psychedelics | M15, Mood/Sanity (G04) | `drug` {class, dose, onset, duration, addiction, effects}; **psychedelics change visuals and sanity only**; paraphernalia as items; pharmacies are **heavily locked** and urban |
| Read a map | M21, knowledge (G09) | *readable* kind `map`, `coverage`; **rare**, found where it makes sense (`city_hall`, `transit_stop`, gas station racks); marks **general locations** of major places even before generation, and generation later lands inside the mark |
| Ride a bicycle | ride (N013) | 2 tiles per turn; terrain footing; **crash** on obstacle or bad footing: a `mechanical` sound, and a body-part injury roll (fracture, concussion) |
| Use firearms | ranged (pending vocabulary), M13 | brand-generic or fictional brands with **real specifications**; **few ammo types** (9 mm, .38, 12 ga, .308, 5.56 and little else); `use_noise` extreme; gun stores: **giant selection**, an *alarm* that fires only with power, ammunition thinned by a week of looting, high-end weapons rare or impractically loud |
| Chemistry | M22 | outcomes stay abstract (fuel, soap, bleach dilutions, repellents, medicine at high skill) but recipes name **real ingredient classes**; multi-skill chains at the top end (chemistry + medicine for pharmaceuticals) |
| Explosives | M22, M3, M4 | **generic ingredients** (oxidiser, fuel, container, initiator), minimal blast model: a radius, blunt and heat effects, a very loud `mechanical` sound; no real recipes |
| Hear the crows | ambience | periodic **text** only; there are no crows and never a creature |

## 3. The vocabulary this implies

### 3.1 Properties (item-level data; units are integers)

- **Physical:** `mass_g`, `dims_mm` [length, width, height], `shape`, `composition` (parts →
  material, role, salvage), `balance`, `hollow`.
- **Assembly** (content-model §7):
  - `attach_surface` {kind, contact_area_mm2, wrap_girth_mm}
  - `attach_strength` (derived)
- **Divisible:** {mode: length | sheet | count, min_piece, needs capability}.
- **Throwing:** `aerodynamics` (poor | fair | good); derived range.
- **Containers:** see the *container* feature.
- **Power:**
  - `power_source` {voltage_class, capacity_wh, rechargeable}
  - `power_use` {voltage_class, watts}
- **Clothing and armour:**
  - `wearable` {slot, layer, coverage[], insulation_clo10, waterproof, windproof}
  - `armor` {blunt, cut, pierce, bite} 0–10
  - `encumbrance`
  - `absorbs_water`, `absorbs_scent`
- **Consumables:**
  - food: `kcal`, `hydration_ml`, `perishable` {class}, `needs_opening`, `cookable`, `servings`
  - medical: {bleed_stop, disinfect, pain, splint, sterile, doses}
- **Light:** `light` {radius_m, beam, duration_min}.
- **Noise:** `use_noise` (quiet | normal | loud | very_loud | extreme).
- **Durability:** `durability` (use-wear rate); the instance holds `condition`.
- **Pressure:** `pressurized` (burst risk under heat).
- **Heat:** `heat_resistant` (can go on a fire).

### 3.2 Features (points on an object)

Existing from content-model §2.3: *grip*, *socket*, *edge*, *point*, *striking_face*, *mouth*,
*lid*, *pocket*, *strap*, *slot*, *mount*.

New from this analysis:

- *container*: capacity, compat, seal
- *tube*: inner diameter, length
- *ignition*: method, charges
- *alarm*: loudness, timer
- *lock*: tier, key link
- *anchor*: max load
- *climbable*: difficulty, height
- *boardable*: an opening that accepts boards or blocking
- *trigger*: tripwire or pressure
- *wheels*: load, terrain
- *readable*: kind, coverage
- *fuel_tank*: a container that also feeds a device

### 3.3 Capabilities

Existing: `cut`, `chop`, `saw`, `pierce`, `hammer`, `pry`, `dig`, `scoop`, `drill`, `scrape`,
`reach`, `grip`, `bind`, `adhere`, `fasten`, `weld`, `sew`, `ignite`, `heat`, `light`,
`contain.*`, `seal`, `insulate`, `waterproof`, `cushion`, `filter`, `noise`.

Added:

- `can_open`
- `sharpen`
- `purify`
- `siphon` (derived from *tube*)
- `navigate`, `time`
- `extinguish`
- `lockpick`

### 3.4 Tag namespaces (content-model §2.6, plus)

- `cat.*`: catalogue category for loot tables and UI (`cat.food.canned`, `cat.tool.hand`,
  `cat.clothing.top`)
- `slot.*`: wear slots
- `room.*` and `fixture.*`: world objects
- `brand.*`: brand relationships

## 4. Worked chains (each becomes a permanent feasibility test)

### 4.1 The owner's firebomb

| Step | Player does | Mechanic | What makes it possible (item data) |
|---|---|---|---|
| 1 | Empties the milk jug | M1 pour | `milk_jug`: *container* (HDPE, 3.8 L, *mouth*, *lid*); milk is a substance and pours out |
| 2 | Disassembles a refrigeration unit | M5 | `refrigerator` composition includes a **hose/tubing** part with a salvage yield; needs `fasten`-type tool (screwdriver) or `pry`; time |
| 3 | Cuts a length of hose | M6, M8 | `hose`: `divisible` length mode, needs `cut ≥ rubber.hardness`; knife qualifies |
| 4 | Siphons gasoline from a car | M1 | `car` has *fuel_tank* holding `gasoline`; hose *tube* (inner diameter, length ≥ reach); jug below the tank (gravity) |
| 5 | Fills the jug | M1 | jug *container* compat: HDPE holds gasoline (short term); capacity limits the amount |
| 6 | Throws the jug in front of the Dead | M4, M2 | throwable (mass ~2.8 kg full, poor aerodynamics → short range); HDPE `onImpact` at that energy → **split and spill** (lid off or cracked); gasoline goes onto the tile surface layer and spreads by viscosity |
| 7 | Lights a match | M3 | `matchbook`: *ignition* (strike method, charges, **fails if wet**) |
| 8 | Throws the match onto the tile | M4, M3 | lit match is throwable; gasoline's very low flash point → ignition on contact |
| 9 | The tile burns, and so do the Dead on it | M3 | the burning tile's fuel amount sets duration; creatures on it get `onHeat`; smoke scent emitted; spreads to flammable neighbours |

### 4.2 Noise lure with an alarm clock

Find an alarm clock (*alarm*, battery *slot*) → take batteries from a remote (*slot* compat AA) →
set the timer → place it in a building across the street → leave → it rings (M13) → hearing
Dead redirect, and their agitation pulls crowds (P-EN-02).

### 4.3 Barricading a front door

Push the sofa and bookshelf against the door (M17: mass and footing) → pry planks off a fence
(`pry`, salvage) → nail planks across the door frame (*boardable*, nails `fasten` + hammer
`hammer`, joint Hold) → the barricade's strength = combined blocking mass + joint Hold vs crowd
push.

### 4.4 Safe water from a water heater

Open the water heater drain valve (fixture *container* ~150 L; drain needs `fasten`/`grip`) →
fill pots (*container*) → boil on a camp stove (`heat`, `heat_resistant` pot, propane) or add bleach
drops (`purify`, dose) → water quality "treated".

### 4.5 Improvised spear

Kitchen knife + broom handle (*socket*) + duct tape (fastener) → Load vs Hold predicts Firm (P-IT-07).

### 4.6 Splinting a broken arm

Two rulers or sticks (rigid rods, `divisible` sticks) + a shirt torn into strips (M6 sheet mode →
`bind`) → splint effect (medical `splint`).

### 4.7 Rope escape from an upper floor

Tie a garden hose or bedsheets knotted into rope (M7 joints between sheets, `bind`) to a radiator
(*anchor*) → descend (M19; the anchor's max load and the rope's strength vs body mass + load).

### 4.8 Scent masking before a supermarket run

Kill one of the Dead → its gore (`emits.rot` substance) → apply to your clothes (`applicable_to_body`) → your
human emission is dominated (scent dominance) → cross the car park with fewer contacts (P-SC-03).

### 4.9 Car battery lamp

Pull the car battery (M5: `fasten` tool, heavy) → 12 V work light or 12 V → USB adapter (voltage
class match, M9) → light for nights inside a barricaded room.

## 5. Open questions for the owner

*Answered in [N013](notes/N013-bikes-guns-bites-locks-animals-words.md) and refined in
[N014](notes/N014-the-dead-ai-content-skills-mood-drugs.md):*

- **Bicycles:** rideable at 2 tiles per turn, with an extra stopping turn; crashes make noise and
  injure limbs (N014).
- **Firearms:** common; noise is the cost; real specifications, fictional brands, few ammo
  types; gun stores alarmed when powered (N014).
- **Bites:** don't infect.
- **Animals:** all dead; crows exist only as ambient text (N014).
- **Chemistry:** abstract outcomes grounded in real ingredient classes; **explosives exist**
  with generic ingredients and minimal mechanics (N014 supersedes the "no explosives" line).

The original questions follow.

1. **Bicycles:** rideable? It's realistic, but it would soften "walking-only travel" (P-WO-03).
   Options: not rideable; rideable but loud and terrain-limited; or rideable only on clear roads.
2. **Firearms:** realistic US availability (common in homes, gun shops, police), with their
   extreme noise as the balance? Claude assumes yes, with realistic scarcity of ammunition.
3. **Bites:** does a bite infect the player (turning, D4.6), or just injure? The armour value
   `armor.bite` matters either way.
4. **Chemistry:** keep it to safe, abstract outcomes (fire, smoke, toxic fumes as a hazard, bleach
   purifying water) and **no explosive recipes**. Claude recommends this.
5. **Animals** (pets, wildlife after a year): in or out?
