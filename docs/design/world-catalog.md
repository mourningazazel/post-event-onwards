# World catalogue: what a US-like world is made of (content plan and id contract)

Origin: owner note [N011](notes/N011-survival-actions-and-world-catalog.md), leanness clarification
included.

This document is two things:

1. **The survey:** settings, buildings, houses, rooms, outdoor objects and utilities in a typical
   US-like world. Each is kept only if it adds gameplay value, or earns its place as world-building.
2. **The id contract:** every planned content id, so domain authors can work in parallel and
   cross-references resolve. `tools/content/lint.py` enforces the references.

## Value filter (applied to every entry)

Keep an **archetype** only if it:

- **(a)** behaves differently: new capability, feature or reaction; or
- **(b)** is a meaningful source of something (supplies, parts, water, fuel, power, knowledge); or
- **(c)** is iconic world-building for the setting.

Everything else is one of these instead:

- a **variant or material modifier** (e.g. an aluminium bat is the `baseball_bat` archetype plus
  `aluminum_body`)
- a **detail table** (colours, prints, contents kinds, labels, titles, headlines)
- **aggregated clutter** (one "desk clutter" object, not 30 office supplies)

## 1. Settings (zones)

| id | What it is | Signature buildings | Density (people/km²) |
|---|---|---|---|
| `downtown_core` | High-rises, offices, hotels, street-level retail, parking garages | office_tower, highrise_apartment, hotel, bank, parking_garage, coffee_shop | 10,000–25,000 |
| `urban_residential` | Rowhouses, walk-ups, corner stores, churches, schools | rowhouse, walkup_apartment, convenience_store, laundromat, church, school | 5,000–12,000 |
| `commercial_strip` | Arterial roads with strip malls, big-box stores, gas stations, fast food, motels | strip_mall, big_box_store, supermarket, gas_station, fast_food, motel, auto_parts_store | 500–2,000 (by day) |
| `suburb` | Single-family houses on cul-de-sacs, parks, elementary schools | ranch_house, colonial_house, split_level, bungalow, mcmansion, townhouse | 1,000–3,000 |
| `industrial` | Warehouses, factories, truck depots, lumber and junk yards, self-storage | warehouse, factory, auto_repair_garage, lumber_yard, junkyard, self_storage | 100–1,000 (by day) |
| `small_town` | Main street with diner, hardware store, post office, bar, church, fire station | diner, hardware_store, post_office, bar, church, fire_station, pharmacy | 300–1,500 |
| `rural` | Farms, homesteads with wells, trailer parks, feed stores, woods and fields | farmhouse, barn, mobile_home, cabin | 5–100 |
| `highway` | Interstate corridors, interchanges, truck stops, rest areas, wreck pile-ups | truck_stop, gas_station, motel | ~0 |
| `park_forest` | City parks, state forest, campgrounds, lake cabins | cabin, park facilities | ~0 |

**Institutional sites** are placed *within* settings by the generator (ADR-0010-style spacing):
`hospital`, `medical_clinic`, `police_station`, `fire_station`, `school`, `library`, `church`.

**Utility sites:** `power_substation`, `water_treatment`, `water_tower`. They drive the utility
rules (N007): the power and water state per neighbourhood.

## 2. Building types (ids)

**Residential:**

- `ranch_house`, `colonial_house`, `bungalow`, `split_level`, `mcmansion`
- `townhouse`, `rowhouse`, `duplex`
- `walkup_apartment`, `highrise_apartment`
- `mobile_home`, `farmhouse`, `cabin`

**Retail and food:**

- `supermarket`, `convenience_store`, `pharmacy`, `hardware_store`, `big_box_store`
- `gun_store`, `sporting_goods`, `clothing_store`, `auto_parts_store`, `liquor_store`, `bookstore`
- `fast_food`, `diner`, `bar`, `coffee_shop`, `gas_station`, `laundromat`
- `strip_mall`: a composite of small units

**Services and civic:**

- `office_lowrise`, `office_tower`, `bank`, `post_office`
- `police_station`, `fire_station`, `hospital`, `medical_clinic`
- `school`, `library`, `church`
- `motel`, `hotel`, `parking_garage`, `self_storage`

**Industrial, farm and utility:**

- `warehouse`, `factory`, `auto_repair_garage`, `lumber_yard`, `junkyard`, `truck_stop`
- `barn`, `power_substation`, `water_treatment`, `water_tower`

Dropped by the value filter:

- **City hall and courthouse:** play like offices, so they use `office_lowrise` with a detail table.
- **Malls:** a `strip_mall` plus several store types covers the gameplay.
- **Stadiums, airports, harbours:** out of scope for now.

## 3. Room types (ids)

**Home:**

- `kitchen`, `living_room`, `dining_room`
- `bedroom_master`, `bedroom_child`, `bathroom`, `closet`
- `laundry_room`, `home_office`, `garage`
- `basement`, `attic`, `shed`

**Retail:**

- `grocery_floor`, `pharmacy_floor`, `pharmacy_counter`, `hardware_floor`
- `store_floor_generic`, `gun_counter`, `checkout`, `stockroom`

**Food service:**

- `commercial_kitchen`, `walk_in_cooler`, `dining_area`, `bar_room`

**Office:**

- `open_office`, `private_office`, `conference_room`, `break_room`
- `reception`, `server_room`, `restroom_public`

**Institutional:**

- `classroom`, `cafeteria`, `gymnasium`, `nurse_office`, `library_stacks`
- `hospital_ward`, `emergency_bay`, `hospital_pharmacy`
- `police_bullpen`, `police_armory`, `holding_cell`
- `apparatus_bay`, `church_nave`

**Industrial:**

- `warehouse_floor`, `workshop`, `loading_dock`, `factory_floor`
- `boiler_room`, `storage_unit`, `control_room`

**Lodging:**

- `motel_room`, `hotel_room`

**Farm:**

- `barn_interior`

## 4. Outdoor sets (ids)

| id | Contents (examples) |
|---|---|
| `residential_street` | parked cars, mailboxes, trash cans, fire hydrants, street trees, fences, street lights |
| `front_yard` | fences, hedges, garden gnome, bikes, mailbox |
| `backyard` | grill, patio furniture, woodpile, shed, rain barrel, kids' play set, pool, garden tools |
| `downtown_street` | benches, bus shelters, newspaper boxes, planters, parked cars, bike racks, street lights, vending machines |
| `alley` | dumpsters, pallets, fire escapes, trash |
| `parking_lot` | cars, shopping carts, light poles, dumpsters |
| `gas_station_forecourt` | pumps (underground tanks), propane exchange cage, ice chest, vending machine |
| `construction_site` | lumber stacks, rebar, cinder blocks, scaffolding, porta-potty, fuel cans, generator |
| `highway_corridor` | wrecked cars, semi trucks, guardrails, jersey barriers, signs, road flares |
| `park_grounds` | playground, benches, picnic tables, trees, trash cans |
| `farmyard` | tractor, elevated fuel tank, hay bales, water trough, hand well pump, barbed wire, fencing |
| `industrial_yard` | forklift (propane), pallets, 55-gallon drums, shipping containers, dumpsters |
| `school_grounds` | school buses, playground, bike racks |
| `trailer_park` | mobile-home skirting, propane tanks, junk cars, grills |
| `campground` | tents, fire rings, picnic tables, coolers, parked cars |
| `emergency_scene` | police cruiser, ambulance, fire engine, cones, barricades (struggle hotspots, N007) |

## 5. Occupant profiles (ids): who was there at the event (P-IT-03)

`office_worker`, `retail_worker`, `grocery_clerk`, `nurse`, `doctor`, `police_officer`,
`firefighter`, `construction_worker`, `mechanic`, `cook`, `teacher`, `child_student`,
`teen_student`, `retiree`, `at_home_adult`, `farmer`, `trucker`, `delivery_driver`,
`security_guard`, `janitor`, `pharmacist`, `bartender`, `outdoorsman`, `jogger`, `sleeper`
(anyone at home at night: sleepwear), `churchgoer`.

## 6. Item domains and planned archetype ids

Existing exemplars are marked ✓. Authors may add ids that pass the value filter and must list them
in their domain's report.

### A. Kitchen, food and drink (`items/kitchen/`, `items/food/`)

- **Containers and cookware:** ✓milk_jug, ✓glass_bottle, ✓cooking_pot, ✓bleach_jug,
  frying_pan (cast iron: weapon), baking_sheet, mug, dinner_plate, plastic_tub, cooler, thermos,
  kettle, water_filter_pitcher, bucket (also B)
- **Tools:** ✓kitchen_knife, cleaver, can_opener, rolling_pin, cutlery (aggregated), fire_extinguisher
- **Consumables:** trash_bags, aluminum_foil, zip_bags, paper_towels, dish_soap
- **Food** (few archetypes; kinds via detail tables):
  - canned_food, boxed_dry_food, jar_food, bottled_water, soda_can, snack_bar
  - bread_loaf, fresh_produce, frozen_food, meat_package, coffee_can, honey_jar
  - bag_of_rice, bag_of_flour, bag_of_sugar, pet_food_bag, baby_formula
  - **Canned kinds** (soup, beans, chili, fruit, tuna, vegetables…) come from a `canned_food_kind`
    detail table that also sets kcal.
- **Appliances** (fixtures, salvage): ✓refrigerator, stove_range, microwave, chest_freezer

### B. Tools, hardware, construction, fasteners, fuel, salvage (`items/tools/`, `items/construction/`, `items/salvage/`)

- **Hand tools:** ✓screwdriver, ✓crowbar, ✓claw_hammer, ✓adjustable_wrench, sledgehammer, hatchet,
  axe, handsaw, hacksaw, pliers, locking_pliers, wire_cutters, bolt_cutters, utility_knife, chisel,
  metal_file, whetstone, tape_measure, tire_iron, pipe_wrench, shovel, pickaxe
- **Power tools:** cordless_drill, drill_battery, angle_grinder, circular_saw, arc_welder,
  propane_torch, chainsaw (also E), portable_generator, jumper_cables, car_jack, extension_cord
- **Fasteners:** ✓duct_tape, ✓nails_box, screws_box, superglue, epoxy, wood_glue, zip_ties,
  nylon_rope, paracord, baling_wire, steel_chain, padlock
- **Materials:** ✓wood_plank, ✓steel_pipe, ✓brick, lumber_2x4, plywood_sheet, cinder_block, rebar,
  sheet_metal, pallet, tarp, plastic_sheeting, sandbag, concrete_mix_bag
- **Fuel and fluids:** gas_can, propane_tank, fuel_canister, lighter_fluid_can, motor_oil_bottle,
  antifreeze_jug, wd40_can, drum_55gal
- **Containers and haulers:** bucket, toolbox, tool_belt (also C), wheelbarrow, hand_truck,
  ladder_step, extension_ladder
- **Salvage:** ✓rubber_hose, ✓copper_tubing, ✓battery_aa, ✓car_battery, battery_d, battery_9v,
  copper_wire, electric_motor, steel_spring, leaf_spring, scrap_metal, glass_shards (debris)

### C. Clothing, bags, personal, hygiene, medical (`items/clothing/`, `items/personal/`, `items/medical/`)

- **Tops:** ✓t_shirt, dress_shirt, sweater, hoodie, fleece_jacket, winter_coat, rain_jacket,
  leather_jacket (bite armour), suit_jacket, hi_vis_vest, police_vest (body armour), scrubs_top,
  firefighter_coat
- **Bottoms:** jeans, work_pants, suit_trousers, skirt, shorts, sweatpants, scrubs_pants
- **Full body:** pajamas, coveralls, dress
- **Underwear and feet:** underwear, socks, sneakers, work_boots, dress_shoes, slippers, rubber_boots
- **Head, face, hands:** beanie, baseball_cap, hard_hat, motorcycle_helmet, bike_helmet,
  football_helmet, sports_pads, work_gloves, leather_gloves, latex_gloves, scarf, bandana,
  dust_mask, goggles, eyeglasses, wristwatch
- **Bags:** school_backpack, hiking_pack, duffel_bag, handbag, laptop_bag, briefcase, fanny_pack
- **Personal:** wallet, house_key, car_key, smartphone, lanyard_badge, photo, cash, jewelry,
  cigarettes, stuffed_toy
- **Hygiene:** soap_bar, shampoo_bottle, deodorant_spray, perfume_bottle, hand_sanitizer_bottle,
  toilet_paper, towel, razor, sanitary_pads, toothbrush_kit
- **Medical:** bandage_box, gauze_pads, first_aid_kit, painkillers, antibiotics, antiseptic_bottle,
  rubbing_alcohol_bottle, suture_kit, tourniquet, splint_sam, crutches, stethoscope,
  purification_tablets, saline_bag

### D. Furniture, fixtures, electronics, lighting, paper and books (`items/furniture/`, `items/fixtures/`, `items/electronics/`, `items/paper/`)

- **Furniture:** ✓sofa, armchair, dining_table, dining_chair, office_chair, desk, bookshelf,
  dresser, bed_frame, mattress, nightstand, wardrobe, coffee_table, filing_cabinet, safe,
  metal_locker, metal_shelving, workbench, hospital_bed, church_pew, school_desk
- **Fixtures:** ✓radiator, door_interior, door_exterior, door_steel, window_house,
  window_storefront, toilet, bathtub, sink, water_heater, kitchen_cabinet, fireplace, wood_stove,
  washing_machine, clothes_dryer, smoke_detector
- **Electronics and light:** ✓tv_remote, ✓alarm_clock, ✓work_light_12v, laptop, desktop_pc,
  television, emergency_radio, walkie_talkie, flashlight, headlamp, battery_lantern,
  kerosene_lantern, candle, glow_stick, power_bank, solar_charger, security_camera_dvr
- **Paper and knowledge:** ✓newspaper, book (titles by detail), skill_manual (subject sets
  `teaches` via detail table, N012), city_map, road_atlas, notebook, pen, marker, spray_paint,
  paper_ream, cardboard_box, desk_clutter, survivor_note (text by generation, P-WO-14)
- **Fire:** ✓matchbook, ✓lighter, ✓firewood, ferro_rod, charcoal_bag

### E. Outdoors, street, vehicles, garden, farm, camping, sports, weapons (`items/vehicles/`, `items/street/`, `items/garden/`, `items/farm/`, `items/outdoors/`, `items/weapons/`)

- **Vehicles** (objects): ✓car_sedan, suv, pickup_truck, minivan, delivery_van, semi_truck,
  police_cruiser, ambulance, fire_engine, school_bus, motorcycle, bicycle, shopping_cart,
  tractor, forklift, riding_mower
- **Street:** mailbox, trash_can_street, dumpster, fire_hydrant, bench, bus_shelter, street_light,
  newspaper_box, vending_machine, parking_meter, manhole_cover, jersey_barrier, traffic_cone,
  road_sign, guardrail, utility_pole, chain_link_fence, wooden_fence, planter, bike_rack,
  shipping_container
- **Garden and yard:** ✓garden_hose, lawn_mower, gas_grill, charcoal_grill, patio_chair,
  patio_table, rake, pitchfork, pruning_shears, machete, backyard_pool, rain_barrel, fire_pit,
  woodpile, play_set, garden_gnome, shed_object, hedge, tree
- **Farm:** hay_bale, feed_sack, farm_fuel_tank, water_trough, hand_well_pump, barbed_wire,
  t_post, grain_silo
- **Camping and outdoors:** ✓camp_stove, tent, sleeping_bag, water_pump_filter, compass,
  binoculars, hunting_knife, trekking_pole, road_flare, air_horn, fireworks
- **Sports:** baseball_bat, golf_club, hockey_stick, dumbbell, bowling_ball
- **Weapons and firearms** (US realism, scarce ammunition; extreme noise):
  - pistol, revolver, shotgun, hunting_rifle, semi_auto_rifle
  - ammo_9mm, ammo_12ga, ammo_308, ammo_556, magazine_pistol, magazine_rifle
  - compound_bow, crossbow, arrows, crossbow_bolts
  - police_baton, pepper_spray, taser, riot_shield

## 7. Loot tables (ids): shared by rooms, containers, profiles

**Kitchen:**

- `fridge_contents`, `freezer_contents`, `pantry_shelf`
- `kitchen_drawer`, `kitchen_cabinet_contents`, `under_sink`

**Bathroom and bedroom:**

- `medicine_cabinet`, `bathroom_cabinet`
- `dresser_clothes`, `closet_clothes`, `nightstand_drawer`, `kids_room_stuff`

**Home work areas and storage:**

- `desk_drawer`, `garage_shelf`, `workbench_tools`, `shed_contents`
- `basement_storage`, `attic_boxes`, `laundry_shelf`

**Shops:**

- `grocery_shelf_canned`, `grocery_shelf_dry`, `grocery_drinks`, `grocery_produce`,
  `grocery_household`
- `pharmacy_otc`, `pharmacy_rx`
- `hardware_shelf_tools`, `hardware_shelf_fasteners`, `hardware_shelf_fuel`
- `gun_store_stock`, `sporting_goods_stock`, `auto_parts_stock`, `liquor_shelf`, `bookstore_shelf`
- `convenience_counter`, `vending_machine_stock`

**Restaurants:**

- `restaurant_kitchen`, `walk_in_cooler_stock`

**Offices:**

- `office_desk`, `break_room_stock`, `reception_desk`

**Schools, hospitals, police and fire:**

- `classroom_supplies`, `nurse_office_stock`
- `hospital_supplies`, `hospital_pharmacy_stock`
- `police_armory_stock`, `police_locker`, `fire_station_gear`

**Industrial and farm:**

- `warehouse_pallet`, `factory_parts`, `farm_barn_stock`

**Vehicles, bodies and survivors:**

- `car_trunk`, `car_glovebox`
- `wallet_contents`, `purse_contents`, `backpack_school`, `pockets_generic`
- `encampment_stash` (N007), `struggle_debris` (N007)

## 8. What is deliberately generated, not authored (N011 leanness)

- **Colours, patterns, prints, logos:** detail tables on clothing, textiles, furniture.
- **Food kinds:** canned soup vs chili vs peaches is one `canned_food` archetype plus a
  `canned_food_kind` detail table that sets name and kcal.
- **Book titles and genres; manual subjects:** detail tables. A manual subject sets `teaches`.
- **Vehicle colours, bumper stickers, trunk mess:** details and loot.
- **Condition:** state modifiers driven by the clock and aftermath (charred, rusted, spoiled,
  broken, bloody).
- **Size and material variants:** `size_small`, `size_large`, `aluminum_body`, `heavy_duty`, `cheap`.
- **Brands:** `brand.*` tags from the brand universe (`content/brands/`).
