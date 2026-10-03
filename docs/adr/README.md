# Architecture Decision Records

Each ADR records **one** decision: its context, the decision itself, and its consequences.

- ADRs are never edited after acceptance. A later ADR supersedes an earlier one instead.
- Status values: `Proposed` → `Accepted` → (`Superseded by ADR-XXXX`).

| ADR | Title | Status |
|---|---|---|
| [0001](0001-platforms-and-gpu-backend.md) | Target platforms and GPU backend | Accepted |
| [0002](0002-save-format.md) | Save format and persistence strategy | Accepted (refined by 0017) |
| [0003](0003-single-cell-size.md) | Single grid cell size everywhere | Accepted |
| [0004](0004-open-source-and-dependency-licensing.md) | Open source and dependency licensing policy | Accepted (license list and assets superseded by 0015) |
| [0005](0005-turn-model.md) | Player-stepped turns with level-of-detail simulation | Accepted (time units refined by 0016) |
| [0006](0006-scent-driven-mobs.md) | Scent-driven blind mobs as the core enemy mechanic | Accepted (details proposed) |
| [0007](0007-endless-staged-world.md) | Endless world with staged, persistent generation | Accepted (structure proposed; river handling superseded by 0010; geography by 0018) |
| [0008](0008-project-license-mit.md) | Project license — MIT | Accepted (assets decided in 0015) |
| [0009](0009-baseline-plus-aftermath-generation.md) | World generation = baseline + event + time-driven aftermath (the 3 stages) | Accepted (fixed-stage part superseded by 0011) |
| [0010](0010-rivers-as-lazy-long-features.md) | Rivers are long features, generated lazily at finer detail | Accepted (parameters proposed; courses come from the geography map, 0018) |
| [0011](0011-global-event-clock-and-world-event.md) | One global days-since-event clock; per-world event parameters | Accepted (curves proposed; point 4 refined by 0019) |
| [0012](0012-speculative-turns.md) | Turn-based, with the next turn computed while waiting | Accepted (names and budgets proposed; point 5 superseded by 0014) |
| [0013](0013-tile-scale-and-shared-occupancy.md) | One-metre tiles, one storey per z-level, shared tile occupancy | Accepted |
| [0014](0014-threads-and-gpu-compute-in-core.md) | Core uses threads and GPU compute it does not own | Accepted (interface names proposed) |
| [0015](0015-mit-compatible-dependencies-and-original-assets.md) | MIT-compatible dependencies; every asset original | Accepted (owner writes all text: D-042 B) |
| [0016](0016-turn-substeps-and-three-second-walk.md) | Twelve substeps per walking turn; a walking turn is 3 game seconds | Accepted (long-action cost: D-039 A) |
| [0017](0017-save-evolution-fallbacks-and-sync-scopes.md) | Saves survive content changes, keep two fallbacks, and know what syncs | Accepted |
| [0018](0018-geography-first-water-and-fitted-towns.md) | Geography first; towns fit the land and water they sit on | Accepted (endless squares: D-041 A) |
| [0019](0019-revisits-re-age-player-made-things.md) | Revisited areas catch up to the clock, player-made things included | Accepted (no gate: D-040 A) |
