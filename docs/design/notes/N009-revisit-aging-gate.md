# N009 — Visited places stay as they were, unless you've been away about a month

- Date: 2026-09-27
- Source: owner, in session (answer to the ADR-0011 item 4 question)

> You can keep the world you were at the same, no need to alter an existing environment due to
> time based changes. I think this should only be re-generated if it is a place you have not been
> to for an extended period. Probably 1 month is a good gate for it.

## What this settles

- **Revisiting within the gate:** an area you have been to stays exactly as it was stored. There
  is no time-based catch-up.
- **Revisiting after about 1 month of game time away:** the area is brought forward to the current
  clock (aftermath re-applied from the stored clock to now, per ADR-0011).
- **Claude's assumption, to confirm:** things the *player* changed (barricades built, items
  moved, walls dug) are preserved through that update, and only untouched content ages.

## Acted on in

- [ADR-0011](../../adr/0011-global-event-clock-and-world-event.md), item 4
- [`purposes.md`](../purposes.md): P-WO-18
