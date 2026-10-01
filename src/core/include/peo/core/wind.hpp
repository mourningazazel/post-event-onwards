#pragma once

#include "peo/core/types.hpp"

#include <array>
#include <cstdint>
#include <iterator>

namespace peo::core {

/// Degrees in a turn: a wind blows toward a whole number of them.
inline constexpr std::int32_t kDegreesPerTurn = 360;
/// cos_degrees() is scaled by this.
inline constexpr std::int32_t kTrigOne = 4096;
/// The wind the pathfinding review called full: a step straight downwind, outdoors, costs
/// the floor of 1 at the default distance cost (scent_wave.hpp checks they agree).
inline constexpr std::int32_t kWindFull = 8;
/// The largest step a wind table holds, either way. The scent kernel scales a step by
/// openness / 255 exactly for products up to 255 x 255; past this a step is far beyond
/// any cost it could change.
inline constexpr std::int32_t kMaxWindStep = 255;
/// A cell's openness to the wind (D-020): outdoors it carries the whole wind, indoors none,
/// anything between for a porch or a broken wall.
inline constexpr std::uint8_t kOpennessOutdoors = 255;
inline constexpr std::uint8_t kOpennessIndoors = 0;

/// A stage's regional wind (D-020, PEO-048): the way it blows, in whole degrees clockwise
/// from east on the map (0 toward +x, 90 toward +y), and how hard. Intensity 0 is calm.
struct Wind {
    std::int32_t toward_degrees = 0;
    std::int32_t intensity = 0;
    friend bool operator==(const Wind&, const Wind&) = default;
};

/// What a wind does to one step of scent, per kNeighbours8 direction of travel (PEO-048).
struct WindTable {
    /// Taken off the step's cost, scaled by the sending cell's openness / 255: positive
    /// downwind (cheaper), negative upwind (dearer). The upward loss is already in it.
    std::array<std::int32_t, std::size(kNeighbours8)> step{};
    /// Bit d set when direction d runs downwind: the directions the gust rounds carry.
    std::uint8_t downwind = 0;
    /// No step changes and nothing runs downwind: the wave keeps its windless kernel.
    [[nodiscard]] bool calm() const noexcept;
};

/// cos(degrees) x kTrigOne, from a fixed table, so every build agrees (D-021).
[[nodiscard]] std::int32_t cos_degrees(std::int32_t degrees) noexcept;

/// The table for `wind`, in integers only (D-021). Per direction: intensity x the step's
/// progress along the wind (a diagonal under a wind along an axis makes a full cell of
/// progress, as the review's probe had it), rounded, less `loss` x intensity for scent
/// lost upward outdoors; clamped to kMaxWindStep. Downwind is progress above zero.
[[nodiscard]] WindTable wind_table(Wind wind, std::int32_t loss) noexcept;

} // namespace peo::core
