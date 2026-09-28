# Architecture Decision Records

Each ADR records **one** decision: its context, the decision itself, and its consequences.

- ADRs are never edited after acceptance. A later ADR supersedes an earlier one instead.
- Status values: `Proposed` → `Accepted` → (`Superseded by ADR-XXXX`).

| ADR | Title | Status |
|---|---|---|
| [0001](0001-platforms-and-gpu-backend.md) | Target platforms and GPU backend | Accepted |
| [0002](0002-save-format.md) | Save format and persistence strategy | Accepted |
| [0003](0003-single-cell-size.md) | Single grid cell size everywhere | Accepted |
| [0004](0004-open-source-and-dependency-licensing.md) | Open source and dependency licensing policy | Accepted |
| [0005](0005-turn-model.md) | Player-stepped turns with level-of-detail simulation | Accepted |
| [0006](0006-scent-driven-mobs.md) | Scent-driven blind mobs as the core enemy mechanic | Accepted (details proposed) |
| [0007](0007-endless-staged-world.md) | Endless world with staged, persistent generation | Accepted (structure proposed; river handling superseded by 0010) |
| [0008](0008-project-license-mit.md) | Project license — MIT | Accepted |
| [0009](0009-baseline-plus-aftermath-generation.md) | World generation = baseline + event + time-driven aftermath (the 3 stages) | Accepted (fixed-stage part superseded by 0011) |
| [0010](0010-rivers-as-lazy-long-features.md) | Rivers are long features, generated lazily at finer detail | Accepted (parameters proposed) |
| [0011](0011-global-event-clock-and-world-event.md) | One global days-since-event clock; per-world event parameters | Accepted (curves proposed) |
| [0012](0012-speculative-turns.md) | Turn-based, with the next turn computed while waiting | Accepted (names and budgets proposed) |
