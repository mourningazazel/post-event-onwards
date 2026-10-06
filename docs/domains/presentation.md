# Domain · presentation

The SDL3 frontend: rendering, glyphs and tilesets, input, menus and UI. The frontend stays thin;
logic in it is a bug (AGENTS.md).

| Read | For |
|---|---|
| [adr/0001](../adr/0001-platforms-and-gpu-backend.md), [adr/0003](../adr/0003-single-cell-size.md), [adr/0015](../adr/0015-mit-compatible-dependencies-and-original-assets.md) | Vulkan, one cell size, tilesets and original assets |
| [design/engine-api.md](../design/engine-api.md) "UI as data" | How menus will be described |
| `src/app/` | The frontend |

- **Decisions:** D-011 (3 moves a second), D-032 (taps and held keys). D-042 (font: SDL3's built-in one until the owner's).
- **Queue:** PEO-018 (crow text); PEO-117 (player-centred view, first); PEO-097 (DF-grid tileset loader).
