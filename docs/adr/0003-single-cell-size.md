# ADR-0003: Single grid cell size everywhere

- Status: Accepted
- Date: 2026-09-27

## Context

Some games (e.g. Cogmind) mix square map cells with half-width text cells. Dwarf Fortress uses one
cell size for everything.

## Decision

All grid layers (map, entities, effects, UI, text) share **one cell size** and one grid
coordinate system. Tilesets and fonts are PNG atlases whose glyphs all have that cell size.

## Consequences

- Simpler layout, hit-testing and UI framework. Any tileset or font swap is a single atlas swap.
- Text is less dense than with half-width glyphs. The UI design has to account for that.
- The renderer keeps an internal per-layer cell-size field set to the global value, so reversing
  this decision would be localized. It is not planned.
