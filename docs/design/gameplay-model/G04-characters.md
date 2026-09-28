# G04 — Characters: what makes a player or NPC

Per G01 D1.2 (recommended), the player's character is an ordinary **creature**, driven by a player
controller. **There are no NPC survivors** (owner note N005); the model still keeps the player
non-special, which enables turning into one of the Dead and whatever the owner adds later. Dead share the body model, but usually exist
as compact mob records (G01 D1.3).

## Proposal: a character is these components

| Component | Contains | Why it exists / what it enables later |
|---|---|---|
| **Identity** | Name, age, background/occupation, appearance notes | Backstory, NPC recognition, dead "former lives", heirs |
| **Body** | A body plan from a template (human, dog, …) with per-part state | Injuries, bites, amputations, wearing and holding (G05) |
| **Attributes** | Physical and mental capacities | Checks and contests (pushing, carrying, climbing) |
| **Skills** | Learned proficiencies | Actions succeed faster or better (G06) |
| **Traits** | Permanent quirks (asthmatic, strong, light sleeper) | Character variety without special-case code |
| **Conditions** | Temporary states (bleeding, infected, exhausted, soaked, prone) | Status effects as data |
| **Needs** | Hunger, thirst, fatigue, warmth… | Survival pressure (G10) |
| **Mood** | One axis, Depressed ↔ Manic; mostly hidden from the player (N014) | Feeds exhaustion, addiction chance and random negatives when low; survival benefits but failure risk on sensitive crafting and misfires when manic |
| **Sanity** | Grip on reality (N014) | Low values cause hallucinations: gibberish speech, phantom objects, phantom modifiers (a "full" bottle that is empty when drunk); psychedelics move it and change visuals only |
| **Equipment** | Physical: hands, worn slots and layers, worn containers | See G03 D3.3 |
| **Senses** | Which senses, and how good they are | Symmetric with Dead: the player may *also* sense scent faintly |
| **Emissions** | **Scent profile per channel**, noise when moving and acting | Drives the core mob mechanic; masking changes this profile |
| **Knowledge** | Map memory, known places, recipes, facts | What the character knows vs what exists (G09) |
| ~~Relationships~~ | *Removed: no NPCs (N005)* | — |
| **Controller** | Player, AI behaviour, or none | Possession, companions, turning into one of the Dead |

Any of these can be missing. One of the Dead has no Skills or Knowledge; a dog has a different body plan
and senses. A future system adds a new component without touching existing characters.

## Decisions

### D4.1 — Attribute set

- **A. Classic RPG six** (STR, DEX, CON, INT, WIS, CHA).
- **B. Physical-realism set:** strength, endurance, agility, dexterity (fine motor), perception,
  plus mental: focus and composure.
- **C. No attributes:** everything derives from body state and skills.
- **Recommendation: B.**
  - It maps onto real actions (digging uses strength and endurance; lock work uses dexterity).
  - It suits "what you could physically do in real life".
  - A mental set is needed later for fear and stress.
- **Owner decision:** _pending_

### D4.2 — Skill model

- **A. Class and level:** points spent on level-up.
- **B. Use-based:** skills improve by doing the related action, with diminishing returns.
- **C. Background-seeded, then use-based.** Your occupation (e.g. a nurse) sets starting skills;
  after that you learn by doing.
- **Recommendation: C.** Every start is meaningfully different, it makes the apocalypse backstory
  matter, and it needs no XP abstraction.
- **Owner input (N012, N014):**
  - Skills train at an unrealistic rate, up to **beyond-human** levels. Scale is **1–100**
    (1 never heard of it · 10 can use it fine · 30 skilled · 50 ten years · above 50 advanced
    to savant); see `survival-actions.md` §2.15 for the success-percentage model.
  - **Reading** found books and manuals is a real learning path alongside use.
  - Option C therefore gains a third source: background-seeded, then use-based **and reading**.
- **Owner decision:** _pending_ (confirm option C, with reading as a learning source)

### D4.3 — Character creation

- **A. Fully chosen by the player.**
- **B. Fully random:** you are whoever you are.
- **C. Pick from a few generated candidates** in the starting city (e.g. three people with
  backgrounds, traits and starting gear).
- **Recommendation: C.** Choice without min-maxing, and it fits "cities with identity".
- **Owner decision:** _pending_

### D4.4 — What happens at death?

- **A. Permadeath:** new world or new game.
- **B. Succession:** continue as another survivor in the same persistent world. Your old character
  may rise as one of the Dead with your gear.
- **C. Both,** as a difficulty option.
- **Recommendation: C, with B as the default.**
  - The persistent, endless world makes continuity valuable.
  - "Meeting your former self" is a strong emergent moment.
- **Owner decision (2026-09-27, N003): A, hard permadeath.** Death ends the run. Bad luck and bad
  decisions are allowed to end a 50-hour character.
- **Follow-up for the owner:** does the next run start in a **new world**, or as a new character
  in the **same persistent world**, where the previous character's body (maybe risen) and gear
  could be found?

### D4.5 — How much of the numbers does the player see?

- **A. Everything numeric.**
- **B. Descriptive:** "exhausted", "badly bleeding", with numbers optional in a detail view.
- **C. Hidden.**
- **Recommendation: B.** Descriptive by default, with an optional numeric toggle for players who
  want it.
- **Owner input (N014):** skills and action success are shown as **percentages** with the
  contributing skills listed; Mood stays mostly hidden. So: numbers for skills and chances,
  descriptive for body and needs unless toggled.
- **Owner decision:** _pending_ (confirm the split above)

### D4.6 — Can the player be infected and turn?

- Story-dependent, so this question is for the owner.
- The model supports it either way: infection is a condition; turning swaps the controller and
  archetype.
- **Owner decision (N013): bites do NOT infect** (it was a demonic event).
  - The player takes ordinary damage, with health mechanics.
  - Damage is **body-part specific**: a hit roll selects a part, and that part's clothing coverage
    and armour decide the outcome. A thick jacket stops bites; a bare neck is deadly.
  - Clothing is a **major** defence.
