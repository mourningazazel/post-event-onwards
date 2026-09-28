# G03 — Items, belongings and unlimited detail

**Owner requirement:** items can have detail added **without limit on nesting**, so later
features can keep deepening items without being restricted by some "tier-2 item" property set.

## Proposal: every item has exactly one *location*, and nesting is a chain

Items are never "stored inside" another item's data. Each item simply records **where it is**:

| Location kind | Meaning | Example |
|---|---|---|
| `OnGround(x, y, z)` | Lying in the world | A can on the kitchen floor |
| `In(container, compartment?)` | Contents of another thing | A lighter in the *left pocket* of a jacket |
| `Worn(creature, slot, layer)` | Worn on the body | Jacket on *torso*, *outer layer* |
| `Held(creature, hand)` | In a hand | A crowbar in the right hand |
| `PartOf(thing, role)` | Structurally part of another thing | A magazine *in the magazine well* of a rifle; a battery in a flashlight; a label on a jar |

- A jacket's pocket holds a wallet, which holds a photo, which has writing on the back. That is
  just four location links. **Depth is unlimited because nothing is nested in storage**; each link
  is a parent reference.
- "What does X contain?" is answered from an index maintained by the Items system.
- This is the "database question" you raised. It's a **graph of things with typed edges**, the
  same shape a relational or graph database would use.

### Contents vs parts

- **Contents** can be taken out without tools and are held, not attached: bullets in a magazine,
  cans in a bag.
- **Parts** are structure: a magazine *in* a gun, a lens in a scope, a blade on a handle.
  Removing a part may need an action or a tool, and may break the whole.
- Keeping them distinct lets later features (repair, crafting, disassembly, modding weapons) work
  on parts without confusing them with inventory.

### "Detail" is either a property or a sub-thing

- **Property:** fill level, condition, temperature, freshness, an inscription.
- **Sub-thing:** a note tucked in a book, a key on a keyring, a blood stain that can be scraped off
  and sampled.
- A future feature can add a new component type (e.g. `Fingerprints`) or new sub-things to any
  item, with no change to existing item types. The save system keeps unknown components intact.

### Substances vs items

- Liquids and granular matter (water, fuel, sand, pills) are **substance + amount** held in a
  container. They are not individual items.
- A bottle holds `water: 0.4 L`, not 400 "water" items.

## Decisions

### D3.1 — Capacity model (what fits where)

- **A. Slot count:** "10 slots", as in many games.
- **B. Weight only.**
- **C. Volume + weight + largest dimension:** real-world style. A 30 L backpack, a small pocket; a
  rifle is too long for a pocket.
- **Recommendation: C.** It matches the "physically plausible" direction.
- **Future flexibility:** encumbrance, awkward loads, strapping items *onto* a pack (a `PartOf`
  or `Attached` location).
- **Owner decision:** _pending_

### D3.2 — Stacks

- **A. Every item is individual,** even 500 nails.
- **B. Identical items merge** into one thing with a quantity. They split when one differs (e.g.
  one nail gets bent).
- **Recommendation: B**, with merging allowed only when *all* properties are identical. Detail is
  never lost; the stack just splits.
- **Owner decision:** _pending_

### D3.3 — Where do belongings live on a character?

- **A. An abstract inventory list.**
- **B. Physically:** items are held in hands, worn on body slots in layers, or inside worn
  containers (pockets, bags, holsters). The "inventory screen" is a *view* of that tree.
- **Recommendation: B.**
  - What you carry depends on what you wear.
  - Losing a backpack loses its contents.
  - A zombie grabbing your jacket matters.
  - Pockets on clothing are just containers.
- **Owner decision:** _pending_

### D3.4 — Item identity

- **Recommendation:** every instantiated item gets a permanent ID. Unobserved items are addressable
  by generation address (G01 D1.5).
- History (who owned it, where it was found) is an optional component. It is added only when a
  feature needs it, so there is no cost otherwise.
- **Owner decision:** _pending_

### D3.5 — Condition model

- **A. One durability number.**
- **B. Condition per part** (the rifle's barrel, stock and magazine each have their own), with the
  whole item's effectiveness derived from its parts.
- **Recommendation: start with A on simple items and B on items made of parts.** It follows
  automatically from the parts model.
- **Owner decision:** _pending_
