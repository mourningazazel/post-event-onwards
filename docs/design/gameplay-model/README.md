# Gameplay model walkthrough

**Purpose.** Before any engine code is written, decide how gameplay concepts are *represented*:

- what a character is made of
- how belongings are stored
- what is authored as a template vs generated
- how much detail exists and when

The owner decides everything that touches gameplay. Claude proposes options and recommendations,
and handles the technical internals behind each decision.

## How each topic works

- Each topic document lists decisions `D<topic>.<n>`. Each decision has options, a recommendation,
  and **future flexibility**: what the choice makes easy or hard for features we haven't designed
  yet.
- The owner fills in **Owner decision** (or tells Claude, who records it).
- Settled decisions that shape the architecture become ADRs.
- Topics build on each other in order, but can be revisited. A later topic may reopen an earlier
  decision explicitly, never silently.

## Agenda

| # | Topic | Status |
|---|---|---|
| G01 | [The thing model — what everything in the world is made of](G01-thing-model.md) | **Decided** (N015) |
| G02 | [Authored vs generated, and levels of detail](G02-templates-generation-detail.md) | **Decided** (N015) |
| G03 | [Items, belongings and unlimited detail](G03-items-and-belongings.md) | **Decided** (N015) |
| G04 | [Characters — what makes a player or NPC](G04-characters.md) | Draft ready |
| G05 | Bodies, health and physical capability. **Limb/appendage damage (N007); body-part hit rolls with clothing coverage as major defence, no bite infection (N013);** dead fragility follows the clock. | Planned |
| G06 | Actions and interactions: verbs, tools, materials, time costs (digging, sawing, climbing) | Planned |
| G07 | Terrain, materials and structures: tile scale, vertical resolution, walls vs objects. **1 m tiles, one storey per z, shared occupancy (ADR-0013); walls are terrain, fittings are things (D1.1).** | Partly decided |
| G08 | Enemies in the gameplay model: mob records vs individuals, senses, alert, contests | Planned (mechanics in `../scent-mobs.md`) |
| G09 | Perception, knowledge and memory: what the character knows vs what exists | Planned |
| G10 | Time, needs and survival: hunger, thirst, fatigue, temperature, infection | Planned |
| G11 | Persistence as a gameplay rule: what the world remembers, what decays, what resets | Planned |
| G12 | Extensibility: content packs, mod hooks, scripting, future-system slots | Planned |
| G13 | Gameplay rules and playtesting: what must always hold, what may break for realism, standard test scenarios (owner note N001, `../playtest-harness.md`) | Planned |
| G14 | Content model: materials, features and joints, capabilities, reactions, tags; environment-driven mobs and rooms (N008, [`../content-model.md`](../content-model.md)); decisions D-CM1–4 | Draft ready |

**Planned topics — first questions to answer:**

- **G05:** How detailed is a body (whole-body health, limb health, or tissue layers)? How do
  injuries limit actions? Does infection have stages?
- **G06:** Are actions generic verbs checked against capabilities (a tool's "cut 3" vs a
  material's "hardness 2"), or hand-written per item? How long do actions take, and can they be
  interrupted?
- **G07:** How large is one tile (≈1 m)? Is one z-level one storey or finer? Is a wall a tile
  material or an object? Do windows and doors sit inside wall tiles?
- **G08:** When does one of the Dead become a full individual (looted, examined)? Does one of the Dead remember
  who it was (occupation, clothing, pockets)?
- **G09:** Does the character's map memory go stale when the world changes? Can knowledge be
  shared through notes or NPCs?
- **G10:** Which needs exist, how strict are they, and what purpose does each serve? (Research
  found each needs a single, clear purpose.)
- **G11:** Do corpses rot? Does loot respawn? Does the world change while the player is away,
  beyond migration?
- **G12:** Should all content live in data files moddable by players? When would scripting be
  worth its cost?
- **G13:** Which rules are laws the checker enforces, e.g. item conservation, no two units in
  one cell, saving and reloading changes nothing? Which may break for realism, e.g. can the player
  get permanently stuck in a pit they dug? Should the shipped game include a wizard/debug mode?
