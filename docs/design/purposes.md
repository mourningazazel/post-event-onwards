# Purpose catalog: what each gameplay function is *for*

Origin: owner note [N002](notes/N002-intended-use-tests.md).

## What the catalog is

- Every gameplay function (a mechanic, action, item behaviour or system feature) gets an entry
  answering **"what should this be used for?"**
- Each intended use gets at least one **purpose test**: a scenario plus a playthrough that proves
  the use can actually be done in the game.
- Purpose tests run on every change, forever. A change that makes an intended use impossible fails
  CI, even if every unit test passes.

## Rules

1. **An entry has an owner-confirmed status.**
   - `confirmed`: the owner agreed with the purpose.
   - `proposed`: Claude's understanding, not yet confirmed.
   - `question`: Claude doesn't know, and asks.
   - Purpose tests for `proposed` entries run but are flagged. `question` entries get no test until
     they are answered.
2. **Tests are never deleted silently.** If the owner deliberately changes what a mechanic is for,
   the entry is updated with a dated change record, and the test changes with it.
3. **Each purpose names its evidence:** the observable outcome that proves it. For example, "the
   player reaches the pharmacy counter with fewer than 5% of nearby mobs alerted".
4. **When code exists, the catalog moves into data** (`tests/purposes/*.toml`, linked to scenario
   files). This document stays as the readable index.

## Catalog (from the designs so far)

Status: everything below is **proposed** until the owner confirms it. ❓ marks questions.

### Scent and senses

| ID | Function | Intended use (Claude's understanding) | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-SC-01 | Player scent emission | Makes the player findable by blind mobs even when unseen, so staying still is not permanently safe | A stationary player in the open is eventually reached by idle mobs downwind | proposed |
| P-SC-02 | Wind carries scent | Makes **approach direction** a real decision: upwind approaches are safer | `upwind-approach` gets measurably fewer mob arrivals than `downwind-approach` | proposed |
| P-SC-03 | Scent masking / changing (e.g. corpse gore) | Lets a prepared player move through or loot a mob-dense area | `masked-scent-walkthrough`: masked player crosses a horde area with mob contact below a threshold; unmasked control run fails | proposed |
| P-SC-04 | Scent overpowering, lures and decoys | Use strong scents as **lures and decoys** to pull or redirect hordes (N004) | A thrown lure pulls a measurable share of nearby idle mobs away from the player's position | **confirmed** |
| P-SC-06 | Repellent scents | Built-up **repellent** scents drive mobs away and **reduce regional build-up**, so a player staying in one place has tools against growing crowds (N004) | In a long `camp-in-one-place` run, repellent build-up keeps local mob density and regional population growth below an untreated control run | **confirmed** |
| P-SC-05 | Rain and strong wind | Make **outdoor travel much more feasible**. Rain washes scent out. Wind carries scent further and faster but loses it upward faster. Scent exists **only on ground tiles**; nothing accumulates in the sky. (N004) | The same `walk-between-towns` route gets measurably fewer mob contacts in rain or strong wind than in calm, dry weather | **confirmed** |

### Enemies

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-EN-01 | Blind bump-to-attack | Danger comes even from mobs that don't know you're there, so crowds are dangerous by density alone | A mob steered only by scent attacks a player it never "detected" | proposed |
| P-EN-02 | Alert cascade | **Punishes noise and exposure** (speaking, grunting, being seen) with escalating threat | One grunt in `alert-cascade-city-block` recruits more mobs than a silent control run, and recruitment stops at the chain limit | proposed |
| P-EN-03 | Sound-capable units (~1 in 100) | Make **noise discipline** matter near the player | A noisy action alerts a sound unit within range; a quiet one does not | proposed |
| P-EN-04 | Vision units (rare, triggered) | **Boss/hunter units.** They find you through better senses and **pathfinding**, and their agitation **draws crowds toward you** (N004). | A triggered hunter reaches a hidden player that scent-only mobs don't, and the crowd drawn by its agitation converges on the player | **confirmed** |
| P-EN-08 | Being swarmed | **The main way to die** (N004). Density, cascades and hunters combine into swarms. | In `alert-cascade-city-block`, a careless bot dies by being surrounded; a careful bot survives the same seed | **confirmed** |
| P-EN-09 | Dead decay over time | Limbs and appendages get **easier to damage and maim** as days-since-event rises; damage output stays similar. Makes **later = easier but poorer** (N007). | The same dead archetype at 7 vs 365 days: equal damage dealt; limb-sever rate higher at 365 | **confirmed** |
| P-EN-05 | Push/swap contest when alerted or following | Alerted hordes can **surge through their own crowd** toward the player; trips create pile-ups | An alerted unit behind an idle crowd reaches a doorway faster than without swapping | proposed |
| P-EN-06 | Tripping | **Exploitable:** terrain becomes a **defence tool**. It also **punishes the player**, who can trip too, and is incidental chaos that slows crowds (N004). | A crowd crossing dense underbrush or rubble trips more often and arrives later than the same crowd on a clear street; the player can trip too | **confirmed** |
| P-EN-07 | Regional population migration | Staying too long, or making a scene in one area, **draws crowds** over time. The world "reacts". | A long camp in one region raises its population and the spawn rate at the detailed edge | proposed |

### World and interaction

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-WO-01 | Digging | Anything plausible: **moats, spike pits, escape routes, reaching basements, barriers**. The point is that you *can*. A hole one full z-level deep is a **very large effort** (N004). | A bot digs a moat across an entrance; mobs fall in or are held up; the time cost scales with volume and soil hardness | **confirmed** |
| P-WO-05 | Terrain affects crowd movement | Street vs underbrush vs rubble changes crowd speed and trip chance (N004) | Crossing time and trip rate differ by terrain type, as specified in the terrain data | **confirmed** |
| P-WO-06 | Falling | Falls into pits or down levels hurt based on drop height, using an acceleration value and a z-drop calculation, **not a physics system** (N004). Spikes at the bottom add damage. | A unit falling 1 vs 2 z-levels takes damage from the configured drop formula; a spiked pit adds its damage | **confirmed** |
| P-WO-02 | Cutting and breaching (sawing is one example) | Breach into or out of buildings. Outcome and time are decided by **wall material vs tool properties** (or bare hands if even remotely capable) (N004). | Sawing drywall with a handsaw succeeds; sawing concrete with the same saw doesn't; bare hands through drywall is very slow or impossible, per the material data | **confirmed** |
| P-WO-08 | Aftermath stages (1 week / 1 month / 1 year) | **Difficulty through world state.** Later stages have fewer Dead **and** fewer supplies, with more spoilage and damage (N005). | For one seed across the three stages: dead counts and usable supplies strictly fall; every window broken at 1 week is still broken at 1 month and 1 year | **confirmed** |
| P-WO-09 | Sealed buildings hold their Dead | Opening a sealed building is a gamble: supplies nobody looted, but its occupants are still inside (N005) | Sealed buildings keep their occupant Dead and unlooted contents at every stage; forced-open buildings lose both over time | **confirmed** |
| P-WO-10 | Other survivors as traces only | No NPCs are ever met; their existence shows through looting, damage and struggle traces (N005) | No NPC entity spawns in any scenario; looting and struggle traces exist at the stage-appropriate rates | **confirmed** |
| P-WO-11 | Per-world event (moment, turned fraction 80–99%) | Every world tells a different story, shown on the opening splash screen, and shapes where Dead are and how scarce things are (N007) | Two seeds with different event moments place occupants differently (homes vs workplaces); the splash screen shows both parameters | **confirmed** |
| P-WO-12 | Survivor-scaled traces | A lower turned % produces more bodies, damage, looting, barricades, notes and encampments (N007) | Same seed at 80% vs 99% turned: bodies, barricades and scarcity are all measurably higher at 80% | **confirmed** |
| P-WO-13 | Encampments | **Concentrated supplies with a visible reason for desertion**: usually overrun, rarely an owner who died elsewhere (N007) | Generated encampments show their reason through evidence; overrun ones have dead presence | **confirmed** |
| P-WO-14 | Notes | Survivors' notes carry story and hints, scaled by survivor count (N007) | Notes exist at survivor-scaled rates and reference plausible places (generation addresses, G01 D1.5) | **confirmed** |
| P-WO-15 | Utilities (power and water) | Power per neighbourhood falls with time and sparseness; water is more reliable; wells hold (N007) | Across a city→rural transect at 7 vs 365 days, powered-neighbourhood rates fall with both; rural wells keep working | **confirmed** |
| P-WO-16 | Fires | Burned buildings or clusters: intensity by area, collapse through the structural rule, charred items, smoke scent (N007) | A severe fire removes walls; unsupported upper contents fall; charred items get mixed conditions; smoke masks scent nearby | **confirmed** |
| P-WO-17 | Global clock as a gradient | **Days-since-event drives all aftermath**; new areas generate at the current clock; it works at any value, e.g. 5 years (N007) | Clock sweep 7 → 30 → 365 → 1,825: every aftermath metric is monotonic and sane; a 1-week start that plays a year generates year-old new areas | **confirmed** |
| P-WO-18 | Revisit aging gate | Places you've been stay as you left them. After about **1 month** away, they catch up to the current clock (N009) | Revisit after 10 days: byte-identical area. After 45 days: untouched content aged, player changes intact | **confirmed** |
| P-WO-07 | Realistic population | Cities and buildings are populated like real ones, and Dead are as numerous as a real outbreak's; the player is one of very few survivors (N004) | Dead density in a generated district matches its configured real-world population density; buildings hold plausible occupants | **confirmed** |
| P-WO-03 | Walking-only travel | Makes travel between cities a **perilous expedition** that needs preparation | `walk-between-towns` bot must manage supplies; an unprepared bot fails more often | proposed |
| P-WO-04 | Landmark-anchored cities | Gives each city an identity and lets players navigate by landmarks | Every generated city records a landmark, visible from inside the city | proposed |

### Character and items

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-CH-01 | Physical belongings (pockets, bags, layers) | Loadout **trade-offs**: what you wear decides what you can carry | Swapping a jacket for a vest changes carry capacity; losing a backpack loses its contents | proposed |
| P-CH-02 | Unlimited item nesting and detail | Future features can deepen items without redesign; the player can find items inside items | A four-deep chain (jacket → wallet → photo → writing) survives save, load and looting | proposed |
| P-CH-03 | Promoted Dead with former lives | Looting one of the Dead is meaningful and fits who it was | A promoted "police officer" dead carries occupation-consistent items | proposed |
| P-CH-04 | Permadeath | Real stakes: bad decisions and bad luck end runs (owner N003) | Death ends the run; no path restores the character | **confirmed** |
| P-CH-05 | Getting permanently stuck (ditch, pit) | Realism over protection (owner N003) | A player in a pit they can't climb out of stays stuck; the rule checker does **not** flag it | **confirmed** |

### Items and content model ([N008](notes/N008-ai-authored-content-and-properties.md), [content-model.md](content-model.md))

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-IT-01 | Generic attachment through features | Attach any object of fitting size to a `socket`/handle (limits such as `maxAttachSize`) and have it *behave*, e.g. a knife on a pipe acts as a spear, with no recipe (N008) | Several unrelated head/haft pairs attach within limits and produce derived reach, pierce and hammer; out-of-limit pairs are refused | **confirmed** |
| P-IT-02 | Generic reactions (`onImpact`, `onHeat`, `onWet`, …) | Objects respond to stimuli from their physical properties: break easily, deal blunt force, cut, shatter into shards (N008) | A glass bottle club shatters on hard impact and becomes a `pierce` weapon; a steel pipe dents but holds | **confirmed** |
| P-IT-03 | Mobs as outputs of their environment | Clothing and pockets follow occupation, place, time of day and season, e.g. an office worker in a suit (N008) | Promoted Dead from an office at 2 p.m. wear office clothing; from homes at 3 a.m., mostly sleepwear | **confirmed** |
| P-IT-04 | Realistic place inventories | Rooms hold real-world items in realistic quantities, filtered to useful, flavour or aesthetic items, with clutter aggregated (N008) | Generated kitchens and offices fall within their room programs' quantity ranges; no orphan items | **confirmed** |
| P-IT-05 | Capability-based actions and crafting | Objects combine and act "in ways that just make sense logically" through capabilities and tags, never item-specific code (N008) | Sawing, prying, digging and improvised crafts work with any object that meets the capability thresholds | **confirmed** |
| P-IT-06 | Keys that open real doors | A key in a dead one's pocket references its home or workplace door by generation address | Following a found key's address leads to a door it opens | proposed |
| P-IT-07 | Load vs Hold attachment | Players understand every joint through Load, Hold and Set time, with a clear status (Secure / Firm / Wobbly / Won't hold); the attach screen predicts it (N010) | The predicted status matches the observed failure rate over many uses of the same build | **confirmed** |
| P-IT-08 | Fasteners differ meaningfully | Glue cures over time; rope is poor on small items but good on big, irregular ones; tape hates wet and heat; welds need a welder, and are loud and smoky (N010) | For the same head and haft, methods rank as specified (e.g. cord < tape < cured epoxy on a knife-spear; rope > instant glue on a brick maul); swinging before cure risks failure | **confirmed** |
| P-IT-09 | Generated descriptions | Surface details come from property combinations as prose; hidden properties are revealed by use (N010) | A described property changes the text when the property changes; a charred radio reads "not sure it works" until tried | **confirmed** |
| P-IT-10 | Brands and name composition | Fictional brands with parent-company relationships; brand tags compose names; tier shifts relevant properties (N010) | Fennick Hollow cereal and bread share a parent; "charred Fennick Hollow …" is composed from tags; Tenacor Pro tape gives more Hold than budget tape | **confirmed** |

### Survival actions and skills ([N011](notes/N011-survival-actions-and-world-catalog.md), [N012](notes/N012-trainable-expert-skills.md), [survival-actions.md](survival-actions.md))

Evidence is **live**: these chains run in `tests/content/chains/` on every change.

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-SA-01 | Improvised incendiaries | Burn crowds with salvaged, siphoned fuel in a breakable container, lit by any flame (N011's own example) | chain `firebomb` (7 steps) | **confirmed** |
| P-SA-02 | Disassembly and salvage | Take real objects apart for useful parts (fridge → hose; car → battery, hoses; remote → batteries) | chains `firebomb`, `alarm_lure`, `car_battery_lamp` | **confirmed** |
| P-SA-03 | Timed noise lures | Pull hearing Dead away with a device left behind | chain `alarm_lure` | proposed |
| P-SA-04 | Barricading | Block and board openings with furniture, planks, nails | chain `barricade_front_door` | proposed |
| P-SA-05 | Water from the built environment | Drain water heaters/cisterns; make it safe by boiling or bleach | chain `water_heater_water` | proposed |
| P-SA-06 | Improvised weapons | Attach heads to hafts (Load vs Hold) | chain `improvised_spear` | **confirmed** (N008/N010) |
| P-SA-07 | Improvised medicine | Splints and bandages from rulers, T-shirts, tape | chain `splint` | proposed |
| P-SA-08 | Improvised climbing/escape | Knotted sheets/hoses on solid anchors | chain `rope_escape` | proposed |
| P-SA-09 | Scavenged power | Car batteries run 12 V lights for nights inside | chain `car_battery_lamp` | proposed |
| P-SA-10 | Fuel realism | Gasoline catches from a match, diesel puddles don't (tool choice matters) | negative chain `diesel_does_not_catch` | proposed |
| P-SK-01 | Beyond-human skills | Trained skills reach feats no real person could (N012) | chain `expert_friction_fire` (min_skill 9); `climb_sheer_wall`, `crack_safe_by_feel` gated by `min_skill` | **confirmed** |
| P-SK-02 | Learning by reading | Manuals and books found in the world train skills (N012) | skill_manual subjects set `teaches` (domain D tests) | **confirmed** |
| P-SK-03 | Skill substitutes for tools | Experts do more with worse tools (`tool_substitution`) | engine test once actions are implemented | proposed |

### N013 answers ([N013](notes/N013-bikes-guns-bites-locks-animals-words.md))

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-TR-01 | Bicycles | Faster travel at a risk: 2 tiles per turn, an extra stopping turn, you can hit things | chains `ride_bicycle`, `shopping_cart_not_rideable` (negative) | **confirmed** |
| P-EN-10 | Gunfire and hearing units | Shots mainly make noise; hearing units push through crowds toward it; only human noises trigger | engine playtest once sound events exist | **confirmed** |
| P-EN-11 | Kill rule | Head destruction or total disablement only | G05 / combat tests | **confirmed** |
| P-CH-06 | Body-part damage and clothing | Hit rolls pick a part; coverage and armour there decide; clothing is a major defence | expectations: leather_jacket forearm coverage and bite armour, scarf neck coverage, T-shirt ~0 | **confirmed** |
| P-WO-19 | Lock entry ladder | Pick (rare lockpick set, silent) > bash the knob (loud) > extra locks make brute force impractical | chains `pick_front_door`, `bash_interior_knob`, `bash_front_door`, `barred_steel_door_resists` (negative) | **confirmed** |
| P-WO-20 | No animals; crows as ambience | Eerie emptiness; only humans remain | content has no animal creatures; audio ambience later | **confirmed** |
| P-IT-11 | Useless less-lethal gear | Pepper spray and stun guns exist and do nothing to the dead (humour) | items present; no effect on the dead | **confirmed** |

### N014 answers ([N014](notes/N014-the-dead-ai-content-skills-mood-drugs.md))

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-EN-12 | Weighted-draw movement with aggregate and company pull | Crowds fan out yet accumulate on strong scent without spawn rules; an even plain drains toward the richer region | scenarios `swarm-accumulation`, `fan-out` (scent-mobs round 3) | **confirmed** |
| P-EN-13 | Triggers by general direction, expiring with displacement | A scent-only trigger is a swarming risk, not a chase; the group ends up where you were | `trigger-displacement`, `no-state-spread` | **confirmed** |
| P-EN-14 | Sound events (kind + dB) | Simple, cheap noise model; mechanical attracts, human triggers; hearing units follow the newest sound; running is trackable | `sound-lure`; a running player is followed by an alerted hearing unit, a walking one is not | **confirmed** |
| P-EN-15 | Trips cascade and trample | A frenzied horde slows itself; a prone unit or player under a crowd takes blunt damage | `frenzy-trips`, `trample-damage` | **confirmed** |
| P-EN-16 | Speeds: alerted ×1.5, following ×2, player can run | Chase is possible only with sound and vision units | same-seed chase: a following hearing unit closes on a walking player, not a running one at full speed | **confirmed** |
| P-EN-17 | Reproducible swarm behaviour | AI tuning never silently changes crowd feel | the scenario table in scent-mobs round 3 runs on every change with reported metrics | **confirmed** |
| P-SK-04 | Skills 1–100 with percentage and failure outcomes | The player sees why an action may fail and what failure costs; late-game actions unlock then ramp | `expectations`: per-skill and aggregate percentages match the data model; failure outcomes occur at the configured rates | **confirmed** |
| P-SK-05 | Multi-skill actions | Complex builds need several competencies, each visible | an action with two skills shows both percentages and their product | **confirmed** |
| P-SK-06 | Tools gated by skill | A lockpick set is useless at security 1 | `pick_lock` refused below the tool's `min_skill`; allowed above | **confirmed** |
| P-CH-07 | Mood (Depressed ↔ Manic), mostly hidden | Quietly shapes exhaustion, addiction, error rates; mania trades survival gains for crafting and firearm mistakes | scripted mood sweep: failure rates and misfire chance track the mood curve | **confirmed** |
| P-CH-08 | Sanity and hallucinations | Low sanity makes the world unreliable: phantom objects, phantom item states, gibberish | at low sanity a scripted run sees objects that vanish on interaction and a bottle that is empty on drinking | **confirmed** |
| P-CH-09 | Drugs and psychedelics | Realistic supply now; later mechanics; psychedelics change visuals and sanity only | content: narcotics, pharmaceuticals, paraphernalia exist with realistic placement; pharmacies locked and urban | **confirmed** |
| P-IT-12 | Quality on every item; broken as text plus disabled tags | Time and situation degrade items without duplicate archetypes; the player can read what broken means | every item has `quality`; `broken` text and `broken_disables` present where the item does anything | **confirmed** |
| P-IT-13 | No-wiki rule | Every detail is usable in play from the item's own text | content lint: any mechanic-bearing property has description rules | **confirmed** |
| P-WO-21 | Building census in real proportions; category retail; persona houses | Cities look and loot like real cities; two houses differ by who lived there | generated district building mix matches the census tables; two persona houses differ in room contents | **confirmed** |
| P-WO-22 | Big-box stores | Thousands of items, deep urban, heavily populated; occasionally out of town but still crowded | placement and population rules hold across seeds | **confirmed** |
| P-WO-23 | Maps | Rare finds in sensible places; reading marks general locations before generation and catches up | `city_hall`/`transit_stop` map rates; read map marks unseen POIs; later generation lands within the marked area | **confirmed** |
| P-TR-02 | Bicycle crashes | Speed at a cost: noise, limb injury (fracture, concussion), attention from the Dead | crash chain: sound event emitted, body-part injury rolled | **confirmed** |
| P-EN-18 | Gun store alarm and stock | Breaking in is loud when powered; the selection is huge but ammunition is thinned a week in; high-end guns rare or too loud | gun_store loot at 7 vs 365 days; alarm fires only with power | **confirmed** |
| P-SA-11 | Explosives | Generic ingredients, minimal explosion mechanics, grounded in real chemistry | chain with generic ingredients; blast resolved by a radius and blunt/heat effect only | **confirmed** |
| P-WO-24 | Ambient crows | Eerie text now and then; never a creature | ambient text events appear at the configured rate; no crow entity exists | **confirmed** |

### N015 answers ([N015](notes/N015-locks-books-death-stats-tiles.md))

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-WO-25 | Lock gradient by kind, weighted by place | Security is legible and realistic: the lock kind tells you the effort; secure places get secure locks | generated locks per building/container type match the weight tables; pick and bash difficulties follow the tier band | **confirmed** |
| P-CH-10 | The dead player rises | Death has a lasting mark on the world; ending yourself near death is a real choice; the risen self is a hunter | after death without brain destruction, a vision-tracking Dead with the old stats and gear stands at the old start; with brain destruction, none; the new character spawns 1–2 miles away | **confirmed** |
| P-SK-07 | Books as slow, capped learning | Libraries are worth the risk but cannot be ground: time passes, hordes build, each book gives at most one point once, gates cut both ends | a scripted reader gains ≤ 1 point per book, never twice from the same instance, nothing from too-easy or too-hard books; reading advances the clock | **confirmed** |
| P-EN-19 | Shared tiles with trip, knock-out and trample | Being swarmed means being on a tile with the Dead; crowds churn through doorways instead of freezing | tile occupancy never exceeds capacity; crowded-tile steps trip and knock out at the configured rates; a prone unit is trampled | **confirmed** |

### Tooling

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-TL-01 | Wizard mode | Testing, sandboxing, reproducing bugs; available to players, clearly marked (owner N003) | Wizard commands work from keyboard and agent protocol; runs are flagged as wizard | **confirmed** |

## Open questions for the owner

All six earlier questions were answered in [N004](notes/N004-realism-spirit-and-purpose-answers.md).
Remaining:

1. Please correct any **proposed** purpose above that doesn't match your intent.
2. ~~P-WO-07 follow-up~~: answered in N005. Yes: Dead are the people who were there, and
   sealed buildings still hold theirs.
3. ~~N005 follow-ups~~: all answered in N007.
4. ~~ADR-0011 item 4~~: answered in N009 (1-month gate); player changes are preserved (N011).
5. ~~Content-model decisions D-CM1 to D-CM4~~: answered in N010.
6. The survival-actions open questions (`survival-actions.md` §5):
   - bicycles
   - firearms availability
   - whether bites infect
   - chemistry limits
   - animals
7. Please confirm or correct the **proposed** P-SA / P-SK entries above.
