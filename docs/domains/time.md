# Domain · time, turns and execution

How time passes: actions and their durations, the clock, substeps, the heavy update's cadence,
speculating the next turn, and the executor that runs passes on threads or the GPU.

| Read | For |
|---|---|
| [adr/0016](../adr/0016-turn-substeps-and-three-second-walk.md) | 12 substeps per walk; a walk is 3 s |
| [adr/0012](../adr/0012-speculative-turns.md), [adr/0014](../adr/0014-threads-and-gpu-compute-in-core.md) | Speculate/commit; executor and GPU rules |
| [architecture.md](../architecture.md) | Where the world update sits |
| `src/core/include/peo/core/{types,action,world,turn_input,executor,field_backend}.hpp` | Contracts |

- **ADRs:** 0005, 0012, 0014, 0016.
- **Decisions:** D-002, D-011, D-014/D-015 (clock; amended by ADR-0016), D-021 (budget),
  D-031 (slots), D-032 (taps), D-035, D-036. Open: D-039.
- **Owner notes:** N014 (speculation), N020, N023.
- **Queue:** PEO-006 (replay), 080, 081, 082, 083, 087; deferred PEO-090.
