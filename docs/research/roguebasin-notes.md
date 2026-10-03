# What we took from RogueBasin

Credit for the ideas this game uses from [RogueBasin](https://roguebasin.com/), and what we made
of them. Only what is in the code or an accepted decision is listed; when a decision drops an
idea, its line goes too. Our implementations are our own (ADR-0015, `docs/LICENSING.md`); the
articles are credited to their authors where the page names them.

Pages were read on 2026-09-27. RogueBasin asks that large blocks of its text not be copied
([RogueBasin:Copyrights](https://roguebasin.com/index.php/RogueBasin:Copyrights)), so this file
holds our summaries and short quotes only.

## In the code

| Idea | Source | What we built |
|---|---|---|
| Distance fields that many agents share: relax goal cells outward, agents step downhill; *"adding more monsters to the map is cheap"* | [The Incredible Power of Dijkstra Maps](https://roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps) (Brian Walker, *Brogue*); [Dijkstra Maps Visualized](https://roguebasin.com/index.php/Dijkstra_Maps_Visualized) | The scent field: an integer geodesic field, strength minus route length minus age, spread as a vectorised pull, bit-identical on threads and the GPU (D-024, ADR-0014) |
| Monsters tracking a scent left by the player | [Tracking by Scent and Sound](https://roguebasin.com/index.php/Tracking_by_Scent_and_Sound); [An Alternative Scent Implementation](https://roguebasin.com/index.php/An_Alternative_Scent_Implementation) | Scent as the core enemy sense: the Dead are blind and climb the field (ADR-0006) |
| Weighting several desires per agent and choosing among moves by weighted chance | Dijkstra maps article, "desire-driven AI"; [Ratio AI](https://roguebasin.com/index.php/Ratio_AI) | The Dead's weighted draw over the eight neighbours and staying, sharper as scent strengthens (D-038), from a per-cell desire field |
| Dense per-tile arrays for hot queries, sparse lists for entities | [Data structures for the map](https://roguebasin.com/index.php/Data_structures_for_the_map) | Flat grids for terrain, blocking and scent; units in their own arrays |
| A level's seed is a hash of the world seed and its coordinates, so it regenerates on demand | [Dungeon persistence](https://roguebasin.com/index.php/Dungeon_persistence) | Stage seeds from (world seed, stage index) (`stage.hpp`) |
| Never `rand()`; separate streams for generation and play | [Random number generator](https://roguebasin.com/index.php/Random_number_generator) | Our own splitmix and xoshiro, plus counter-based hashes so parallel draws need no shared stream (ADR-0014) |
| Turn-based, grid-based, permadeath as deliberate choices | [Berlin Interpretation](https://roguebasin.com/index.php/Berlin_Interpretation) | The world moves only when the player acts (ADR-0012); hard permadeath (N003) |

## In accepted design, not built yet

| Idea | Source | Where it lands |
|---|---|---|
| Save the seed and RNG state, stable IDs, remap tables across versions, validate every value, warn before a lossy conversion | [Save Files](https://roguebasin.com/index.php/Save_Files) | ADR-0002, ADR-0017. We use binary, checksummed, compressed sections rather than the text the page prefers |
| Content as editable data files with no logic, later scripts returning tables | [Info Files](https://roguebasin.com/index.php/Info_Files) | `content/` TOML now; Lua tables later (`docs/design/engine-api.md`) |
| One item lifecycle and one entity-reference scheme, designed up front | [Code design basics](https://roguebasin.com/index.php/Code_design_basics) | Stable 64-bit IDs shared by the world and saves (ADR-0002); items as seeds (D-023) |
| Fixed damage to a random body part, which resolves fast in crowds | [World of Rogue](https://roguebasin.com/index.php/World_of_Rogue) | Bites and blows roll a body part (N013, G04) |
| Rivers as a few waypoints, perturbed and smoothed | [Winding ways](https://roguebasin.com/index.php/Winding_ways) | Lazy river paths through control points with seeded meander (ADR-0010, ADR-0018) |
| Permadeath with the world going on | [Permadeath](https://roguebasin.com/index.php/Permadeath) | The world persists; the next character is a son or daughter of, or loosely tied to, the last, and starts a mile or two away (vision) |

## Where we went our own way

- **Tone.** The game is horror and gore on a gradient, from slightly unsettling to graphic
  ("a severed limb soaked in blood and reeking of rot"), not the restraint some articles advise.
- **Scale.** RogueBasin has nothing on hordes of tens of thousands or on city generation; those
  come from our own measurements (`docs/research/parallel-and-gpu.md`, `docs/bottlenecks.md`).
- **Modern hardware.** Fields run as integer stencils across threads and on the GPU through
  Vulkan (ADR-0001, ADR-0014), where the articles assumed one slow core.
