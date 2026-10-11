#pragma once

// PEO-123: HUD lines wrapped to the window's width. A tiling window manager may ignore the
// window's minimum size and leave fewer than the 80 columns the HUD was written for; the
// lines then break at their spaces onto extra rows rather than clip, since a clipped HUD
// loses the very numbers it exists to show. Only where text breaks changes, never its
// words (D-042). SDL-free, so tests/core checks it headlessly.

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

namespace peo::app {

/// One HUD row: `length` characters of a line from `begin`.
struct HudSpan {
    int begin;
    int length;
};

/// Greedy word wrap of `line` into rows of at most `cols` characters (below 1 counts as 1).
/// A row takes as many whole space-separated words as fit; the spaces at a break are
/// dropped, so no row starts or ends with a space; runs of spaces inside a row are kept as
/// written. A word longer than `cols` is broken at `cols`. Returns the rows the whole line
/// needs, which may be more than out.size(); only the first out.size() spans are written.
/// An empty line needs one empty row. No allocation.
[[nodiscard]] constexpr int hud_wrap(std::string_view line, int cols, std::span<HudSpan> out) noexcept {
    constexpr auto npos = std::string_view::npos;
    const auto width = static_cast<std::size_t>(std::max(cols, 1));
    int rows = 0;
    const auto emit = [&](std::size_t begin, std::size_t end) {
        if (static_cast<std::size_t>(rows) < out.size()) {
            out[static_cast<std::size_t>(rows)] = {static_cast<int>(begin), static_cast<int>(end - begin)};
        }
        ++rows;
    };
    // Each row starts on its first non-space and ends after its last, so no row starts or
    // ends with a space.
    for (std::size_t begin = line.find_first_not_of(' '); begin != npos;) {
        std::size_t end = line.size();
        if (end - begin > width) {
            // The last space within reach ends the row; none means one long word: break it.
            const std::size_t space = line.rfind(' ', begin + width);
            end = space != npos && space > begin ? space : begin + width;
        }
        emit(begin, line.find_last_not_of(' ', end - 1) + 1); // line[begin] is not a space
        begin = line.find_first_not_of(' ', end);
    }
    if (rows == 0) {
        emit(0, 0);
    }
    return rows;
}

} // namespace peo::app
