# N016 — Answers to the full codebase review: wind, character creation, sound, GPU, z-levels, sealed buildings, indoor walls, budget, Dead speed, items

- Date: 2026-09-29
- Source: owner, in the "Full codebase review" project thread, commenting on the review
  top to bottom (`/mnt/project-files/reviews/2026-09-29-full-codebase-review.md`)

> I will comment on this code review as i read through it start to bottom. For scent reach with
> it "getting ahead of you" you aren't really thinking of wind are you? If you have wind blowing
> against your back, it should affect the dispersion of the scent, with enough wind speed it
> should drift farther in front than you are walking. For G04 D4.3, on character creation, you
> will have a completely random character each time - as design. This will include not only
> stats, starting items, but also a background, full physical makeup (mainly just flavor text,
> littered with tiny details that show deeper specifics), and things such as psychiatric, medical
> and physical limitations/conditions. These will be random per-playthrough, so sometimes you are
> just really unlucky. for G04 D4.5, i think you are on the money with the Owner Input comment. I
> think physical stats should be numeric, but all other stats about the character are
> descriptive, including their skills. I think for skills we should decide on 5 generic "skill
> level" descriptions ranging from 1-20 up to 81-100 levels, tiered at each 20. Physical
> attributes would come across in a character description, that i would like broken up by
> categories of descriptors (what they look like broken up by physical traits (hair = length,
> color and style mention, strength represented as descriptions of physique, and mental/physical
> state/conditions referenced as descriptions of the ailment) , preferences that can give mood
> boosts, randomly generated history broken up into "who you are" including things like previous
> job (which affects character gen and starting skills), but also a section on "Where were you?"
> that describes where the player was at the beginning of the Event; it shouldn't match the
> starting area / condition exactly, but should be somewhat near. If a person was an office
> worker, probably start them somewhere near the heavier urban area. If they were a cop, they
> start out with a gun and firearms skill. For sound classification you will want Human,
> mechanical and ambient. Ambient noise will display as a descriptor when examining your
> surrounding environments (as an action), which will help dictate the ambient noise floor, which
> should really only come into play when determining if the audio-driven dead can hear the player
> (same as human vs rot scent, an inverse relationship on if they see you). For the GPU question
> in section 1 point 5, i need a bit of background on this. You know what i am trying to do, and
> with what we've added so far, i am sure you can imagine the resource drain will increase as
> development goes on. Do you feel it is worth it to aim at the GPU level, or keep it CPU? Aiming
> at GPU does introduce some complexity that CPU alone wouldn't, but once we get this game working
> I do plan on increasing things like horde size, granularity on certain things, and adding more
> checks in that could increase the latency when resolving calculations. Your call on what you
> think is best considering that. I am looking through this testing however, your testing is not
> taking into account multiple Z levels. While scent does not travel "in the air" above the ground
> tiles, since there is multi-z level traversal, i believe that scent should have downward tiles
> available as a route of dispersion, if those are are available. I think this would essentially,
> if the downward tile is just treated as an additional route, be easy to accommodate. If you
> multiply this across many buildings, it would quickly make a 512x512 map grow with far more tile
> spots to keep track of. Given that, I think that z-level dispersion should only take place in a
> radius of the player, relying on the aggregates or other methods of tracking for when the
> player isn't actively there. I don't care about the scent drifting through a stairwell in some
> house i'm not even inside of. We are countering "long-stay" holdings with soak values, so we
> don't need to be super granular about the actual dispersion inside the house, when it isn't
> even close to us to care. In fact, probably take that logic and apply it elsewhere to save
> compute. A completely sealed building will only waste resources having its tiles counted (which
> shouldn't be changing in an enclosed environment anyways) and "opened" buildings can be caught
> as a generative tag when created (does it have broken windows, is the door open, etc). Those
> buildings can be caught in the same scent map generation when you approach the building, and
> again, rely on the aggregates when away. for cramped layouts, i'm confused why this is an issue
> and am curious if you are just thinking about it wrong. If you are in a 1x8 corridor leading to
> a 3x3 room and you stand at the far end of the 1x8, it logically makes sense that your buildup
> would flow down the hallway and into the 3x3. While the walls are absorbing a lot of what would
> flow, you still have a single line towards the 3x3 that it should flow out into. If going
> around two L corners, that would result in no scent traveling in a straight line, and being
> absorbed by the walls. I think this is easier to accommodate with special mechanics if you do
> not integrate those mechanics into the base scent calculations. Outside, it is unlikely and can
> just be considered impossible, to be in some long hallway, especially turning multiple times,
> so you can keep the logic for walls and barriers specific to houses. To do this, i wouldn't
> adjust the base scent calculations, but account for it inside the house. If we are not doing
> scent dispersion on closed buildings, this should reduce the overhead. I think a good starting
> point, feel free to expand upon it, if we are having scent travel through a cramped space, wall
> absorption eating away at any sort of real spread, i think INNER walls should react with a
> buildup mechanic, as there isn't really any upper z levels (worth mentioning) that it would
> naturally flow into with the wind, like outside would. You also need to make sure that wind
> isn't blowing inside, and perhaps putting some sort of extra stop in the calculation would let
> you do that as well. For the levels you are testing, 16ms is pretty short, if you're
> considering the minimum time a player can move being 1/3rd a second. When doing this testing,
> also include larger map variants with far more enemies so we can track the high end of resource
> utilization too. If we can arrive at a good engine but have 80% headroom on compute, we can
> easily up the mob count and increase variation in generation. Also keep in mind, i'm running
> the local copy on a mac m1 air, so modern PCs will probably blow my performance out of the
> water. Makes my PC a good test bed though so we can see low end specs in performance. for 5.3,
> Between the first and third options, I am fine if we want to adjust dead speed on a "higher
> compute if fast" but I do like the probability approach as it would allow up to 6 spaces of
> movement per 1 6 second step. Not that i want anything really that fast yet, but that's how im
> reading it. If you think that is a good approach and good on resources, i'm fine with the
> gameplay element. Even faster dead could move 2 extra steps instead of 1. On the items in 5.4,
> i agree, an unopened container sits as a seed until opened and instantiated, and a physical
> representation of those exact items lives just as the calculation needed to create it, till the
> item is actually modified from its original space. On 2. i do agree on no per-turn iteration
> through items. We have a global clock already, so we can resolve that on actually accessing the
> item. since this could result in a "open inventory and lag for 2 seconds" scenario as it
> updates all those records (could be 300 items in the backpack, idk), you can probably just have
> the name resolve to the needed iterated version, and any details just resolve from there. Each
> time you access that item probably just follow that, unless you think the overhead on just
> updating the records on view (open the backpack) is low enough cost (even at 500 items) that it
> is worth doing to save compute later. Its not likely they will have 500 items, but i dont want
> the game breaking just because someone decides to do so and can physically do so. If you need
> to implement a max item count for containers, im fine with that as long as its stupid high and
> not going to actually interfere with normal gameplay. Since we have other systems denoting what
> you can and cannot fit, probably just make it a hidden mechanic and disallow adding more (make
> sure you allow items that grow from other item stacks, and doesn't crash the game saying "hey i
> can't fit anymore and the computer told me i needed an extra item in this bag". I think only
> items that need resolved and stored are those that would not match their original seed, and
> even then, all changes to the item should stick to the seed for reading the properties, unless
> that is changed and then the item is instantiated as a new unique item. If that doesn't really
> save compute, whatever you think is best. I see your section on pathfinding in 6 and you can
> just read my answers above on that topic before coming back with new suggestions or concerns.
> You can now dish out the work as you see fit, i have 2 other reviews i need to look through
> before giving the builder work.

## What this settles

- **Wind carries scent ahead of a moving player** when it blows from behind; with enough wind
  the scent front outruns the walker. The review's "the Dead must never head off a moving
  player" reading (D-008) is corrected: they may, downwind. Round 1 of `scent-mobs.md` already
  had regional wind; the field must actually do it (D-020).
- **D4.3: character creation is fully random, by design** (D-016): stats, starting items, a
  background, a full physical makeup as flavour text with small telling details, and
  psychiatric, medical and physical conditions. Some characters are simply unlucky.
- **D4.5: physical stats are numeric; everything else is descriptive, skills included** (D-017).
  Skills show as five level descriptions tiered at 20 (1-20, 21-40, 41-60, 61-80, 81-100). The
  character sheet is prose grouped by categories: appearance by trait (hair length, colour,
  style; strength as physique; conditions as the ailment), preferences that give mood boosts,
  and a generated history in two parts: "who you are" (previous job, which seeds skills and
  starting gear: a cop starts with a gun and firearms skill) and "where were you?" at the
  Event, which places the start near a fitting area (an office worker near the dense urban core)
  without matching it exactly. This amends N014's "skills as percentages".
- **Sound kinds are human, mechanical and ambient** (D-018). Ambient noise is a descriptor
  when the player examines their surroundings (an action) and sets an **ambient noise floor**
  that matters only for whether hearing Dead can hear the player, the inverse relationship
  human scent has with rot.
- **GPU or CPU is the Architect's call** (D-021 records it: CPU only, integer-valued fields) given that horde size, granularity
  and the number of checks will all grow.
- **Z-levels: a downward tile is one more route for scent** where one exists; scent still does
  not travel in the air above ground tiles. **Z-level dispersion runs only within a radius of
  the player**; elsewhere aggregates and other coarse tracking stand in (D-019).
- **Apply that everywhere to save compute.** A **sealed building** is never simulated; its
  tiles cannot change. **Opened buildings** get a generative tag at creation (broken windows,
  an open door) and join the scent map when the player approaches; away from the player the
  aggregates carry them. Soak already covers long stays, so indoor dispersion far from the
  player need not be granular (D-019).
- **Cramped layouts:** scent must flow down a 1x8 corridor into the room beyond; two L-corners
  and absorbing walls may stop it. Long turning corridors do not occur outdoors, so wall and
  barrier rules are house-specific and sit **outside the base scent calculation**. The owner's
  starting point, open to expansion: **inner walls build up** rather than absorb (there is no
  upper air to vent into), **no wind blows indoors**, with an explicit stop in the calculation
  (D-020; the solver choice is D-024).
- **Budget:** 16 ms is too short when the fastest move is a third of a second. Perf tests must
  add **larger maps with far more Dead** to see the high end; 80% headroom would go to horde
  size and generation variety. The **M1 Air is the low-end test bed** (D-021).
- **Dead speed (review 5.3): the probability approach**, allowing up to six tiles per 6 s update
  as it reads; faster Dead may take two extra steps rather than one. The Architect confirms it
  is cheap (D-022).
- **Items (review 5.4):** an unopened container is a seed until opened and instantiated; the
  items exist as the calculation that creates them until one is modified. No per-turn iteration:
  the clock resolves an item on access, name first and details from there, unless updating all
  records on view (a 500-item backpack) is cheap enough. A hidden, very high per-container item
  cap is fine; it must never block items that grow out of existing stacks or crash. Only items
  that no longer match their seed are stored; changes read through the seed until a property
  changes, then the item becomes unique (D-023; the item-system thread's own questions are D-026 to D-029).
- **Pathfinding (review 6):** the owner's answers above are the input; new suggestions come
  after them. The transport question is D-024.

## Acted on in

- `docs/production/decisions.md` D-016 to D-023; `docs/production/DECISIONS_NEEDED.md` D-024 onward
- `docs/design/gameplay-model/G04-characters.md` D4.3, D4.5
- `docs/design/scent-mobs.md` "Owner answers, round 4"
- `docs/production/vision.md` performance target
- Queue: PEO-042 to PEO-051; notes on PEO-004, 009, 030, 035, 039; PEO-031 and PEO-032 handed to the Builder
