#include "peo/core/memory.hpp"

#include <algorithm>

namespace peo::core {

void MapMemory::reset(int width, int height) {
    if (cells_.width() == width && cells_.height() == height) {
        cells_.fill(false);
    } else {
        cells_ = Grid<bool>(width, height, false);
    }
    count_ = 0;
}

std::size_t MapMemory::remember(const SightField& sight) {
    const CellRect window = sight.window();
    const int x0 = std::max(window.min.x, 0);
    const int y0 = std::max(window.min.y, 0);
    const int x1 = std::min(window.max.x, cells_.width() - 1);
    const int y1 = std::min(window.max.y, cells_.height() - 1);
    std::size_t added = 0;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            auto& cell = cells_.at(x, y);
            if (cell == 0 && sight.seen({x, y})) {
                cell = 1;
                ++added;
            }
        }
    }
    count_ += added;
    return added;
}

void MapMemory::mark(Vec2i cell) noexcept {
    if (cells_.in_bounds(cell) && cells_.at(cell) == 0) {
        cells_.at(cell) = 1;
        ++count_;
    }
}

bool operator==(const MapMemory& a, const MapMemory& b) noexcept {
    return a.width() == b.width() && a.height() == b.height() && a.count_ == b.count_ &&
           std::equal(a.cells_.begin(), a.cells_.end(), b.cells_.begin());
}

} // namespace peo::core
