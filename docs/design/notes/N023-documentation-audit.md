# N023 — Documentation audit: scripting intent, saves and sync, licensing, turn layer, geography, revisits

- Date: 2026-10-02
- Source: owner, in the project chat ("Documentation audit" thread). Recorded verbatim, including
  the owner's ADR numbers; his "ADR-0006" note is about world generation (ADR-0007/0010), not
  scent.

> Hi, I am doing an audit over existing documentation. I would like you to review these comments
> and adjust the game documentation accordingly. Please break these into small and actionable
> modifications that can be updated in the documentation but also by the builder and architect on
> things it adjusts. Want to save some context on organizing this as it touches a large amount of
> the game engine.
>
> I will be wanting to single the game engine into its own project at some point. I want to
> eventually be able to program the logic and function of the RPG/RogueLike portion of the game
> myself. I will be eventually wanting to create a scripting system for the engine that will allow
> me to manipulate everything from the on screen elements, enemy/skill/item logic, build in other
> features etc. As you noticed, the way the game is setup currently, i had it built where adding
> new features is easy - this will play a direct part in the eventual low-code scripting that will
> work off the game's existing APIs. I will provide more guidance on this later, but please keep
> this in mind so we can easily call different APIs for future development. We will essentially be
> breaking the game elements into similar game engine development processes. "Create Action -
> Desc(Short:"Widdle Wood", Long: "Use a sharp cutting instrument to carve an object"),
> Character Skill{
> 	Requires(item.tool*[ItemActions(cuttingTool)],quantity(1)),
> 	Sharpness(sharp),
> 	Difficulty(24),
> 	TimeCommit(980),
> 	UsesItems(material.wooden*[Size(small)]),
> 	Produces(object.spoon[ItemMaterial(wood),quantity(1)),
> 	FailAction(player.injure(type:cut,damage:10,bodyPart(player.body.arm.hand.finger*,player.body.arm.hand*,multiInjure:false)
> 	}
> If the above action script can be done in lua with a similar structure, we can definitely pursue
> that. I just want you to be aware of how i plan on interacting with the system when i take over
> development.
> The referenced tags and format is only an example to show concept. The end result should be
> something along those lines, and that sort of format would work for most things.
>
> In docs\adr\0002-save-format.md as you build the save format, keep in mind that this will
> eventually be net-enabled, hopefully through SSH as default. When you configure a save file,
> determine what parts make the most sense for sync'ing across remote client sessions.
> Eventually, i will have a server-client -> remote-client relationship, where one user runs the
> world calculations and then changes are sync'd between clients, with each save able to be played
> individually from either as a single player session after. I have been structuring the
> characters to be generic, also to increase player count, and then we are worried more about
> active radius around the players and how that would build into resource bottlenecks. Any sort
> of change in game content, missing items, etc, should have valid methods of adjusting game saves
> to the new structure/content. Missing items becoming generics of similar types, or becoming
> "junk" in the item descriptions as just throw-away items. Please keep versioned save copies that
> can be fallen back to if new version crashes or corrupts the save file. Perhaps keeping 2
> historical saves for fallback.
>
> When you go to configure the UI in the future, we will be wanting to make it easy to
> add/remove/edit onscreen elements. My idea is to have different types of menus (Tab
> menu(pressing tab, cycling through on-screen, anchored to side expanding outwards, screens, one
> at a time), pop up menus, context menus, etc. These will be done in a similar format to the
> actions/items etc, where it'd be something along the lines of <TabMenu><MenuPage =
> "Stats"><row><menu.field.textField( player.conditional.stats.health(field(desc)))></textField>
> <menu.visual.object.bar(colorFilled(#33333),colorEmpty(#222222),currentValue(player.conditional.stats.health(value(currentValue))),maxValue(player.conditional.stats.health(value(maxValue))).
> Please note that this follows a very similar structure, and that structure needs to be
> represented throughout the system. There should be little difference between a player unit and
> an item, and calls to system hooks should follow a similar format.
>
> You mention in the ADR-0004 that "Dependencies must use permissive, GPL-compatible licenses".
> Please be sure that is MIT compatible instead, as that is the licensing of this project.
>
> In ADR-0005, I want to talk about the turn steps. Currently we are doing 6 steps to every turn
> of the player on default speeds. I would like to keep 100% of the same functionality, but for
> every 1 current step, i want the system to actually pass 2 step opportunities. I want the same 6
> actions per turn as we currently have, but have the option to add some inbetween the steps we
> currently have, later. This will be a secondary turn layer that we can use to give a greater
> spread of actions to certain things. It would allow also, a bigger variance of speeds for
> specific units moving, while keeping the current dead's behavior the same (new build should be
> roughly equivalent to the current 6 step turns, but with 12 available). Usage of this secondary
> inbetween layer should be restricted to special units, processes and actions by the user. We can
> also offload small amounts of calculations into this layer if it suits a good performance need.
> Also, while this should not affect actual gameplay, but just the perspective of time passing and
> the relationship between game steps and the global timer, instead of a movement turn taking 6
> seconds, it should take 3. I think everything can be kept as is for calculations, but just have
> the turn represented as 3s rather than 6s, and item usage, sleeping, waiting etc. adjusts for
> that new time period. This shouldn't increase any performance drains, as it is more a
> re-labeling of what that time means, and a readjusting of expectations on step count for certain
> actions.
>
> ADR-0006: Please be sure to include large bodies of water, that could be bordering on a town
> (with applicable lake-side or ocean-side details/decorations/buildings/items).
>
> ADR-0008: No art or text assets may be taken from other sources, everything must be original,
> and i will supply all detailed text descriptors, lore-containing items, and all art. I will be
> using a similar grid structure for tileset that DwarFortress uses, to increase compatibility
> with existing tilesets that users might have. None of these tilesets will be provided, I will
> supply my own as a default aside from the ASCII option.
>
> ADR-0009: I would like you to let places falling far enough out of the player's active range, to
> redo the generation calculations based off the global timer, to affect what is in that previous
> area (including player changes). I am thinking it would be smart to adjust the general horde
> size appropriately, simply comparing current population of that section, and adjusting the
> numbers to match what would be a natural thinning-out from the world timer. Objects
> created/placed/modified by the user will undergo similar calculations to see if any new
> modifications need to be made to the items (unexplored cabinets now searched by other,
> never-seen but assumed, survivors. Barriers the player created now in a diminished
> quality/function or broken/removed altogether. A sealed house might no longer be sealed with
> windows or a door broken down. It shouldn't match the starting scenario's degradation as
> severely, and just rely on the same generative mechanics that the world already has tied to
> that timer, but adjusted to make sense for a re-visited area. Outside of placed objects, there
> is no need to re-generate things such as containers, building types etc. You have design for
> this already, but i just wanted to make sure the intention matched, additional details you have
> or suggest are still applicable.
>
> ADR-0010: I am a fan of this lazy river generation, but i want to make a note here. I want the
> generation of the towns and areas to be reflective of the surrounding geography. A town that
> that has a river flowing through it, should be built around the river with the river in mind
> near it. Bridges crossing the river, river-associated items and structures, etc.. Also, areas
> need to have strong chances to NOT have rivers, as this is supposed to be more representative of
> a normal world, so there is a strong chance, given typical placement, that a city does not have
> a river going directly through it or next to it, even if one is available further out. I would
> suggest the top level geography that is pre-generated, is a large and massive area that is
> unlikely a player would expand upon more (i'm thinking 100km square of basic geography
> (mountains, rivers, lakes, valleys, forests, etc). You can keep the larger-out details minimal,
> probably only referenced in-game as mentions of landmarks being n/s/e/w of the current
> location/city.
>
> ADR-0011: please see my notes on ADR-0009 as these are the same general topic i discussed. The
> big change is the modification of player changed/created objects already placed. Please be sure
> that this global timer does not create a 12am game-time lag as the entire overworld generates a
> new day. This should be considered as a long-term problem as well, incase a player visits a
> large amount of places over a large area and has a long gametime on one character.
>
> Please take a look over the project and see if you can organize things by problem domain, so
> agents working through documentation do not need to take items into context that are unrelated
> to their work. Things such as visions/intent and major design checks and testing still need to
> apply, but i want to segregate some of the work into logical sections.
>
> Please be sure in your roguebasin-notes.md that you link and attribute anything you can, to give
> better recognition to the ideas you are using in practice. Also, please keep this updated with
> major design decisions so it is only representative of what is currently in the game. Also,
> update it for modern tech and features, so you aren't referencing a bunch of stuff you aren't
> even using anymore. Keep it simple and concise as there is a lot of this that is "it suggested
> this", and those sort of notes do not need to live in the document describing what you DID take
> from roguebasin's knowledge set. In fact, this is not a high-fantasy game, and those sorts of
> features will be built into the game engine (not specifically this game), but at a later date
> since it doesn't really matter for a zombie roguelike. And stop saying Heir, a simple
> son/daughter and loose relationship to a previous player character is enough. There are no
> quests. And you will definitely have horror and gore, i dont know why you think there wouldnt
> be. There will be a gradient on descriptors from slightly unsettling to things as graphic as "a
> severed limb soaked in blood and reeking of rot. Several fingers have fallen or been bitten
> off". You can remove the project management aspect in that document, i think you are taking far
> too much context off roguebasin without much use.

## Acted on in

- ADR-0015 (licensing and original assets), ADR-0016 (turn layer), ADR-0017 (save evolution and
  sync), ADR-0018 (geography first), ADR-0019 (revisits re-age player-made things)
- [engine-api.md](../engine-api.md) (scripting, UI markup, one addressing scheme: intent only)
- [world-generation.md](../world-generation.md) "Geography first" and "Revisits"
- [research/roguebasin-notes.md](../../research/roguebasin-notes.md) rewritten
- [vision.md](../../production/vision.md) tone and succession; `docs/domains/`
- Open: D-039 to D-042; deferred queue items PEO-090 to PEO-098
