#pragma once

// PEO-127: the look-ahead strip. Sight reaches further than a small screen shows, so
// ctrl plus a direction opens a strip along that edge of the view showing the cells just
// past it, and pushes it further out with each press (Ryan's design, notes 2026-10-10).
// Counted in view cells and stage cells, never pixels, so a later zoom (D-050) needs
// nothing here. SDL-free, so tests/core checks it headlessly.

#include "peo/core/sight.hpp"
#include "peo/core/types.hpp"

#include <algorithm>
#include <cassert>
#include <optional>

#include "camera.hpp"

namespace peo::app {

/// How deep the strip is, as a share of the view across its edge: Ryan's "maybe 15%".
inline constexpr int kPeekDepthPercent = 15;
/// The shallowest strip, so a tiny view still gets one row or column of look-ahead.
inline constexpr int kPeekMinDepth = 1;

/// The strip's depth in cells across its edge: 15% of the rows for north or south, of the
/// columns for east or west, rounded down, at least kPeekMinDepth and never past the view.
[[nodiscard]] constexpr int peek_depth(View view, core::Vec2i dir) noexcept {
    const int extent = dir.x == 0 ? view.rows : view.cols;
    return std::min(std::max(kPeekMinDepth, extent * kPeekDepthPercent / 100), extent);
}

/// How far each further press moves the strip out: half its depth, at least one cell.
[[nodiscard]] constexpr int peek_stride(int depth) noexcept {
    return std::max(1, depth / 2);
}

/// Where a strip is drawn and what it shows.
struct PeekStrip {
    /// The view cell of the strip's top-left.
    core::Vec2i at;
    /// The strip's size in view cells.
    View size;
    /// The stage cell drawn at the strip's top-left.
    core::Vec2i origin;
};

/// The strip's state: which edge and how many presses out. Held as a direction and a step
/// count, never as stage cells, so it follows the view as the player walks and keeps its
/// place across a window resize.
class Peek {
public:
    [[nodiscard]] bool open() const noexcept { return open_; }
    [[nodiscard]] core::Vec2i dir() const noexcept { return dir_; }
    [[nodiscard]] int steps() const noexcept { return steps_; }

    void close() noexcept {
        open_ = false;
        steps_ = 0;
    }

    /// One ctrl+direction press, `dir` a cardinal unit step. Closed, it opens on that edge;
    /// the same edge again pushes it out one stride, unless that strip would lie wholly
    /// outside `limit` (sight's window: nothing past it could be shown); the opposite edge
    /// closes it; a side edge moves it there, starting just past that edge. Returns
    /// whether anything changed, so the caller redraws only then.
    bool press(core::Vec2i dir, View view, core::Vec2i view_origin, core::CellRect limit) noexcept {
        assert((dir.x == 0) != (dir.y == 0) && dir.x >= -1 && dir.x <= 1 && dir.y >= -1 && dir.y <= 1);
        if (!open_) {
            open_ = true;
            dir_ = dir;
            steps_ = 0;
            return true;
        }
        if (dir == dir_) {
            const PeekStrip next = strip_at(dir_, steps_ + 1, view, view_origin);
            if (!overlaps(next, limit)) {
                return false;
            }
            ++steps_;
            return true;
        }
        if (dir.x == -dir_.x && dir.y == -dir_.y) {
            close();
            return true;
        }
        dir_ = dir;
        steps_ = 0;
        return true;
    }

    /// The strip to draw for a view whose top-left is stage cell `view_origin`, or nullopt
    /// when closed. At step 0 it shows the cells right past its edge; each step moves it a
    /// stride further out, always beside the view's own columns or rows.
    [[nodiscard]] std::optional<PeekStrip> strip(View view, core::Vec2i view_origin) const noexcept {
        if (!open_) {
            return std::nullopt;
        }
        return strip_at(dir_, steps_, view, view_origin);
    }

private:
    [[nodiscard]] static constexpr PeekStrip strip_at(core::Vec2i dir, int steps, View view,
                                                      core::Vec2i o) noexcept {
        const int d = peek_depth(view, dir);
        const int k = steps * peek_stride(d);
        if (dir.y < 0) {
            return {{0, 0}, {view.cols, d}, {o.x, o.y - d - k}};
        }
        if (dir.y > 0) {
            return {{0, view.rows - d}, {view.cols, d}, {o.x, o.y + view.rows + k}};
        }
        if (dir.x < 0) {
            return {{0, 0}, {d, view.rows}, {o.x - d - k, o.y}};
        }
        return {{view.cols - d, 0}, {d, view.rows}, {o.x + view.cols + k, o.y}};
    }

    /// Whether any stage cell the strip shows lies in `limit` (inclusive both ends, so a
    /// strip sharing only limit's edge row still overlaps).
    [[nodiscard]] static constexpr bool overlaps(const PeekStrip& s, core::CellRect limit) noexcept {
        return s.origin.x <= limit.max.x && s.origin.x + s.size.cols - 1 >= limit.min.x &&
               s.origin.y <= limit.max.y && s.origin.y + s.size.rows - 1 >= limit.min.y;
    }

    bool open_ = false;
    core::Vec2i dir_{};
    int steps_ = 0;
};

} // namespace peo::app
