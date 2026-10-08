#pragma once

// PEO-117: which stage cells the window shows. The view is counted in whole map cells,
// centred on the player and never clamped to the stage (D-047 to D-049): the player
// always sits on the centre cell and cells beyond the stage draw dark. SDL-free, so
// tests/core checks it headlessly.

#include "peo/core/types.hpp"

#include <algorithm>
#include <optional>

namespace peo::app {

/// The view's size in map cells.
struct View {
    int cols;
    int rows;
};

/// A view fitted to a window: its size, and where its top-left map cell starts.
struct ViewLayout {
    View view;
    /// The letterbox bar left of the map, in pixels.
    int left_px;
    /// The letterbox bar above the map, in pixels, counted below the HUD.
    int top_px;
};

/// The whole map cells of `map_cell_px` that fit a `px_w` by `px_h` window below a HUD
/// of `hud_px`, at least one each way; `map_cell_px` must be above 0. The leftover
/// pixels split evenly into dark bars either side (the odd pixel goes right or below).
/// The map cell is its own parameter, separate from the HUD's, so a later zoom (D-050)
/// changes only `map_cell_px`.
[[nodiscard]] constexpr ViewLayout layout_view(int px_w, int px_h, int map_cell_px, int hud_px) noexcept {
    const int map_h = std::max(px_h - hud_px, 0);
    const int cols = std::max(px_w / map_cell_px, 1);
    const int rows = std::max(map_h / map_cell_px, 1);
    return {{cols, rows},
            std::max(px_w - cols * map_cell_px, 0) / 2,
            std::max(map_h - rows * map_cell_px, 0) / 2};
}

/// The stage cell drawn at the view's top-left: the player on the centre cell. With an
/// even width the player sits on column cols / 2, one right of true centre (and one
/// below it with an even height). Not clamped: near the stage's top-left it is negative.
[[nodiscard]] constexpr core::Vec2i view_origin(core::Vec2i player, View view) noexcept {
    return {player.x - view.cols / 2, player.y - view.rows / 2};
}

/// The view cell that shows stage cell `cell`, or nullopt when it is off view.
[[nodiscard]] constexpr std::optional<core::Vec2i> view_to_screen(core::Vec2i cell, core::Vec2i origin,
                                                                  View view) noexcept {
    const core::Vec2i v{cell.x - origin.x, cell.y - origin.y};
    if (v.x < 0 || v.y < 0 || v.x >= view.cols || v.y >= view.rows) {
        return std::nullopt;
    }
    return v;
}

} // namespace peo::app
