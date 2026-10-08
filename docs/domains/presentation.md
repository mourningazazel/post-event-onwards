# Domain · presentation

The SDL3 frontend: rendering, glyphs and tilesets, input, menus and UI. The frontend stays thin;
logic in it is a bug (AGENTS.md).

| Read | For |
|---|---|
| [adr/0001](../adr/0001-platforms-and-gpu-backend.md), [adr/0003](../adr/0003-single-cell-size.md), [adr/0015](../adr/0015-mit-compatible-dependencies-and-original-assets.md) | Vulkan, one cell size, tilesets and original assets |
| [design/engine-api.md](../design/engine-api.md) "UI as data" | How menus will be described |
| `src/app/` | The frontend |

- **Decisions:** D-011 (3 moves a second), D-032 (taps and held keys). D-042 (font: SDL3's built-in one until the owner's). D-047 to D-050 (the view, sight); D-051 A (seen terrain stays drawn dimmed, a per-stage seen grid in core, saved); D-052 B (the Look cursor reaches any seen cell, the view scrolls with it); D-053 A (nothing named past 20 ft).
- **Done:** PEO-117 (player-centred view over a larger stage, `src/app/camera.hpp`).
- **Queue:** PEO-018 (crow text); PEO-097 (DF-grid tileset loader); PEO-123 (HUD under the minimum width); PEO-119 (sight, D-049); PEO-120 (Look cursor, D-052); PEO-121 (labels within 20 ft, D-053); PEO-124 (map memory, D-051, after PEO-119).
