#include "peo/core/sight.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace peo::core {

namespace {

/// The four quadrants a scan covers, each a cone round one axis out from the origin.
constexpr int kQuadrants = 4;
/// The row stack reserved per window column when the range grows.
constexpr std::size_t kRowsPerColumn = 2;

/// floor(a / b) and ceil(a / b) for b > 0: C++ division truncates toward zero, which is
/// wrong for a negative a.
constexpr int floor_div(int a, int b) noexcept {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}
constexpr int ceil_div(int a, int b) noexcept {
    return -floor_div(-a, b);
}

/// The cell `depth` out from the origin and `col` across, in quadrant `q`: north, east,
/// south, west (y down).
constexpr Vec2i to_cell(Vec2i origin, int q, int depth, int col) noexcept {
    switch (q) {
    case 0:
        return {origin.x + col, origin.y - depth};
    case 1:
        return {origin.x + depth, origin.y + col};
    case 2:
        return {origin.x + col, origin.y + depth};
    default:
        return {origin.x - depth, origin.y + col};
    }
}

} // namespace

void SightField::mark(Vec2i cell) noexcept {
    const int x = cell.x - (origin_.x - range_);
    const int y = cell.y - (origin_.y - range_);
    seen_[static_cast<std::size_t>(y) * static_cast<std::size_t>(side_) + static_cast<std::size_t>(x)] = 1;
}

bool SightField::seen(Vec2i cell) const noexcept {
    if (cell.x < window_.min.x || cell.y < window_.min.y || cell.x > window_.max.x ||
        cell.y > window_.max.y) {
        return false;
    }
    const int x = cell.x - (origin_.x - range_);
    const int y = cell.y - (origin_.y - range_);
    return seen_[static_cast<std::size_t>(y) * static_cast<std::size_t>(side_) +
                 static_cast<std::size_t>(x)] != 0;
}

void SightField::compute(const Grid<bool>& opaque, Vec2i origin, SightParams params) {
    assert(opaque.in_bounds(origin));
    // Clear only what the last compute could have set, under its own layout.
    for (int y = window_.min.y; y <= window_.max.y; ++y) {
        for (int x = window_.min.x; x <= window_.max.x; ++x) {
            const int wx = x - (origin_.x - range_);
            const int wy = y - (origin_.y - range_);
            seen_[static_cast<std::size_t>(wy) * static_cast<std::size_t>(side_) +
                  static_cast<std::size_t>(wx)] = 0;
        }
    }
    origin_ = origin;
    range_ = std::clamp(params.range_cells, 0, kMaxSightRangeCells);
    side_ = 2 * range_ + 1;
    const auto need = static_cast<std::size_t>(side_) * static_cast<std::size_t>(side_);
    if (seen_.size() < need) {
        seen_.resize(need, 0); // grows for a longer range only; never shrunk
        // Pending rows are the gaps along one scan path; a few per window column covers
        // any stage seen so far, so later moves at this range do not grow it.
        rows_.reserve(static_cast<std::size_t>(side_) * kRowsPerColumn);
    }
    window_ = {
        {std::max(origin.x - range_, 0), std::max(origin.y - range_, 0)},
        {std::min(origin.x + range_, opaque.width() - 1), std::min(origin.y + range_, opaque.height() - 1)}};
    mark(origin);
    if (range_ == 0) {
        return;
    }
    for (int q = 0; q < kQuadrants; ++q) {
        scan_quadrant(opaque, q);
    }
}

// Each row of a quadrant is scanned between the slopes still visible from the row before.
// A cell's centre on the sector makes an open cell seen (that is what keeps sight
// symmetric); any part of an opaque cell in it makes the wall seen, and the wall narrows
// the sector behind it. Slopes run column over depth, so -1 to 1 covers the quadrant.
void SightField::scan_quadrant(const Grid<bool>& opaque, int quadrant) {
    const int range_sq = range_ * range_;
    rows_.clear();
    rows_.push_back({1, -1, 1, 1, 1});
    while (!rows_.empty()) {
        Row row = rows_.back();
        rows_.pop_back();
        const int d = row.depth;
        // The cells whose extent meets the sector: the start slope rounded half up, the
        // end slope rounded half down, at this depth.
        const int min_col = floor_div(2 * d * row.start_num + row.start_den, 2 * row.start_den);
        const int max_col = ceil_div(2 * d * row.end_num - row.end_den, 2 * row.end_den);
        enum class Last : std::uint8_t { None, Wall, Open };
        Last last = Last::None;
        for (int col = min_col; col <= max_col; ++col) {
            const Vec2i cell = to_cell(origin_, quadrant, d, col);
            const bool on_grid = opaque.in_bounds(cell);
            const bool wall = !on_grid || opaque.at(cell) != 0;
            // The centre col / d lies in [start, end].
            const bool centred =
                col * row.start_den >= d * row.start_num && col * row.end_den <= d * row.end_num;
            if (on_grid && (wall || centred) && d * d + col * col <= range_sq) {
                mark(cell);
            }
            if (last == Last::Wall && !wall) {
                row.start_num = 2 * col - 1; // the open cell's near edge
                row.start_den = 2 * d;
            }
            if (last == Last::Open && wall && d < range_) {
                rows_.push_back({d + 1, row.start_num, row.start_den, 2 * col - 1, 2 * d});
            }
            last = wall ? Last::Wall : Last::Open;
        }
        if (last == Last::Open && d < range_) {
            rows_.push_back({d + 1, row.start_num, row.start_den, row.end_num, row.end_den});
        }
    }
}

} // namespace peo::core
