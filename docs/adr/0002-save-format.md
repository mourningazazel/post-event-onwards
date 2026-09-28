# ADR-0002: Save format and persistence strategy

- Status: Accepted
- Date: 2026-09-27

## Context

The world is endless and persistent down to individual items. RogueBasin's *Save Files* guidance
recommends:

- save the seed and RNG state rather than generated output
- never serialize pointers
- use version-stable content IDs with remap tables
- validate on load

It also recommends delimited text; we deviate from that for performance and size.

## Decision

- **Binary, sectioned, versioned.** Each system owns its section, and each section carries its own
  version and a migration path.
- **Every section is zstd-compressed and carries a checksum.** Corruption is detected per section
  and does not silently truncate the rest of the file.
- **Stable IDs.** Entities and items use 64-bit stable IDs (the same IDs the ECS and World
  Database use). Content references use **stable string IDs**, with a remap table across game
  versions.
- **Regenerate or freeze.** Unvisited world is regenerated from seed plus versioned generators.
  Visited detail (generation levels L3–L5) is frozen into the World Database the first time it
  materializes, then loaded from there. See ADR-0007.
- **RNG state is saved per stream.**
- **Every loaded value is validated.** Bad data falls back to a default with a logged warning,
  never a crash. The player is warned before any lossy migration.

## Consequences

- Save size grows with explored area, not world size.
- Generator improvements never alter already-visited places.
- A small ID-remap table is maintained whenever content IDs are renamed or removed.
