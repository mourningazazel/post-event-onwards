#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>

namespace peo::core {

using Tick = std::uint64_t;
using Seed = std::uint64_t;

/// Integer grid coordinate. (0,0) is the top-left cell; y grows downward.
struct Vec2i {
    int x = 0;
    int y = 0;

    constexpr Vec2i operator+(Vec2i o) const noexcept { return {x + o.x, y + o.y}; }
    constexpr Vec2i operator-(Vec2i o) const noexcept { return {x - o.x, y - o.y}; }
    constexpr auto operator<=>(const Vec2i&) const noexcept = default;
};

/// The eight king-move neighbours, clockwise from north.
inline constexpr Vec2i kNeighbours8[8] = {{0, -1}, {1, -1}, {1, 0},  {1, 1},
                                          {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};

/// The four orthogonal neighbours: N, E, S, W.
inline constexpr Vec2i kNeighbours4[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

} // namespace peo::core
