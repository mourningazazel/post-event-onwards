#include "peo/core/stage.hpp"

#include "peo/core/rng.hpp"

#include <algorithm>

namespace peo::core {

Seed stage_seed(Seed world_seed, std::uint32_t stage_index) noexcept {
    // Mix so that adjacent stage indices produce unrelated seeds.
    std::uint64_t z = world_seed ^ (static_cast<std::uint64_t>(stage_index) * 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

StageSpec stage_spec(Seed world_seed, std::uint32_t stage_index) noexcept {
    constexpr float kBaseDensity = 0.10F;
    constexpr float kDensityPerStage = 0.01F;
    constexpr float kMaxDensity = 0.35F;
    StageSpec spec;
    spec.world_seed = world_seed;
    spec.index = stage_index;
    spec.wall_density =
        std::min(kMaxDensity, kBaseDensity + kDensityPerStage * static_cast<float>(stage_index));
    return spec;
}

Stage generate_stage(const StageSpec& spec) {
    Stage stage;
    stage.spec = spec;
    stage.blocked = Grid<bool>(spec.width, spec.height, false);
    Rng rng(stage_seed(spec.world_seed, spec.index));

    for (int y = 0; y < spec.height; ++y) {
        for (int x = 0; x < spec.width; ++x) {
            const bool border = x == 0 || y == 0 || x == spec.width - 1 || y == spec.height - 1;
            stage.blocked.at(x, y) = border || rng.chance(spec.wall_density);
        }
    }

    // Entry on the left edge, exit on the right; both carved open.
    stage.entry = Vec2i{1, rng.range(1, spec.height - 2)};
    stage.exit = Vec2i{spec.width - 2, rng.range(1, spec.height - 2)};
    stage.blocked.at(stage.entry) = false;
    stage.blocked.at(stage.exit) = false;
    stage.openness = Grid<std::uint8_t>(spec.width, spec.height, kOpennessIndoors);
    for (int y = 0; y < spec.height; ++y) {
        for (int x = 0; x < spec.width; ++x) {
            stage.openness.at(x, y) = stage.blocked.at(x, y) ? kOpennessIndoors : kOpennessOutdoors;
        }
    }
    return stage;
}

} // namespace peo::core
