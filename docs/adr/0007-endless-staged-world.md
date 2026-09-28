# ADR-0007: Endless world with staged, persistent generation

- Status: Accepted (direction). The level structure in `docs/design/world-generation.md` is a
  proposal.
- Date: 2026-09-27

## Decision

- **The world is endless and generated regionally, on demand.**
- **Generation is hierarchical.** Levels run from macro, region and settlement (identities only)
  down through structures and tiles to item contents. Each level is a pure function of its seed,
  coordinates, parent data and generator version.
- **Detail rings around the player** are sized by memory and time budgets, not constants.
- **Neighbouring context is available without generating it.** Examples are neighbouring
  cities, and rivers or mountains crossing a region.
- **Everything the player has seen is persistent,** down to items.

## Consequences

- World coordinates are 64-bit integers.
- Cross-boundary features (rivers, roads, ranges) must be decided at a level whose cell spans the
  boundary. Rivers in an endless world are the main technical risk and get a dedicated spike.
  *(River handling superseded by [ADR-0010](0010-rivers-as-lazy-long-features.md): rivers are
  lazy long features, with no drainage simulation, and are no longer a risk.)*
- Generator versioning and a freeze-on-materialization policy (ADR-0002) are needed so updates
  don't corrupt visited areas.
