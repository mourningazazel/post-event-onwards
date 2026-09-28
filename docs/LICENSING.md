# Licensing

Policy: see [ADR-0004](adr/0004-open-source-and-dependency-licensing.md). In short: open source,
permissive dependencies only, every dependency verified and recorded here.

## Project license — decision pending

| Option | Effect | Fits if… |
|---|---|---|
| **GPL-3.0-or-later** | Anyone may use, modify and sell it, but distributed forks must stay open under the GPL. This is the roguelike tradition (Angband and DCSS use GPLv2+, Brogue CE uses AGPL). | You want the game and any derivative to stay open |
| **MPL-2.0** | Modified *files* must stay open, but they can be combined into closed products | You want engine improvements shared back without forcing whole projects open |
| **MIT or Apache-2.0** | Anyone may reuse anything, including in closed-source products. Apache-2.0 adds an explicit patent grant. | You want maximum adoption of the engine |

All candidate dependencies below are compatible with every option.

Two related decisions:

- **Original art and text assets** are usually licensed separately, e.g. **CC-BY-SA-4.0** or
  **CC-BY-4.0**.
- **Outside contributions.** If the project may ever be relicensed or sold under different terms,
  decide early whether contributors sign off under a DCO or a CLA.

## Candidate dependencies

The license of each was verified on 2026-09-27 via the GitHub license API (the `LICENSE` file was
read where GitHub reported `NOASSERTION`).

| Library | Use | License (SPDX) |
|---|---|---|
| SDL3 | Platform, input, GPU | Zlib |
| SDL3_image | PNG loading | Zlib |
| SDL_shadercross | *(only if D3D12 or Metal are ever added)* | Zlib |
| flecs | ECS candidate | MIT |
| EnTT | ECS candidate | MIT |
| enkiTS | Job system | Zlib |
| FastNoise2 | Noise | MIT |
| toml++ | Data files | MIT |
| zstd | Save compression | BSD-3-Clause (dual BSD / GPLv2; we use it under BSD) |
| Tracy | Profiler | BSD-3-Clause |
| Dear ImGui | Debug UI | MIT |
| spdlog | Logging | MIT |
| fmt | Formatting | MIT |
| doctest | Tests | MIT |
| nanobench | Benchmarks | MIT |
| CPM.cmake | Dependency fetching (build only) | MIT |
| PCG (pcg-cpp) | RNG, or our own implementation of the algorithm | Apache-2.0 or MIT (dual) |
| xoshiro | RNG algorithm (we write our own implementation) | Public domain (CC0) |
| libtcod | Test oracle only (FOV/A\* comparisons in tests, not shipped) | BSD-3-Clause |
| Lua + sol2 | *(deferred)* scripting | MIT + MIT |

Libraries that SDL3_image may bundle (libpng, zlib) are permissive (libpng license, Zlib). They
will be recorded when the build configuration is fixed.

## Original implementation policy

All game and engine code in this repository is **our own construction**.

- **Techniques and algorithms are ideas, and we use them freely.** Examples: shadowcasting,
  Dijkstra maps, cellular automata, timing wheels. We learn them from articles, papers and other
  games.
- **Implementations are written from our own understanding and design.** No code is copied, ported
  line-by-line, or lightly edited from RogueBasin, tutorials, blogs, Stack Overflow, other games or
  any other source. This holds even when the license would allow it, and even when our result ends
  up structurally similar.
- **Reference implementations may be *read* to understand a technique,** but are closed while
  writing ours. Where a test checks our output against a library (e.g. libtcod as a FOV oracle),
  the library is a test-only dependency and none of its code enters ours.
- **The only third-party code in the product is the declared dependencies** listed above, used
  through their public APIs under their licenses.
- **Documentation cites sources** for techniques (title and link). Quotes stay short and
  attributed.

## Third-party content we do **not** redistribute

- **RogueBasin page text.** Its copyright status is unclear, and
  <https://roguebasin.com/index.php/RogueBasin:Copyrights> asks that large blocks not be copied.
  We keep our own summaries with links and short quotes.
- **Community Dwarf Fortress tilesets and fonts.** Mixed or unspecified licenses. We ship only
  CC0-licensed, self-made or explicitly permitted assets.
