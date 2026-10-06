# Licensing

Policy: [ADR-0004](adr/0004-open-source-and-dependency-licensing.md) as amended by
[ADR-0015](adr/0015-mit-compatible-dependencies-and-original-assets.md). In short: open source,
MIT-compatible dependencies only, every dependency verified and recorded here, every asset
original.

## Allowed dependency licenses (MIT-compatible)

`MIT`, `BSD-2-Clause`, `BSD-3-Clause`, `Zlib`, `BSL-1.0`, `Apache-2.0`, `ISC`, `CC0-1.0`,
`Unlicense`, public domain. Anything else is refused, including GPL, LGPL, AGPL, MPL, EPL,
CC-BY-SA and non-commercial terms. Known traps: libssh is LGPL (use libssh2, BSD-3-Clause, or
the system `ssh`, for ADR-0017's future transport).

## Project license: MIT

The code is MIT-licensed ([ADR-0008](adr/0008-project-license-mit.md), `LICENSE`). All candidate
dependencies below are compatible.

Assets: every asset is original (ADR-0015). The owner supplies the art, the default tileset,
detailed descriptions and lore-bearing items. Their license is still open for later (usually
separate from the code, e.g. CC-BY-4.0 or CC0).

Still open for later:

- **Outside contributions** would come in under MIT through a DCO sign-off, if contributions are
  accepted.

## Candidate dependencies

The license of each was verified on 2026-09-27 via the GitHub license API (the `LICENSE` file was
read where GitHub reported `NOASSERTION`). `tools/verify.py` enforces this table (PEO-093): every
row's license must be on the allowed list, and every library the build pulls in needs a row.

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
- **Community Dwarf Fortress tilesets and fonts.** None is bundled. Our tilesets use the same
  16 x 16 code page 437 grid layout, so players can load their own (ADR-0015). We ship only
  original assets.
