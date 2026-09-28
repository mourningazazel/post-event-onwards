# G02 — Authored vs generated, and levels of detail

## Proposal: author the *parts and rules*, generate the *combinations*

| Authored (data files, hand-written) | Generated (procedural, deterministic from seed) |
|---|---|
| Materials and their physical properties | World terrain, rivers, lakes, landmarks |
| Item templates ("kitchen knife", "30 L backpack") | Which cities exist, where, and their identities |
| Creature archetypes and body plans | City layout, streets, lots |
| Building types and room programs ("suburban house: 2–4 bedrooms, kitchen, …") | Each building's floor plan (from its type's rules) |
| Loot and placement tables ("kitchen drawer: cutlery 60%, batteries 5%, …") | Which items exist in which containers (rolled from the tables) |
| Scent channels and their dominance matrix | Per-instance variation: condition, fill level, wear, labels, handwriting |
| Actions and recipes | Names, occupations and backstories of NPCs and former people (Dead) |
| Name lists, text fragments | Notes, inscriptions and other text assembled from fragments |

Rule of thumb: **if a designer would want to tune it, it's authored. If it should differ every
playthrough, it's generated from authored rules.**

## Levels of detail for any thing or place

| Level | What exists | Example | Stored? |
|---|---|---|---|
| **1 Implied** | Only a statistical expectation | "This house probably has kitchen stuff." | No; derived from the place's type |
| **2 Identified** | The thing's identity and shape, but not its insides | "A kitchen cabinet (contents unknown)." | No; regenerable from the seed |
| **3 Instantiated** | Full detail | "Cabinet: 3 cans of beans (one dented), a can opener, a dead mouse." | Yes, frozen once generated (ADR-0002) |
| **4 Changed** | Instantiated, then altered by play | "Cabinet: empty, door broken." | Yes |

The same ladder applies to places: a city (identified) → its buildings (identified) → a building
(instantiated floor plan) → its rooms' contents.

## Decisions

### D2.1 — When does hidden detail generate?

- **A. By proximity:** everything within *X* tiles becomes instantiated.
- **B. By need:** visible surfaces generate by proximity; **hidden contents** (containers, pockets,
  under the floorboards) generate when first **opened or searched**.
- **Recommendation: B.**
  - The cost follows what the player actually does.
  - The result is identical to generating it earlier, because it's seeded, so nothing is lost.
- **Future flexibility:** skills or tools that reveal contents without opening (X-ray, a stethoscope
  on a safe) just trigger generation earlier.
- **Owner decision:** _pending_

### D2.2 — Do instances copy their template, or reference it?

- **A. Copy:** each item stores all its properties.
- **B. Reference + delta:** each item stores its template ID and **only what differs** (condition
  70%, contents, a name scratched into it).
- **Recommendation: B.**
  - Saves are much smaller.
  - Improving a template (a rebalanced knife) updates every existing knife, except the properties
    that particular knife has changed.
- **Trade-off:** a balance patch changes items in existing saves. That's usually desirable. If a
  property must *never* change retroactively, the instance pins it.
- **Owner decision:** _pending_

### D2.3 — Template inheritance

- **A. Flat:** each template stands alone.
- **B. Single inheritance plus traits.** For example, `kitchen knife` → `knife` → `blade tool`,
  plus traits like `metal`, `sharp`, `small`.
- **Recommendation: B.** Content authoring scales, and one change to `knife` fixes every knife.
- **Owner decision:** _pending_

### D2.4 — How are generated places kept stable?

- **Recommendation: freeze on instantiation** (already accepted in ADR-0002).
  - The first time a building's floor plan or a container's contents is instantiated, it's saved.
  - It never re-rolls, even if a game update changes the generators.
  - Untouched, never-instantiated areas pick up new generator versions.
- **Question for the owner:** is it acceptable that *unexplored* parts of an existing save may
  look different after a game update (better generators), while explored parts never change?
- **Owner decision:** _pending_
