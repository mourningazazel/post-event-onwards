#pragma once

#include "peo/core/types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace peo::core {

/// Dense row-major 2D array. Hot loops index by (x, y) through at(); nothing here
/// allocates after construction so it is safe to use per-tick.
///
/// Grid<bool> is stored as one byte per cell (never std::vector<bool>): it is
/// addressable, faster to scan, and at() can hand out real references.
template <typename T> class Grid {
public:
    using value_type = std::conditional_t<std::is_same_v<T, bool>, std::uint8_t, T>;

    Grid() = default;
    Grid(int width, int height, const T& fill = T{})
        : width_(width), height_(height),
          cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                 static_cast<value_type>(fill)) {
        assert(width >= 0 && height >= 0);
    }

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] std::size_t size() const noexcept { return cells_.size(); }

    [[nodiscard]] bool in_bounds(Vec2i p) const noexcept {
        return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_;
    }

    [[nodiscard]] value_type& at(Vec2i p) noexcept { return cells_[index(p)]; }
    [[nodiscard]] const value_type& at(Vec2i p) const noexcept { return cells_[index(p)]; }
    [[nodiscard]] value_type& at(int x, int y) noexcept { return at(Vec2i{x, y}); }
    [[nodiscard]] const value_type& at(int x, int y) const noexcept { return at(Vec2i{x, y}); }

    void fill(const T& value) { cells_.assign(cells_.size(), static_cast<value_type>(value)); }

    [[nodiscard]] value_type* data() noexcept { return cells_.data(); }
    [[nodiscard]] const value_type* data() const noexcept { return cells_.data(); }

    [[nodiscard]] auto begin() noexcept { return cells_.begin(); }
    [[nodiscard]] auto end() noexcept { return cells_.end(); }
    [[nodiscard]] auto begin() const noexcept { return cells_.begin(); }
    [[nodiscard]] auto end() const noexcept { return cells_.end(); }

private:
    [[nodiscard]] std::size_t index(Vec2i p) const noexcept {
        assert(in_bounds(p));
        return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(p.x);
    }

    int width_ = 0;
    int height_ = 0;
    std::vector<value_type> cells_;
};

} // namespace peo::core
