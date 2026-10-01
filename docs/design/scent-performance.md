# Scent field: what the measurements say

Architect analysis, 2026-09-29. Measured on the session container at `-O2`
unless stated. The `headless` preset is Debug + ASan/UBSan, which costs
roughly 40-70x more than `-O2`; budget against that number, not this one.

## The cost is not the problem

`ScentField::step()` at 200x120 (24,000 cells) costs **210 us** at `-O2`,
about 8.8 ns per cell. PEO-007 budgets 16 ms for `speculate()`. There is
~75x headroom. Nothing here justifies optimising throughput.

## D-021 variants (PEO-043)

`python3 tools/verify.py --perf` prints one line per variant from `headless-release`: the World
after 20 waits, emitters (floor tiles drawn from the seed, `player_scent` each before every
sample), then `speculate` and `commit` of a 6 s wait, best of 10. Budget: commit under 1 ms,
speculated update under 100 ms on the M1 Air.

| variant | emitters / 1000 tiles | M1 Air speculate | M1 Air commit | container |
|---|---|---|---|---|
| 200x120, 5,000 Dead | 0 | 224 us | 32 us | not measured |
| 200x120, 5,000 Dead | 6 | 233 us | 37 us | not measured |
| 512x512, 20,000 Dead | 6 | 2.06 ms | 337 us | not measured |
| 512x512, 50,000 Dead | 6 | 2.77 ms | 371 us | not measured |
| 512x512, 50,000 Dead | 20 | 2.63 ms | 415 us | not measured |

M1 Air (Asahi Linux, GCC 16), on mains power, pinned to a performance core (`taskset -c 6`),
2026-09-30; the median of three runs. Every variant is inside the budget: the worst commit uses 42%
of 1 ms, the worst speculate 3% of 100 ms. Unpinned, the first variant can land on an efficiency
core and read ~536 us / 52 us; the rest match. On battery (same day, pinned, median of three)
every figure is within 1% of mains power.

## The latency is the problem

`step()` is one Jacobi sweep of the screened Poisson equation
`D grad^2 v - k v + s = 0`. Its stencil is one cell wide, so information
travels at most one cell per turn, and diffusively `r ~ sqrt(2 D t)`.

Front advance from a standing source (D=0.8, k=0.01, floor 1e-6, deposit 500):

| turns | 10 | 25 | 50 | 100 | 200 | 400 | 800 | 1600+ |
|---|---|---|---|---|---|---|---|---|
| front (cells) | 10 | 17 | 24 | 33 | 45 | 59 | 70 | 72 |

Rate per turn falls 1.0 -> 0.28 -> 0.11 -> 0.07 -> 0.027 -> 0. Converged
reach is **72 cells after ~3000 turns**.

Theory `lambda * ln(peak/floor)` predicts 91.6 cells with
`lambda = sqrt((D/4)/k) = 4.47`. Observed 72. The floor flush truncates the
tail: a cell that would land under the floor is zeroed, which starves its
outward neighbour, so the front stalls early. Treat the formula as an upper
bound, roughly 20% optimistic.

## A moving player outruns their own scent

The player moves 1 cell/turn. The front's fastest advance is also 1 cell/turn,
and it decays from there. Measured with a source walking east:

| offset from player | -40 | -20 | -10 | -2 | +2 and beyond |
|---|---|---|---|---|---|
| value | 32.5 | 55.4 | 83.8 | 141.7 | **0.0** |

The field ahead of a moving player is **identically zero at every distance,
permanently**. The maximum sits 1-2 cells behind. `strongest_neighbour` 20
cells ahead returns nothing.

So with this scheme the Dead can only follow a trail. They cannot intercept,
cut off, or converge from in front. That is a property of the solver, not of
the tuning, and no choice of diffusion, decay or floor changes it.

## Diffusion must stay at or below 0.5

The checkerboard mode's amplification factor is `1 - 2D`.

| D | 1-2D | behaviour |
|---|---|---|
| 0.5 | 0.000 | critically damped; trail strictly monotone |
| 0.8 | -0.600 | sign-alternating; 5 direction reversals measured in the trail behind a moving player |

A gradient climber sitting in that ripple can be pulled the wrong way.
Choosing 0.5 costs 15 cells of converged reach (57 vs 72 on 200x120) and
converges 169 turns sooner.

## The geodesic field (PEO-030, D-024)

The world's scent is now the integer wave of `scent_wave.hpp`. M1 Air, `-O3`, pinned, 200x120 with
5000 Dead, a 6 s step, 300 samples: `speculate` 28 us best, 266 us median, 277 us worst (main before
it: 96 / 229 / 294), of which the wave's copy and update is 86 us (the float sweep's was ~85); commit
0-1 us (was 25-30: no whole-grid clamp). 512x512 with 50,000 Dead: speculate median 1.5 ms (was 2.3),
commit 0-3 us (was 269-280). On the town fixture a standing source in the deepest office reaches all
969 cells within 60 route-cells and climbs from 100% (diffusion: 14.5% reached, 15.4% climbed).

## The direction bytes (PEO-079)

`ScentWave` keeps one byte per cell naming the neighbour `strongest_neighbour` would pick, rewritten
after each update's rounds over the tiles they changed (each with a one-cell ring), round a deposit
or a patched deposit, and copied by `sync_from`. The poll and `decide_move` read it through
`flow_target`. Builder's M1 Air, `-O3`, pinned, 80 warm-up updates then median (p95) of 40; the
Dead's share is the speculate with the Dead less the same speculate with none.

| case, 6 emitters per 1000 tiles | before (nine-cell scan) | after (bytes) |
|---|---|---|
| scent update alone, saturated 512x512 | 222 (224) us | 675 (677) us |
| speculate, 512x512, 50,000 Dead | 4,617 (4,652) us | 2,617 (2,646) us |
| speculate, 512x512, no Dead | 292 (297) us | 768 (771) us |
| the Dead's share, 512x512 | 4,325 us | 1,849 us (2.3x less) |
| speculate, 200x200, 5000 Dead | 460 (467) us | 308 (313) us |

Commit is unchanged at 4-5 us. The refresh kernel vectorises (NEON) only with unconditional loads
and integer masks; with a `bool & bool` select GCC left it scalar at about 2.4 ms.

## The dense pull (PEO-078)

`ScentWave::update` now pulls active 128x8 tiles between two buffers; it is bit-identical to the
push (a reference copy of it runs beside the field in the tests, compared on every update).
Builder's M1 Air, `-O3`, pinned to a performance core, median (p95) of 40 after warm-up.

| case | before (push) | after (pull) |
|---|---|---|
| scent update alone, saturated 512x512, 6 emitters per 1000 tiles | 4,756 (4,765) us | 227 (229) us |
| scent update alone, saturated 512x512, 20 emitters per 1000 tiles | 4,639 (4,647) us | 227 (229) us |
| speculate, 200x120, 5000 Dead, no emitters | 243 (274) us | 189 (203) us |
| speculate, 200x120, 5000 Dead, 6 emitters | 1,007 (1,030) us | 444 (449) us |
| speculate, 512x512, 50,000 Dead, 6 emitters | 11,082 (11,159) us | 4,608 (4,659) us |
| speculate, 512x512, 50,000 Dead, 20 emitters | 11,142 (11,223) us | 4,470 (4,496) us |
| speculate, 2048x2048, warm (no Dead) | 2,941 us cold copy | 251 us (6 tiles synced) |

Commit is unchanged at 1-5 us. What is left of the 512x512 speculate is the Dead's run-ahead
(PEO-060), not the scent. Before the kernel's loads were made unconditional GCC left it scalar
(865 us, 5.5x); with them it vectorises with NEON.

## The direct solve

*Superseded by D-024: the geodesic field above replaced diffusion and the IIR far layer.*


A separable two-pass IIR solves the same equation exactly instead of
iterating it, per row then per column:

    forward:  yf[i] = x[i] + a*yf[i-1]
    backward: yb[i] = x[i] + a*yb[i+1]
    out[i]    = g*(yf[i] + yb[i] - x[i])

with `a = exp(-1/lambda)` and `g = (1-a)/(1+a)`.

Verified numerically: the impulse response equals `g*a^|d|` to 3.5e-18;
linearity holds to 2.2e-16, so PEO-007's patch survives; cost is independent
of lambda; resetting the accumulator at walls leaks exactly 0.0.

| operation, 200x120 | ms |
|---|---|
| FTCS one step | 0.134 |
| IIR one iteration, full-map reach | 0.304 |
| BFS Dijkstra map, full rebuild | 0.246 |
| FTCS to radius 100 | 13.4 |

Walls under-reach rather than leak. One sweep pair realises only L-shaped
paths, so behind a detour the field read 0.0 where the true geodesic value
was 2.5e-4 — conservative, and the right failure direction for a game.
Iterating recovers it and stays linear: 0.0 -> 4.8e-2 -> 1.4e-1 -> 2.4e-1
over 1 to 4 iterations. Whether 3 iterations suffices on real dungeon
topology is **not yet verified**.

The 2D separable kernel is `exp(-|d|_1 / lambda)`: diamond isolines, a
Manhattan metric. Defensible on a 4-neighbour grid, but it is a visible
design choice.

This also splits the two knobs that currently fight each other. A decaying
source layer carries history and trail freshness; the IIR carries reach via
lambda. Both are linear, so they compose.

## Determinism hazard

The build sets no floating-point flags. `-ffp-contract=fast` is the gcc and
clang default at `-O2`, so `here * share` and `(here - out + in) * keep` may
fuse into FMAs in Release and not in Debug, and differ across architectures.
D-002 requires `speculate` and `commit` to be bit-identical.

## Prior art

Brogue does not diffuse at all: it stores `scentTurnNumber - distance` and
combines with `max`, folding age and distance into one integer. Read from its
source. That is max-plus, not linear, so it would not fit D-002's patching.

Several primary sources (RogueBasin, Game AI Pro, arXiv) were unreachable
from the container; claims taken from search summaries rather than primary
text are not relied on above. The maths and timings in this note were derived
and measured locally.
