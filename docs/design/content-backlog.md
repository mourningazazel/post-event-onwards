# Content backlog: reconciliation decisions, vocabulary proposals, open questions

Collected after the first full content authoring pass (N011). Seven domain agents authored in
parallel; Claude reconciled their output.

**State at reconciliation:**

| Content | Count |
|---|---|
| Item archetypes | 402 |
| Modifiers | 144 |
| Detail tables | 78 |
| Rooms | 60 |
| Loot tables | 71 |
| Buildings | 56 |
| Occupant profiles | 26 |
| Outdoor sets | 16 |
| Settings | 9 |
| **Derivation expectations** | **365** |
| **Feasibility chains** | **102** (including negative controls) |

Lint is clean and all tests are green.

## 1. Decisions made during reconciliation (technical; Claude's call)

| Decision | Why |
|---|---|
| **Trademark-safe ids:** `wd40_can` → `penetrating_oil_can`, `taser` → `stun_gun`, `taser_cartridge` → `stun_gun_cartridge`. Descriptions reworded to avoid Styrofoam, Plexiglass, Kevlar and "crescent" wrench. | Licensing care (ADR-0004, N010) |
| **Slicing cap:** a derived edge `cut` tops out at 5 (`derive.SLICE_CAP`), so no knife parts steel, glass or stone. Shearing tools (bolt cutters 7, wire cutters 6, tin snips 6) author their cut explicitly. | A domain B report: sharpness alone let tool-steel knives cut steel |
| **Wieldability gate:** items over 12 kg, or fixed, derive no hammer or pry (`derive.WIELD_MAX_G`) | A domain B report: drums and generators derived hammer 9–10 |
| **Tearing needs a hand-tearable fabric** (`divisible_by_hand` predicate). New action `cut_strips` for tough fabrics with a blade. | A domain C report: denim was tearable by hand |
| **`aluminum_body` modifier:** mass factor 0.4 → 0.6 | A domain E report: aluminium tools aren't that light |
| **Specialty store floors kept** as separate rooms (liquor, books, sporting goods, auto parts, clothing) rather than per-building stock overrides | Simpler generator; honours the value filter |
| **A bare item id in `contains`** means exactly one of that item. Use a loot table for quantities. | W1 question |
| **Stoves:** electric ranges need grid power or a generator; gas ranges work while gas lasts, lit with a match (content already models this; the engine enforces it) | Realism (N004) |

## 2. Additions beyond the planned ids (all passed the value filter)

- **Tools and hardware:** `tin_snips`, `hatchet_head`, `tool_handle` (rehafting), `steel_wool` (the 9 V ignition trick)
- **Medical:** `elastic_bandage`, `burn_gel`
- **Fire:** `dryer_lint` (tinder, salvaged from dryers)
- **Street and vehicles:** `fuel_pump` (the forecourt), `semi_trailer`
- **Weapons:** `ammo_38spl` (revolver), `stun_gun_cartridge`
- **Rooms:** `clothing_floor`, `sporting_goods_floor`, `auto_parts_floor`, `liquor_floor`, `bookstore_floor`
- **Loot:** `kitchen_counter`, `living_room_surfaces`, `bookshelf_home`, `dining_sideboard`,
  `laundry_hamper`, `home_safe`, `grocery_cold_case`, `grocery_freezer_case`, `gym_equipment`,
  `storage_unit_extras`, `minibar`, `luggage`

## 3. Vocabulary proposals from the domains (not yet registered)

These wait until the systems that read them are designed. Until then, the stand-ins in content are
commented in place.

| Proposal | From | Needed for |
|---|---|---|
| `feature.ranged` {projectile_tag, effective_range_m, accuracy, action, reload_s, draw_weight_n}; actions `shoot`, `reload` | E | Firearms and archery (combat design, G06/G08) |
| `feature.shield` {coverage, block} | E | Riot shields |
| `feature.remote` {range_m, target} | C, D | Car-key panic button, walkie-talkie triggers (M13 lures) |
| `feature.crank` / `power_source.self_charge` | D | Crank radios, solar |
| `feature.utility_outlet` {water, gas, power} | E | Hydrants, taps, gas lines (N007 utilities) |
| Multiple pockets per item | C | G03 containment |
| Capabilities `listen`, `spot`, `lift`, `absorb` | C, E, B, A | Perception, jacks, spill clean-up |
| Voltage `dc_tool` (18–20 V packs) | B | M9 power |
| Materials `dry_chemical`, `mineral_wool`, `fiberglass`, `ferrocerium`, `soap`, `hay_straw`, `pyrotechnic`, `epoxy_resin`, `gold`/`silver` | A, B, C, D, E | Accuracy of derivations |
| Substances `natural_gas`, `penetrating_oil`, `grain`, `saline`, `two_stroke_mix`, `concrete_mix` | A, B, C, E | M1, M3, utilities |
| Tags `use.keycard`, `from.library`, `hazard.snag`, `hazard.engulfment`, `state.locked`, `state.boarded` | C, D, E | M10, M17, M18, generation |
| Ignition methods `solar` (lenses), and a `needs` partner param (steel wool + battery) | C, B | M3 |
| Predicate `joint_status_min`; test op `not_has` | B, D | Load vs Hold negative controls |
| Part-level reactions (a glass front shatters while the steel body dents) | E | Reactions |
| Food field `stimulant`; substance `cookable`/`raw_risk` | A | M15 |
| Garment `fit` property (child sizes) | C | M16 |
| Occupant `chance_pct` and the outfit slots `full_body`/`eyes` in the schema | W2 | Profiles (already used; formalize) |

## 4. Questions that need the owner (gameplay-facing)

1. **Bicycles:** rideable? They're currently pushable haulers only. (Survival-actions §5)
2. **Firearms:** confirm realistic US availability with extreme noise and scarce ammunition as
   the balance. (Domain E modeled it this way.)
3. **Pepper spray and stun guns vs the dead:** any effect? The realistic answer is probably
   "little to none", since the dead feel no pain.
4. **Bites:** do they infect the player (G04 D4.6), or just injure? Armour already rates
   `bite` protection.
5. **Worn backpacks:** should they protect your back from bites (coverage on the torso)?
6. **Lock tiers:** Domain D set lock tier ≈ the security skill needed to pick it (1 latch …
   7 commercial … 15 safe). Is that the scale you want?
7. **Animals** (pets, wildlife after a year): in or out?
8. **"Dumpster":** it's a registered trademark but widely used as a generic word. Keep the word,
   or use "waste container"?
