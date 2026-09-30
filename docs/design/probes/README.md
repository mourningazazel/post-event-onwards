# Review probes (2026-09-29)

Scratch programs from the pathfinding and scent review, kept as reference source for
PEO-035 and PEO-030. They are not built, formatted or linted, and nothing links them.
Port what a brief names into `tests/`; do not include these files.

- `probe.cpp`: `make_town` (the cramped 200x120 town), `bfs_dist`, `climb_all`, and the
  diffusion variants the review compared.
- `probe2.cpp`: renormalised and wave fields near the source, sweep scaling, a trail walk.
- `probe3.cpp`: wind in the geodesic wave field; 512x512 with 50k Dead.

The probes allow diagonal moves past wall corners; the game no longer does (PEO-044), so
a port follows the brief's "no corner cutting", and its baseline numbers may shift slightly
from the review's.
