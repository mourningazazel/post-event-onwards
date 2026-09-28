# N006 — Rivers: keep it simple

- Date: 2026-09-27
- Source: owner, in session

> the river thing, you need to relax about it. Just have the river be an endless stream that
> connects one point to the other. Figure out logically where you want it coming from and link it
> to somewhere further out on the other side of the generated point, have new rivers be generated
> less likely near eachother or matching more realistic expectations, but you dont need to include
> rivers in the overarching geograhpic generation, but instead make them after the fact as the
> world is generated on a finer detail

## What this settles

- **No drainage or watershed simulation.** Rivers are removed from the top-level geography pass.
- **A river is a long stream between two far-apart points**, chosen logically: it runs from higher
  ground toward lower ground or a lake. It is created when finer-detail generation reaches an
  area, and extends beyond that area in both directions.
- **Spacing:** new rivers are less likely near existing ones.
- **The biggest world-generation risk** (rivers in an endless world) is retired.

## Acted on in

- [ADR-0010](../../adr/0010-rivers-as-lazy-long-features.md)
- [`world-generation.md`](../world-generation.md): "Rivers (N006)"
