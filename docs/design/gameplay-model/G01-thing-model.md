# G01 — The thing model: what everything in the world is made of

This is the most foundational decision. Every later topic (characters, items, enemies, actions,
persistence) builds on the categories chosen here.

## Proposal: four kinds of "stuff"

| Kind | What it is | Examples | Why it's separate |
|---|---|---|---|
| **Terrain** | The matter of the world grid: one material and shape per cell | Soil, rock, water, asphalt, brick wall, concrete floor, air | There are billions of cells. They must be compact arrays, not objects. Digging and sawing act on *matter*. |
| **Things** | Discrete objects with identity | The player, survivors, zombies, a knife, a backpack, a door, a car, a fridge | Things carry individual state and can be moved, owned, nested, examined |
| **Fields** | Continuous quantities over space | Scent channels, light, temperature, wind | Things and terrain emit or block them; creatures sense them |
| **Places** | Named identities with no body | A region, a city, a building, a room, a lake (as a landmark) | Places carry meaning: name, type, owner, history, "the kitchen". Quests, knowledge and generation refer to them. |

## Decisions

### D1.1 — Is a wall terrain or a thing?

- **A. Terrain:** walls, floors and ceilings are materials in cells.
- **B. Things:** walls are objects placed on cells.
- **C. Hybrid:** walls and floors are terrain; *fittings* are things placed in or on terrain
  cells. Fittings include doors, windows, stairs, furniture and fixtures.
- **Recommendation: C.**
  - Sawing through a wall or digging a hole is a change of *matter*, so it's cheap and uniform
    with terrain.
  - A door or window needs state (open, locked, broken) and identity, so it's a thing that sits
    in a wall cell.
- **Future flexibility:**
  - New materials (reinforced concrete, sheet metal) are just data.
  - New fittings (vents, hatches, barricades the player builds) are things, with no terrain
    changes.
  - Player-built barricades could be either. Rule of thumb: *if it has state or can be carried,
    it's a thing.*
- **Owner decision:** _pending_

### D1.2 — Is the player special?

- **A. The player is a unique object** with its own rules.
- **B. The player's character is an ordinary creature.** It follows the same rules as any
  survivor NPC, and a **controller** (player, AI or none) is attached to it.
- **Recommendation: B.**
- **Future flexibility.** This makes these possible later, almost for free:
  - switching characters
  - heir or succession on death (the research flagged this as a good fit for permadeath)
  - companions using the same actions
  - the player **turning into a zombie** after infection (swap the controller and archetype)
  - NPCs doing everything the player can (looting, digging)
- **Cost:** player-only conveniences must be written as rules that *could* apply to anyone.
- **Owner decision:** _pending_

### D1.3 — Are zombies the same kind of thing as humans?

- **A. Different kinds:** zombies are simple monsters.
- **B. Same kind in principle, different detail tier.**
  - Every zombie is conceptually a creature with a body, clothes and pockets.
  - Most exist only as a **compact mob record** (archetype, position, a few numbers).
  - A zombie is **promoted** to a full creature when something needs the detail: it is examined,
    killed and searched, or grabs you.
  - Its detail is generated from its record and seed, e.g. a former office worker with a badge
    and keys in the pockets.
- **Recommendation: B.** Mob performance stays as benchmarked, and every zombie can still be a
  person with a story when the player looks closely.
- **Future flexibility:** zombie variants, visible "former lives" (police zombies carry police
  gear), and loot that makes sense.
- **Owner decision:** _pending_

### D1.4 — How is a thing composed?

- **A. Fixed classes:** Weapon, Food, Container…
- **B. Composition:** a thing = template + **components** (typed data the engine understands,
  e.g. `Container`, `Wearable`, `Edible`, `ScentEmitter`, `Body`) + an **open property layer**
  (tags and key/values for features not built yet).
- **Recommendation: B.** A bottle can be a container *and* a weapon *and* fuel without any class
  hierarchy. New features add new component types and never modify old ones.
- **Future flexibility:** very high. This is the answer to "later game features can add more and
  more logic and detail".
- **Owner decision:** _pending_

### D1.5 — Can things be referenced before they exist?

- **A. No:** only generated things can be referenced.
- **B. Yes, through a generation address.** For example: "city 17 → building 402 → room
  *kitchen* → container 2 → slot 3". The address resolves to the same thing whenever that place
  is eventually generated.
- **Recommendation: B.** A note found today can point to "the pharmacy on 5th street's back room
  safe" before that building has any detail. Quests, rumours and maps can reference the unseen
  world.
- **Future flexibility:** quests, NPC knowledge, treasure maps, radio broadcasts.
- **Owner decision:** _pending_
