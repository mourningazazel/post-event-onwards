# ADR-0014: Core uses threads and GPU compute it does not own

- Status: Accepted (owner decisions D-035, D-036; N020). Interface names are proposals.
- Date: 2026-10-01
- Supersedes: ADR-0012 point 5 ("core spawns no threads"). The rest of ADR-0012 stands.

## Context

`research/parallel-and-gpu.md` measured that the scent update is a maximum over offers, so it can be
computed as a dense pull that is bit-identical in any order, and that the Dead's calm decisions
reduce to a per-cell direction plus a per-tile minimum claim. Both split across threads with no
change to the result. The owner wants the largest world the hardware allows: 1024x1024 to
2048x2048 stages, far-reaching scent, and fast time passing while the player sleeps or waits
(D-036). Integer fields (D-024) make GPU results reproducible bit for bit.

## Decision

1. **Core still creates no thread and no GPU device.** It stays free of SDL, I/O and globals.
2. **Executor.** Core's heavy passes take an executor from their caller: a parallel-for over N
   pieces. The frontend supplies a pooled one; tests and tools supply a serial one by default.
3. **Field backend.** The scent field (and later fields) runs behind a backend interface. Core
   holds the CPU reference. A GPU backend lives in its own library beside core (SDL_GPU on
   Vulkan, ADR-0001), selected by the frontend, and `--no-gpu-compute` always works.
4. **Determinism rules for any parallel or GPU pass:**
   - integers only;
   - results combine by maximum, minimum or integer sum, never by arrival order;
   - per-unit outputs go to per-unit slots, read in index order;
   - random draws come from counter-based hashes of (salt, cycle, index), never a shared stream;
   - each pass reads one buffer and writes another.
5. **Equivalence is law, extended.** Serial, threaded and GPU runs of the same seed and actions
   give the same world bit for bit, checked by golden tests beside speculate/commit's. CI runs the
   GPU path on Mesa lavapipe (ADR-0001).
6. **Latency.** GPU work is dispatched while the player is idle (speculation) and read back
   before commit; commit stays on the CPU and under 1 ms (D-021). Long actions may run many
   updates on the GPU before one readback.

## Consequences

- PEO-078 (pull field) and PEO-079 (flow grid) land single-threaded first; PEO-080 adds the
  executor; PEO-081 the GPU backend; PEO-082 and PEO-083 use them for long actions and big stages.
- Every new heavy pass states its split (by tile, by unit) and its combine (max, min, sum).
- The perf skill's "core spawns no threads" line now reads: core uses the executor it is given.
