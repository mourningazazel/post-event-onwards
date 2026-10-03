# Domain · persistence and network

Saves, their versions and fallbacks, content migration, replays, and the future host-client
sync.

| Read | For |
|---|---|
| [adr/0002](../adr/0002-save-format.md), [adr/0017](../adr/0017-save-evolution-fallbacks-and-sync-scopes.md) | Format, fallbacks, migration, sync scopes |
| [design/world-generation.md](../design/world-generation.md) "Principle 3" | Freeze on materialization |

- **ADRs:** 0002, 0007 (freeze policy), 0017.
- **Owner notes:** N023 (net sync, SSH, two fallbacks, junk and generic stand-ins).
- **Rule:** nothing in core assumes one player.
- **Queue:** PEO-006 (replay); deferred PEO-091, 092.
