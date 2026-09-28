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
| P-SC-04 | Scent overpowering | ❓ Should the player be able to use strong scents as **lures or decoys** (throw rotten meat to pull a horde away)? Or does overpowering only exist for realism, e.g. an area soaked in blood hides the player? | — | question |
| P-SC-05 | Rain and weather reducing scent | ❓ Is rain meant as a **window of opportunity** the player should learn to exploit? | — | question |

### Enemies

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-EN-01 | Blind bump-to-attack | Danger comes even from mobs that don't know you're there, so crowds are dangerous by density alone | A mob steered only by scent attacks a player it never "detected" | proposed |
| P-EN-02 | Alert cascade | **Punishes noise and exposure** (speaking, grunting, being seen) with escalating threat | One grunt in `alert-cascade-city-block` recruits more mobs than a silent control run, and recruitment stops at the chain limit | proposed |
| P-EN-03 | Sound-capable units (~1 in 100) | Make **noise discipline** matter near the player | A noisy action alerts a sound unit within range; a quiet one does not | proposed |
| P-EN-04 | Vision units (rare, triggered) | ❓ What triggers them, and what is their role: an escalation event, a "boss", or a hunter that forces the player to move? | — | question |
| P-EN-05 | Push/swap contest when alerted or following | Alerted hordes can **surge through their own crowd** toward the player; trips create pile-ups | An alerted unit behind an idle crowd reaches a doorway faster than without swapping | proposed |
| P-EN-06 | Tripping in contests | ❓ Is tripping meant to be **exploitable** (e.g. creating choke-point pile-ups on purpose), or only incidental chaos? | — | question |
| P-EN-07 | Regional population migration | Staying too long, or making a scene in one area, **draws crowds** over time. The world "reacts". | A long camp in one region raises its population and the spawn rate at the detailed edge | proposed |

### World and interaction

| ID | Function | Intended use | Purpose test (evidence) | Status |
|---|---|---|---|---|
| P-WO-01 | Digging | ❓ Which uses are intended? Escape routes, pits and traps for mobs, reaching basements or underground, burying things, making barriers? | — | question |
| P-WO-02 | Sawing through walls | ❓ Breaching locked buildings, making escape routes, both? Should it be noisy (tying into P-EN-03)? | — | question |
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

1. **P-SC-04** Scent overpowering: lures and decoys, or realism only?
2. **P-SC-05** Rain: a deliberate opportunity window?
3. **P-EN-04** Vision units: what triggers them, and what role do they play?
4. **P-EN-06** Tripping: exploitable on purpose, or incidental?
5. **P-WO-01** Digging: which uses are intended?
6. **P-WO-02** Sawing: which uses are intended, and is it noisy?
7. Please also correct any **proposed** purpose above that doesn't match your intent.
