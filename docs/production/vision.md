# Vision

*Post-Event Onwards* is an ASCII roguelike about being hunted by something
that cannot see you but never stops smelling you. Every design choice serves
three pillars.

## Pillars

1. **Scent is the game.** Enemies do not path-find to the player; they climb
   a scent field the player cannot help feeding. Tension comes from managing
   what you leave behind: where you walk, what you bleed, what you burn.
   Any mechanic that lets the player ignore scent is suspect.
2. **Hordes, not encounters.** Thousands of cheap units, not dozens of smart
   ones. Emergent flanking and flooding from simple rules beats scripted AI.
   This is why performance is a pillar, not a polish task.
3. **Onwards, always.** The world is an endless sequence of stages. There is
   no going back; the exit is the only goal, and each stage is denser than
   the last. Runs end. Seeds make them shareable.

## What it is not

- Not a tactical grid game with unit stats and initiative.
- Not a story game; the setting is told through what you find, never cutscenes.
- Not tile-graphics. ASCII (with colour) is a deliberate constraint that keeps
  the horde cheap to draw and the rules legible.

## Performance target

A 200×120 stage with 5,000 hordelings ticks at 10 Hz on a mid-range laptop
with the renderer holding 60 FPS. Until measured otherwise, treat this as the
budget every system must fit inside.

## Near-term direction

1. A playable loop: move, be hunted, reach the exit, next stage. (Exists in
   rough form.)
2. Scent layers beyond the player: blood on hit, fire that burns scent away.
3. Hordeling variety through `speed`, sense radius and scent affinity, not
   new AI.
4. Stage generators: caves and ruins behind the same `generate_stage`
   signature.
5. Replays from seed + input log, so bugs can be reproduced headlessly.
