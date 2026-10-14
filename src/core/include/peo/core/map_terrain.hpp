#pragma once

// PEO-146: the one map's terrain by world cell, a placeholder until the real tile generator
// (world-generation.md L4, after PEO-114 and PEO-115). Each cell's wall is a pure function of
// (seed, world cell), so any rectangle can be generated alone and joins its neighbours exactly,
// as geography.hpp's terrain_at and generate_geography do (D-041).

#include "peo/core/chunk.hpp"
#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"

#include <cstdint>

namespace peo::core {

struct TerrainParams {
    /// A cell is a wall when its hash's high 32 bits are below this: a fraction of 2^32, so
    /// 429496730 is one tenth (today's generate_stage density). Integers only.
    std::uint32_t wall_chance = 429496730;
};

/// Salt for the terrain hash.
inline constexpr std::uint64_t kTerrainSalt = 0x7E22A1ULL;

/// Whether `cell` is a wall: the source of truth every fill matches.
[[nodiscard]] bool wall_at(Seed seed, const TerrainParams& params, WorldCell cell) noexcept;

/// Fill grid cells [x0, x1) x [y0, y1) (clipped to the grids), grid cell (x, y) being world
/// cell origin + (x, y): blocked is wall_at, openness kOpennessIndoors on walls and
/// kOpennessOutdoors on open cells. Both grids are one size. No border, entry or exit;
/// allocates nothing.
void fill_terrain(Seed seed, const TerrainParams& params, WorldCell origin, Grid<bool>& blocked,
                  Grid<std::uint8_t>& openness, int x0, int y0, int x1, int y1) noexcept;

/// The same over the whole grids.
void fill_terrain(Seed seed, const TerrainParams& params, WorldCell origin, Grid<bool>& blocked,
                  Grid<std::uint8_t>& openness) noexcept;

} // namespace peo::core
