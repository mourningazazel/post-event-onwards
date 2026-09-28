# ADR-0006: Scent-driven blind mobs as the core enemy mechanic

- Status: Accepted (mechanic). The implementation details in `docs/design/scent-mobs.md` are
  proposals.
- Date: 2026-09-27

## Decision

- **Mob-type enemies are blind.** Each mob chooses among its free neighbouring tiles with a
  weighted random draw. The weights come from the per-tile scent values, through a per-archetype
  table.
- **Each mob moves at most once per step.**
- **Blocked mobs don't move.** Mobs without a free neighbour are skipped.
- **Scent is a per-tile field with one or more channels,** owned by a general Fields system.

## Consequences

- **The largest enemy population needs no pathfinding.** Performance is dominated by the scent
  field (a GPU/CPU stencil) and by memory locality in the movement step. Measured: 100k mobs take
  about 3 ms per step on one M1 core.
- **The mob processing order is a pluggable policy.** The proposed default is scent-descending
  order with a wake-up cascade, which lets hordes flow.
- **Enemies that can see, if any exist, use the Perception/FOV and Navigation systems instead.**
