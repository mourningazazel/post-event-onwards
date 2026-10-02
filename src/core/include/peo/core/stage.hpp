#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"
#include "peo/core/wind.hpp"

#include <cstdint>

namespace peo::core {

/// The world is an endless sequence of stages. Each stage is generated from
/// (world seed, stage index) only, so any stage can be rebuilt on demand and
/// two players with the same seed see the same world.
struct StageSpec {
    Seed world_seed = 0;
    std::uint32_t index = 0;
    int width = 80;
    int height = 45;
    /// Wall density in [0, 1]. Later stages get denser; see stage_spec().
    float wall_density = 0.10F;
};

/// Derive a per-stage seed. Pure function; stable across builds.
[[nodiscard]] Seed stage_seed(Seed world_seed, std::uint32_t stage_index) noexcept;

/// Default difficulty curve for a stage index. Pure function.
[[nodiscard]] StageSpec stage_spec(Seed world_seed, std::uint32_t stage_index) noexcept;

struct Stage {
    StageSpec spec;
    /// true = wall. Border cells are always walls.
    Grid<bool> blocked;
    /// How far the wind reaches each cell (D-020, PEO-048): kOpennessOutdoors on every
    /// open cell of a generated stage today, kOpennessIndoors on walls; buildings mark
    /// their insides indoors.
    Grid<std::uint8_t> openness;
    Vec2i entry{};
    Vec2i exit{};
};

/// A stage's exit when it has none (a hand-built layout, PEO-088): off the map, so the
/// player never reaches it.
inline constexpr Vec2i kNoExit{-1, -1};

/// Generate a stage. Deterministic for a given spec.
[[nodiscard]] Stage generate_stage(const StageSpec& spec);

} // namespace peo::core
