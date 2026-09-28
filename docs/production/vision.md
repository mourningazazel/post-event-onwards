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
   end; hard permadeath. Seeds make runs shareable.
5. **A mind under pressure.** Mood (Depressed ↔ Manic) and Sanity are main mechanics. Low
   sanity makes the world unreliable; drugs, including psychedelics, are real choices with
   real costs.

## Turn model

Turn-based: the world moves only when the player steps, waits or acts. The time between turns
is used to compute the next turn ahead, and the player's action patches only what it touches
(ADR-0012). The player never waits on the world.

## What it is not

- Not a tactical grid game with unit stats and initiative.
- Not a story game; the setting is told through what you find and the notes the dead survivors
  left. No NPCs are ever met.
- Not tile graphics. ASCII first, with swappable fonts and tilesets later.
- No wildlife. Every animal is inexplicably dead; crows are heard in text, never found.

## Performance target

A 200×120 detailed area with 5,000 Dead resolves a full turn in under 16 ms on a mid-range
laptop, and a speculated turn commits in under 1 ms, with the renderer holding 60 FPS. Beyond
the detailed radius the Dead are population numbers. Until measured otherwise, treat this as
the budget every system must fit inside.

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
