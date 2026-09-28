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
| P-SC-03 | Scent masking / changing (e.g. zombie guts) | Lets a prepared player move through or loot a mob-dense area | `masked-scent-walkthrough`: masked player crosses a horde area with mob contact below a threshold; unmasked control run fails | proposed |
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
| P-WO-08 | Aftermath stages (1 week / 1 month / 1 year) | **Difficulty through world state.** Later stages have fewer zombies **and** fewer supplies, with more spoilage and damage (N005). | For one seed across the three stages: zombie counts and usable supplies strictly fall; every window broken at 1 week is still broken at 1 month and 1 year | **confirmed** |
| P-WO-09 | Sealed buildings hold their zombies | Opening a sealed building is a gamble: supplies nobody looted, but its occupants are still inside (N005) | Sealed buildings keep their occupant zombies and unlooted contents at every stage; forced-open buildings lose both over time | **confirmed** |
| P-WO-10 | Other survivors as traces only | No NPCs are ever met; their existence shows through looting, damage and struggle traces (N005) | No NPC entity spawns in any scenario; looting and struggle traces exist at the stage-appropriate rates | **confirmed** |
| P-WO-07 | Realistic population | Cities and buildings are populated like real ones, and zombies are as numerous as a real outbreak's; the player is one of very few survivors (N004) | Zombie density in a generated district matches its configured real-world population density; buildings hold plausible occupants | **confirmed** |
| P-WO-03 | Walking-only travel | Makes travel between cities a **perilous expedition** that needs preparation | `walk-between-towns` bot must manage supplies; an unprepared bot fails more often | proposed |
| P-WO-04 | Landmark-anchored cities | Gives each city an identity and lets players navigate by landmarks | Every generated city records a landmark, visible from inside the city | proposed |

### Character and items

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-CH-01 | Physical belongings (pockets, bags, layers) | Loadout **trade-offs**: what you wear decides what you can carry | Swapping a jacket for a vest changes carry capacity; losing a backpack loses its contents | proposed |
| P-CH-02 | Unlimited item nesting and detail | Future features can deepen items without redesign; the player can find items inside items | A four-deep chain (jacket → wallet → photo → writing) survives save, load and looting | proposed |
| P-CH-03 | Promoted zombies with former lives | Looting a zombie is meaningful and fits who it was | A promoted "police officer" zombie carries occupation-consistent items | proposed |
| P-CH-04 | Permadeath | Real stakes: bad decisions and bad luck end runs (owner N003) | Death ends the run; no path restores the character | **confirmed** |
| P-CH-05 | Getting permanently stuck (ditch, pit) | Realism over protection (owner N003) | A player in a pit they can't climb out of stays stuck; the rule checker does **not** flag it | **confirmed** |

### Tooling

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-TL-01 | Wizard mode | Testing, sandboxing, reproducing bugs; available to players, clearly marked (owner N003) | Wizard commands work from keyboard and agent protocol; runs are flagged as wizard | **confirmed** |

## Open questions for the owner

All six earlier questions were answered in [N004](notes/N004-realism-spirit-and-purpose-answers.md).
Remaining:

1. Please correct any **proposed** purpose above that doesn't match your intent.
2. ~~P-WO-07 follow-up~~: answered in N005. Yes: zombies are the people who were there, and
   sealed buildings still hold theirs.
3. The N005 follow-ups listed in `world-generation.md`, "Aftermath model": event timing, traces of
   other survivors, zombie decay, nature at one year, utilities, fires.
