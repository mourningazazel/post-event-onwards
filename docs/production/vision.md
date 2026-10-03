# Vision

*Post Event: Onwards* is a super-realistic survival roguelike. Nearly everyone turned at once
into the Dead: walking corpses that cannot see you but never stop smelling you. You are one of
a very few who did not. Cities are as full of things as real cities, and as full of the Dead as
a real outbreak would leave them. You can do almost anything a real person could, and the
world answers with physics, not special cases.

## Naming

The enemies are **the Dead**. Generically, "the Dead"; a large group is a **Horde**; one of
them is described as the corpse it was: "dead office worker", "mutilated cop", "armless
child". They bear wounds from the Event that never healed, discoloured skin, rot. The word
"zombie" never appears in docs, UI, code, comments or commit messages, except inside the
owner's verbatim notes. In code: `Dead` is one unit, `horde` is any collection.

## Pillars

1. **Scent is the game.** The Dead climb a scent field the player cannot help feeding. Tension
   is managing what you leave behind: where you walk, what you bleed, what you burn, what you
   sound like. Any mechanic that lets the player ignore scent and sound is suspect.
2. **Hordes, not encounters.** Thousands of cheap units following one weighted rule, and
   **being swarmed is the main way to die**. Crowds form because units walk to strong scent,
   trip over each other and trample what falls. Performance is a pillar, not a polish task.
3. **Realism you can act on.** Items are physical things with materials, features and a
   quality; actions check capabilities against materials; skills are 1–100 with honest
   percentages and real failure. Nothing needs a wiki: the item's own text tells you what it
   does and what broken means.
4. **Onwards, always.** An endless, staged, persistent world on one clock, days since the
   Event. Early is harder and richer; late is easier and poorer. There is no going back. Runs
   end in hard permadeath, but the world persists: your last character rises as one of the
   Dead that can see, with your stats and gear, unless you made sure it could not. The next
   one, a son or daughter of the last or someone loosely tied to them, starts a mile or two away.
5. **A mind under pressure.** Mood (Depressed ↔ Manic) and Sanity are main mechanics. Low
   sanity makes the world unreliable; drugs, including psychedelics, are real choices with
   real costs.

## Scale

One tile is one metre, one z-level is one storey, and several units may share a tile at a
price: crowded tiles trip, knock out and trample (ADR-0013).

## Turn model

Turn-based: the world moves only when the player steps, waits or acts. The time between turns
is used to compute the next turn ahead, and the player's action patches only what it touches
(ADR-0012). The player never waits on the world.

## Tone

Horror and gore, on a gradient. Descriptions run from slightly unsettling to fully graphic ("a
severed limb soaked in blood and reeking of rot. Several fingers have fallen or been bitten
off"), scaled by what the thing is and how close you look.

## What it is not

- Not a tactical grid game with unit stats and initiative.
- Not a story game; the setting is told through what you find and the notes the dead survivors
  left. No NPCs are ever met, and there are no quests.
- Not high fantasy. Magic and its kin belong to the engine later, not to this game.
- Not tile graphics first. ASCII first; tilesets use the Dwarf Fortress grid layout so players'
  own tilesets fit, and every bundled asset is original (ADR-0015).
- No wildlife. Every animal is inexplicably dead; crows are heard in text, never found.

## Performance target

The player moves at most three times a second (D-011), so a world update has that gap to be
computed ahead: a speculated update finishes in under 100 ms and its commit in under 1 ms on
the M1 Air, the low-end test bed, with the renderer holding 60 FPS (D-021). Measured at three
sizes: 200×120 with 5,000 Dead, 512×512 with 20,000, and 512×512 with 50,000, each with many
scent emitters. Beyond the detailed radius the Dead are population numbers. Headroom is spent
on horde size and generation variety, not saved.

## Where the detail lives

- Direction and rules: `docs/adr/` (accepted architecture decisions), this file.
- Design corpus: `docs/design/` (scent and the Dead, world generation, content model, survival
  actions, world catalogue, gameplay model, purposes, playtest harness).
- The owner's own words: `docs/design/notes/`.
- Open questions for the owner: `DECISIONS_NEEDED.md`; answered ones: `decisions.md`.

## Near-term direction

1. A playable turn-based loop with speculate/commit (PEO-002, PEO-007).
2. The Dead's weighted-draw movement with aggregates, triggers, sound events, trips and
   trample, with the reproducible swarm scenarios (scent-mobs.md round 3).
3. Content loaded from `content/` into the game: registry, items, modifiers, quality model.
4. Building census, category retail and persona houses in world generation.
5. Skills 1–100 with percentages and failure; Mood and Sanity.
6. Replays from seed plus action log, and the headless playtest harness.
