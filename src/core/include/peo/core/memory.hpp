#pragma once

// PEO-124: what a character remembers seeing of the stage (D-051 A). Walls and floor once
// seen stay known out of sight; nothing on them does. One byte a stage cell, allocated when
// the stage loads, never per move: a fold walks only the sight window, not the stage.

#include "peo/core/grid.hpp"
#include "peo/core/sight.hpp"
#include "peo/core/types.hpp"

#include <cstddef>

namespace peo::core {

/// One character's map memory of one stage. It knows nothing of World, the stage or the
/// player, so a second character later is simply a second MapMemory.
class MapMemory {
public:
    /// 0 x 0: remembers nothing.
    MapMemory() = default;

    /// Forget every cell, at `width` x `height`. Reallocates only when the size changes;
    /// the same size is a fill, so a stage reload keeps the buffer.
    void reset(int width, int height);

    /// Remember every cell `sight` sees, walking only sight.window() clipped to this grid.
    /// Returns how many cells were newly remembered. Allocates nothing.
    std::size_t remember(const SightField& sight);

    /// False off the grid, including negative cells and a default-constructed memory.
    [[nodiscard]] bool remembered(Vec2i cell) const noexcept {
        return cells_.in_bounds(cell) && cells_.at(cell) != 0;
    }

    /// Remember one cell; ignored off the grid. For restore and tests.
    void mark(Vec2i cell) noexcept;

    [[nodiscard]] int width() const noexcept { return cells_.width(); }
    [[nodiscard]] int height() const noexcept { return cells_.height(); }
    /// Cells remembered: a running total, so O(1).
    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    /// The grid itself, one byte a cell, row-major: for the save and the no-allocation test.
    [[nodiscard]] const Grid<bool>& cells() const noexcept { return cells_; }

    /// Same size and the same cells.
    friend bool operator==(const MapMemory& a, const MapMemory& b) noexcept;

private:
    Grid<bool> cells_;
    std::size_t count_ = 0;
};

} // namespace peo::core
