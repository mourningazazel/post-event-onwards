# Decisions needed

The user's inbox. Both agents append here whenever a choice is about
**gameplay, engine abstraction, or project direction**, or when a useful
change would cause a **big rewrite**. Technical method (how to implement an
agreed thing) is never asked here; agents decide that themselves.

Answer by replying in chat with the id and a letter, e.g. `D-002: B`, plus any
notes. The agent that reads the answer moves the entry to `decisions.md` and
unblocks the related queue item. Newest at the top. Entry format is fixed:

```
## D-000 · short title                              (raised by architect|builder · YYYY-MM-DD · PEO-xxx)
Why now: one sentence.
Affects: gameplay | abstraction | direction · rewrite: none | small | large
- A) option — one line of consequence   ← recommended
- B) option — one line of consequence
```

---

## D-008 · How should the Dead find the player: keep diffusing, or solve the field?   (raised by architect · 2026-09-29 · PEO-026)
Why now: measurement shows a player walking 1 cell/turn permanently outruns their own scent — the field ahead of a moving player is exactly zero at every distance, so the Dead can only ever trail you, never intercept. No tuning changes this; the stencil is one cell wide. See docs/design/scent-performance.md.
Affects: gameplay | abstraction · rewrite: small to medium (ScentField internals; the public interface and D-002's linearity both survive)
- A) Solve the field each turn with a separable IIR — the same screened-Poisson equation the current loop half-solves, solved directly: full-map reach every turn at 0.30 ms, linear to 2e-16 so speculate/commit survives, zero leakage through walls. Splits history (a decaying source layer) from reach (lambda), so long reach and fresh trails stop fighting, and the Dead can intercept.   ← recommended
- B) Keep diffusing and accept trailing-only Dead — retune per PEO-026 and stop there: 57 cells of reach for a standing player, the Dead always behind you. Cheapest, and "they can never head you off" is a legitimate horror aesthetic.
- C) Add a geodesic distance field (a Dijkstra map) alongside scent — exact routing around walls to your current cell, O(N) per turn, but min-plus rather than linear, so it cannot be patched and must live outside the speculate/commit path.
