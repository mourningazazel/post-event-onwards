# ADR-0004: Open source and dependency licensing policy

- Status: Accepted (the choice of project license is still open; see `docs/LICENSING.md`)
- Date: 2026-09-27

## Decision

- The project will be **open source**. The repository stays private during early design.
- **Dependencies must use permissive, GPL-compatible licenses:**
  - MIT
  - BSD-2/3
  - Zlib
  - Boost
  - Apache-2.0
  - ISC
  - CC0 or public domain
- **No GPL/LGPL/AGPL dependencies.** This keeps every choice of project license open, and avoids
  LGPL relinking obligations with static builds on Windows.
- **Every dependency is recorded** in `docs/LICENSING.md` with its SPDX ID, the date it was
  verified, and how it was verified.
- A `THIRD_PARTY_NOTICES` file is generated for binary releases.
- **Assets** (fonts, tilesets, sounds) need an explicit license. Community Dwarf Fortress
  tilesets are **not** bundled without the author's permission.
- **All code is an original implementation.** Techniques may be learned from anywhere, but no
  third-party code is copied, ported or adapted. The only third-party code in the product is the
  declared dependencies. See "Original implementation policy" in `docs/LICENSING.md`.
- **Third-party text is not copied into the repo.** In particular, RogueBasin's copyright status
  is unclear and its copyright page asks that large blocks not be copied. We keep our own
  summaries with short attributed quotes.
