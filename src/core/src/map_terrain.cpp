// PEO-146: the one map's placeholder terrain (peo/core/map_terrain.hpp).
#include "peo/core/map_terrain.hpp"

#include "peo/core/rng.hpp"
#include "peo/core/wind.hpp"

#include <algorithm>
#include <cassert>

namespace peo::core {

namespace {
constexpr int kHighBits = 32;
} // namespace

bool wall_at(Seed seed, const TerrainParams& params, WorldCell cell) noexcept {
    const std::uint64_t h =
        hash_u64(seed ^ kTerrainSalt, static_cast<std::uint64_t>(cell.x), static_cast<std::uint64_t>(cell.y));
    return static_cast<std::uint32_t>(h >> kHighBits) < params.wall_chance;
}

void fill_terrain(Seed seed, const TerrainParams& params, WorldCell origin, Grid<bool>& blocked,
                  Grid<std::uint8_t>& openness, int x0, int y0, int x1, int y1) noexcept {
    assert(blocked.width() == openness.width() && blocked.height() == openness.height());
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, blocked.width());
    y1 = std::min(y1, blocked.height());
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const bool wall = wall_at(seed, params, origin + Vec2i{x, y});
            blocked.at(x, y) = wall;
            openness.at(x, y) = wall ? kOpennessIndoors : kOpennessOutdoors;
        }
    }
}

void fill_terrain(Seed seed, const TerrainParams& params, WorldCell origin, Grid<bool>& blocked,
                  Grid<std::uint8_t>& openness) noexcept {
    fill_terrain(seed, params, origin, blocked, openness, 0, 0, blocked.width(), blocked.height());
}

} // namespace peo::core
