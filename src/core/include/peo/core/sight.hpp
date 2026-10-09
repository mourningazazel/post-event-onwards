#pragma once

// PEO-119: the player's sight (D-049). Every cell within the optical range that walls do
// not hide, by symmetric shadowcasting: if an open cell sees another, it is seen back, so
// the Dead that can see (vision.md) can later read the player's visibility from the same
// pass. Integer only, like the rest of core, so it is the same on every compiler. It runs
// once per player move, never per update: its cost is the open area within range.

#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"

#include <cstdint>
#include <vector>

namespace peo::core {

/// The optical range in cells: 500 ft is 152.4 m, and one tile is one metre (D-049,
/// ADR-0013).
inline constexpr int kOpticalRangeCells = 152;

/// The longest range computed: the largest stage side (PEO-083). Keeps the slope maths in
/// int (max 2 * 2048 * 4097 = 1.7e7) and the window buffer bounded.
inline constexpr int kMaxSightRangeCells = 2048;

/// What sight is computed with. The range is the one hook binoculars (later) use to see
/// further; nothing else reads a range. A negative range is taken as 0, one above
/// kMaxSightRangeCells as that.
struct SightParams {
    int range_cells = kOpticalRangeCells;
};

/// An inclusive rectangle of cells.
struct CellRect {
    Vec2i min;
    Vec2i max;

    friend constexpr bool operator==(const CellRect&, const CellRect&) noexcept = default;
};

/// The cells the player can see from one origin, recomputed from scratch per move. Its
/// buffers are reused: a compute at the same or a shorter range allocates nothing.
class SightField {
public:
    /// Fill the field for `origin` (inside `opaque`, asserted). `opaque` is what blocks
    /// sight: callers pass stage.blocked today, and a separate sight layer when doors,
    /// windows or glass land. Cells off the grid count as opaque and are never seen.
    void compute(const Grid<bool>& opaque, Vec2i origin, SightParams params = {});

    /// True only for a cell the last compute found visible; false anywhere else, including
    /// off the grid and before the first compute.
    [[nodiscard]] bool seen(Vec2i cell) const noexcept;

    /// The last compute's origin and range.
    [[nodiscard]] Vec2i origin() const noexcept { return origin_; }
    [[nodiscard]] int range() const noexcept { return range_; }

    /// The cells the last compute could reach: origin +/- range clipped to the grid. A map
    /// memory (D-051) folds the seen cells into its own grid by walking only this window.
    [[nodiscard]] CellRect window() const noexcept { return window_; }

private:
    /// One row of a quadrant's scan still to do: its depth from the origin and the sector
    /// of slopes (column over depth) still visible there, each slope a fraction with a
    /// positive denominator so they compare by cross-multiplying.
    struct Row {
        int depth;
        int start_num;
        int start_den;
        int end_num;
        int end_den;
    };

    void mark(Vec2i cell) noexcept;
    void scan_quadrant(const Grid<bool>& opaque, int quadrant);

    Vec2i origin_{};
    int range_ = 0;
    /// The square of side 2 * range + 1 round the origin, unclipped; seen_ is laid out over it.
    int side_ = 0;
    CellRect window_{{0, 0}, {-1, -1}};
    std::vector<std::uint8_t> seen_;
    std::vector<Row> rows_; // the scan's explicit stack, reused
};

} // namespace peo::core
