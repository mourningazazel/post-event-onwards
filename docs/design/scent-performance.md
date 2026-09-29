# Scent field: what the measurements say

Architect analysis, 2026-09-29. Measured on the session container at `-O2`
unless stated. The `headless` preset is Debug + ASan/UBSan, which costs
roughly 40-70x more than `-O2`; budget against that number, not this one.

## The cost is not the problem

`ScentField::step()` at 200x120 (24,000 cells) costs **210 us** at `-O2`,
about 8.8 ns per cell. PEO-007 budgets 16 ms for `speculate()`. There is
~75x headroom. Nothing here justifies optimising throughput.

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

## The direct solve

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
