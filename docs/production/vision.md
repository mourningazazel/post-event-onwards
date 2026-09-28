# Vision

*Post-Event Onwards* is an ASCII roguelike about being hunted by the Dead:
they cannot see you, but they never stop smelling you. Every design choice
serves three pillars.

## Naming

The enemies are **the Dead**. Generically, "the Dead"; a large group is a
**Horde**; one of them is described as the corpse it was: "dead office
worker", "mutilated cop", "armless child". They bear wounds from the Event
that never healed, discoloured skin, rot. The word "zombie" never appears in
docs, UI, code, comments or commit messages. Aesthetic rule, not negotiable.
In code: `Dead` is one unit, `horde` is any collection.

## Pillars

1. **Scent is the game.** The Dead do not path-find to the player; they
   climb a scent field the player cannot help feeding. Tension comes from managing
   what you leave behind: where you walk, what you bleed, what you burn.
   Any mechanic that lets the player ignore scent is suspect.
2. **Hordes, not encounters.** Thousands of cheap units, not dozens of smart
   ones. Emergent flanking and flooding from simple rules beats scripted AI.
   This is why performance is a pillar, not a polish task.
3. **Onwards, always.** The world is an endless sequence of stages. There is
   no going back; the exit is the only goal, and each stage is denser than
   the last. Runs end. Seeds make them shareable.

## Turn model

Turn-based: the world moves only when the player steps or waits. The
simulation never runs on a clock. The time between turns is not wasted:
the next turn is computed while the player thinks, and the player's action
patches only the cells it touches. The player must never wait on the world.

## Decisions

Gameplay, abstraction and direction choices are the user's. Agents raise
them in `DECISIONS_NEEDED.md` in a fixed A/B/C format and move on; they do
not guess. Technical method is the agents' own call.

## What it is not

- Not a tactical grid game with unit stats and initiative.
- Not a story game; the setting is told through what you find, never cutscenes.
- Not tile-graphics. ASCII (with colour) is a deliberate constraint that keeps
  the horde cheap to draw and the rules legible.

## Performance target

A 200×120 stage with 5,000 Dead resolves a full turn in under 16 ms on a
mid-range laptop, and a speculated turn commits in under 1 ms, with the
renderer holding 60 FPS. Until measured otherwise, treat this as the budget
every system must fit inside.

## Near-term direction

1. A playable turn-based loop: step, be hunted, reach the exit, next
   stage. (Exists in rough real-time form; PEO-002 makes it turn-based.)
2. Scent layers beyond the player: blood on hit, fire that burns scent away.
3. Hordeling variety through `speed`, sense radius and scent affinity, not
   new AI.
4. Stage generators: caves and ruins behind the same `generate_stage`
   signature.
5. Replays from seed + input log, so bugs can be reproduced headlessly.
