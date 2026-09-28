# N014 — The Dead's AI and sound, content depth, skills 1–100, mood and sanity, drugs, maps, guns, chemistry

- Date: 2026-09-28
- Source: owner, in session (reply to the questions stored in the design docs, given to the
  Architect session after it took over design)

> I like how you are doing the item modifiers based off more generic item templates. I think as
> you build out these items, it will be AI managing these lists, so you can be a bit heavy handed
> with details, as long as you think that detail will be worthwhile to future or currently planned
> mechanics. The biggest thing however, make sure that whatever you include details for, makes
> sense enough that a user would be able to use it for that mechanic without relying on a wiki
> article. For buildings, this is going to be one of the biggest visual differences. I want you to
> lay out far more buildings, and i want you to just look at a normal city in real life and see
> what buildings and in what quantity you see. I want there to be random buildings like "retail
> store that sells clothing" and then have items taht would fit that. You will probably need a lot
> of items and modifiers to really create a city. Since there will be so many different items
> needed to account for all these different types, focusing on modifiers as your main way to
> diffentiate, will make not only item generation easy, but world generation as you're mainly
> generating things based off tags, not off of prebuilt item lists. When you do houses for
> instance, think about what kind of person, what kind of hobbies, etc, would go into that house.
> Plan heavy, as the amount of randomness and unique playthrough ability is a strong selling
> point. I want you to offload as much as you can into modifiers and tags, so you can reduce the
> amount of template /generic items. Also, be sure to give quality indicators on all items, so you
> can easily generate time-based degredation as well as situational degredation when generating
> the items. Instead of a "broken" version of an item, you can just include text for what "broken"
> would look like, and you should also identify what tags broken would affect and make the item
> unable to perform. A note for future design - I think having the game perform all the
> calculations on player step is a shame given modern programming - please look into running some
> of the calculations while the player is currently not moving. A lot of the game will not be
> impacted by the player, but calculations still need to run. Would make sense to do those while
> hte player is waiting / mid step, so the next step has less latency on completing. Also, zombies
> that have touched / triggered / seen / heard (if applicable) will now path find on the player.
> Pathfinding regular blind zombies will work via general direction, not specific path. So if you
> end up going into a doorway, they might not really know how to get to you, but will follow your
> direction. After going a set distance away from them, they resume normal behavior, but now will
> be in a different place, adjusting mob generation and attraction. I think mobs should also move
> towards other mobs in a weighted calculation: i think if you have a distribution of zombies, the
> math on their pathfinding should cause them to fan out naturally, due to the randomness of their
> walking. This would cause a natural accumulation towards strong scents, as more and more zombies
> are introduced to the heavier scent tiles. This should replace a lot of mob generation
> mechanics, but i feel that areas with a certain concentration should see more mobs being place
> on screen when exploring adjacent areas. There will need to be a radius determined where the
> units are auto generated and kept as a population number rather than a pathfinding unit. I want
> to balance this on performance, so we can have really large groups that naturally swarm onto
> areas through the pathfinding calculation, and when you do you do tests for the game, please try
> to make sure that such behavior can be replicated whenever we adjust the AI engine. I also like
> the idea that as zombies push past eachother, the idea that they trip, can cause other zombies
> to trip. A horde that is moving in a frenzy, can cause a lot of zombies to just fall over and
> slow down the horde. I think trample mechanics should take a place, where if a unit walks over
> another unit, maybe a random chance and weighted value of damage is applied to the creature
> underneath, blunt damage. You mentioned alerted and follower units recruit idle units within a
> radius, i think this needs to be carefully described. A follower and alerted unit will pull in
> other units, but does not take on the follower or alerted tag, and their pull should be baked
> into the pathfinding rather than a complete new system of follow. I see later on in
> scent-mobs.md you describe how you'd do far off enemy mechanics / generation, and i agree with
> it. You have the general gist of "being swarmed" as the main danger. On the note of triggered
> enemies, i think you have the general idea down, and we just need to work out a balance where
> one triggered enemy mainly brings over new enemies unless those enemies are triggered too. A
> good scenario would be if the leading zombie that can hear you, does attack you, the sound you
> make in retaliation could attract other sound ones, or if one of the following zombies touches
> you, the zombies around it moving in further. For classifying sound events, i think you need to
> keep it simple to help the calculations. Perhaps a type of sound, whether it be mechanical,
> human, etc, as well as dB to determine radius. A triggered sound event would then query the
> surrounding radius for enemy units that react to sound, and follow appropriate mechanics. I
> think sound enemies should move towards the source of the sound, but only follow that or new
> sounds. A player that is "running" would be producing footstep sounds that while not the
> loudest, could easily be heard by an alerted zombie. This would make chase possible with sound
> and visual zombies, where being triggered by a regular zombie is mainly a risk for swarming, as
> you can simply walk away and they can't really chase you far. Just causes a bigger group up
> where you were at, increasing the risk of staying around there. I think to encourage the
> pathfinding to have enemies move in bigger group directions, you should keep bigger groupings of
> scent tiles as an aggregate, and weight enemy pathfinding towards the higher aggregate area. If
> you have a completely even plot of scent tiles, it would make it so if one area has a higher
> aggregate, it naturally pulls the crowd that direction. And then alerted or following enemy
> units tend to pathfind with the following or alerted unit. I think units that are alerted
> should move 1.5x faster per step, where enemies that are actually following the unit should be
> 2x. The player will also have the ability to run, so this should be easy to account for.
> Reading through suvival-actions.md now - you need to be able to do things you would in normal
> life, including drugs. I think you should have a realistic spread of narcotics, pharmaceuticals
> and paraphanelia (including pharmacies with heavily locked and urban locations, and then in the
> future we will build in game mechanics based off taking them. I plan on letting the user take
> psychedelics and then adjusting the visual output of their gameplay, while not really touching
> the underlying mechanics (just visual with sanity / mental toll or benefit. Taking psychs is
> good, unless you see a zombie, etc). a bit of realism. I want you to imagine you generate a Home
> Depot type of store, and how many thousands and thousands of items would be inside. Big
> buildings like that should be almost always in deep urban environments full of zombies, but can
> on chance, be more outside of town, while still heavily populated. Maps should be able to be
> found, but since you dont really see them often anymore, have it be a rare find in a place that
> makes sense. Like if you go to city hall, i imagine you'd find one there, or if you can go to a
> metro bus stop. Reading a map should highlight major locations / stores etc on your in-game
> map, but mainly just put the general location, even if not generated yet, and then have that
> catch up later. The skills i want going on a scale of 1-100, and i want you to imagine that 1 is
> "ive never even heard of that" where 10 is "i can use this fine", 30 being "i am skilled at
> this" with 50 being "i've done this 10 years". Everything above 50 starts getting into Advanced
> and eventually Savant levels of skill. The difference in a level 10 versus 100 skill in
> chemistry would be the difference between making a battery out of different materials, versus
> making pharmaceuticals at a hospital you find. I think skills should be listed as a percentage
> chance to succeed, with failure conditions for not succeeding. If you are building something,
> failing it could result in either a failed build, damaged components, or a bad effect / event.
> The higher your skills, the less chance you have to fail, while never really being 0%. Where
> later game things will be 0% till a certain skill, and then rising as a gradient. Make this easy
> to add / remove as skill difficulty values rather than pre-set ranges. Also, certain things
> should require multiple skills of varying levels, and have your skill level affect those. You
> should see the individual skills and their percentage of working, aggregated into a general
> percentage of success. I also like your idea of being able to use certain tools for things if
> given enough skill. I think this would be a good type of mechanic for things like lock picks,
> that someone with 0 security might not be able to use at all as a tool. As you make these
> items, are thinking about possible game mechanics, know that sanity / mental state will be a
> main mechanic. You should have a Mood mechanic ranging from Depressed to Manic, as well as a
> Sanity function for your grip on reality. Lower values of Sanity will cause hallucinations,
> random gibberish to be spoken, objects that aren't really there, random modifiers to objects
> that aren't really there (like a water bottle being full, but when you finally go to drink from
> it, its empty). Where mood will determine lots of mechanics, but be a relatively hidden
> mechanic in general. I would think that a low mood would affect exhaustion, increase addiction
> chance on drug use, and in general just cause random negatives, where a manic mood might have
> benefits for survival, but also increase risk of failure on sensitive crafting or super high
> mania resulting in mistakes like misfiring a gun etc. For your questions at the end. I think
> bicycles should be ridable, as it would play on the existing terrain mechanics and could result
> in the bike crashing which makes noise, random damage chances to limbs (maybe break a bone, get
> a concussion), and then the sound could alert zombies etc. Firearms, keep it brand-generic or
> with fake brands, but use real specifications. Keep it relatively simple so you aren't having
> 900 types of ammo, but keep it realistic enough that it adds some immersion. If you break into a
> gun store, it should set off an alarm if it has electricity, but also should literally have a
> giant selection of weaponry. Keep in mind that even at the earliest point, it has been a week
> since the outbreak so munitions wouldn't be fully stocked, and higher end weaponry would
> probably remain a rarer item or unfeasibly loud to use. I think having abstract outcomes for
> chemistry is fine, but i want you to take into account real life chemistry with the types of
> items you will have - even if the byproduct is a more generic outcome. I do think explosives
> would be a good add, but i think keeping the ingredients rather generic would be best, and the
> mechanics for it exploding should be kept minimal, even if losing some realism. No wildlife, but
> i want text descriptions of crows in the background, that appears every so often. There aren't
> even really any crows, its just for ambiance.

Naming note: the owner's words above use "zombie"; per D-001 the enemies are **the Dead**
everywhere else, and quotes are never rewritten.

## What this settles

- **Content depth.** Detail may be heavy when it serves a planned or current mechanic, and every
  detail must be usable by a player **without a wiki**. Modifiers and tags are the main axis of
  variety; template items stay few. **Every item carries a quality indicator** for time-based and
  situational degradation. There is **no separate "broken" item**: each item says what broken
  looks like and which tags broken disables.
- **Buildings.** Far more building types, in the proportions of a real city, including generic
  "retail store that sells X" types stocked by category. **Houses are generated from a
  persona** (who lived there, hobbies). Big-box stores hold thousands of items, sit almost always
  in dense urban areas full of the Dead, occasionally outside town but still heavily populated.
  Pharmacies are heavily locked and urban.
- **Compute while waiting** (confirms D-002): the world's player-independent work runs while the
  player is idle, so a step has less latency.
- **The Dead's AI.**
  - Triggered Dead (touched, saw, heard) pursue the player; blind ones by **general direction**,
    not a path (a doorway can defeat them). After a set distance from the player they resume idle
    behaviour, displaced, which changes local attraction.
  - Movement is a **weighted draw** that includes pull toward other Dead and toward the
    **higher-aggregate scent area** (coarse aggregates of scent tiles), so crowds fan out yet
    accumulate on strong scent naturally. This **replaces most spawning**; areas with high
    concentration place more Dead when adjacent areas are explored. Beyond a radius, the Dead
    are a **population number**, not units.
  - Alerted and following units **pull** others through the pathfinding weights only; pulled
    units do **not** become alerted or following.
  - **Trips cascade** when Dead push past each other; a frenzied horde slows itself. **Trample:**
    walking over a downed unit applies a weighted random chance of blunt damage.
  - A triggered unit mainly **brings** others; those become triggered only by their own trigger
    (the player's retaliation sound, contact by a follower).
  - **Speeds:** alerted ×1.5, following ×2 per step. The player can run.
  - **Swarm behaviour must be reproducible in tests** whenever the AI changes.
- **Sound events:** a **kind** (mechanical, human, …) and **dB** for radius; the event queries
  the radius for sound-reactive units. Sound units move to the source and follow only that or
  newer sounds. Running makes footstep sounds an alerted unit can hear, so sound and vision units
  can **chase**; scent-only triggers cause a **group-up where you were**.
- **Skills are 1–100** (1 never heard of it · 10 can use it fine · 30 skilled · 50 ten years ·
  above 50 advanced to savant). Actions show a **percentage chance** with **failure outcomes**
  (failed build, damaged components, bad event); never 0% fail; late-game actions are 0% until a
  threshold then rise as a gradient; difficulty is a **data value**, not preset ranges;
  **multi-skill** actions show each skill's percentage and an aggregate. Some tools are unusable
  below a skill (lockpicks at security 1).
- **Mood and Sanity** are main mechanics. Mood runs Depressed ↔ Manic, mostly hidden, and feeds
  exhaustion, addiction chance, random negatives; mania helps survival but risks failures on
  sensitive crafting and misfires. Sanity is grip on reality: hallucinations, gibberish speech,
  phantom objects and phantom modifiers (a full bottle that is empty when drunk).
- **Drugs:** a realistic spread of narcotics, pharmaceuticals and paraphernalia now; mechanics
  later. Psychedelics change the **visuals** and sanity/mental state, not the underlying rules.
- **Maps** are rare, found where it makes sense (city hall, transit stops). Reading one marks
  major locations on the in-game map by general location, even before generation, and the
  detail catches up later.
- **Bicycles** are rideable; crashes make noise and can injure limbs (fracture, concussion).
- **Firearms:** brand-generic or fictional brands, **real specifications**, few ammo types. Gun
  stores have a **giant selection**, an **alarm if powered**, munitions not fully stocked a week
  in, high-end weapons rare or impractically loud.
- **Chemistry:** abstract outcomes, but grounded in real chemistry for the items involved.
  **Explosives exist** with generic ingredients and minimal explosion mechanics.
- **No wildlife.** Ambient **text** about crows appears now and then; there are no crows.

## Acted on in

- `docs/design/scent-mobs.md` "Owner answers, round 3"
- `docs/design/survival-actions.md` §2.15 skills, §2.16 drugs, chemistry and explosives, maps
- `docs/design/content-model.md` §10 quality and broken, §11 depth rules
- `docs/design/world-catalog.md` §10 building census, persona houses, retail categories
- `docs/design/gameplay-model/G04-characters.md` Mood and Sanity components, skill scale
- `docs/adr/0012-speculative-turns.md`
- `content/registry/skills.toml`, `content/registry/actions.toml` (1–100 rescale)
- `docs/design/purposes.md` new purposes
- `WORK_QUEUE.json` items
