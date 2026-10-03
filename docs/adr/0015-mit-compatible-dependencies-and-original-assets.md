# ADR-0015: MIT-compatible dependencies; every asset original

- Status: Accepted (owner note N023)
- Date: 2026-10-03
- Supersedes: ADR-0004's "permissive, GPL-compatible" wording and its assets bullet; ADR-0008's
  "original art, fonts and text assets ... a future decision". The rest of both stands.

## Context

The project is MIT (ADR-0008). ADR-0004 framed the dependency rule as "GPL-compatible", which
was written before the project license was chosen. The test that matters is whether a
dependency can ship inside an MIT-licensed game without changing what users of our code may do.

## Decision

1. **Dependencies must be MIT-compatible:** usable in, and redistributable with, an MIT project
   without imposing further terms on our code. Allowed SPDX ids: `MIT`, `BSD-2-Clause`,
   `BSD-3-Clause`, `Zlib`, `BSL-1.0`, `Apache-2.0`, `ISC`, `CC0-1.0`, `Unlicense`, public
   domain. Anything else (GPL, LGPL, AGPL, MPL, EPL, CC-BY-SA, "non-commercial") is refused.
   `docs/LICENSING.md` holds the list and every dependency's verified id.
2. **Every asset is original.** No art, tileset, font, sound or text is taken from another
   source, whatever its license. The owner supplies all art, the default tileset, detailed
   descriptions and lore-bearing items; how agent-written text relates to that is D-042.
3. **Tilesets use the Dwarf Fortress grid layout:** one PNG atlas of 16 x 16 square glyphs in
   code page 437 order, so tilesets players already own drop in. No third-party tileset is
   bundled. The game ships an ASCII mode and the owner's default tileset (ADR-0003's one cell
   size still holds).
4. **Techniques stay free to learn; code stays ours** (ADR-0004, LICENSING.md "Original
   implementation policy"). Sources of ideas are cited (`docs/research/roguebasin-notes.md`).

## Consequences

- `tools/verify.py` should refuse a dependency whose recorded SPDX id is off the list (PEO-093).
- A future network layer cannot use libssh (LGPL); libssh2 (BSD-3-Clause) or the system's own
  `ssh` binary as a transport are the compatible routes (ADR-0017).
- The ASCII mode needs a font; it is an asset, so it is drawn for the project too (D-042 asks
  who draws it).
