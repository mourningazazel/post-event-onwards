#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>

namespace peo::core {

using Tick = std::uint64_t;
using Seed = std::uint64_t;
/// Game time in substeps (ADR-0016): a walking step is 12 of them and reads as 3 game
/// seconds. Today's behaviour runs on the even substeps; the odd ones are the between
/// layer, used only where the owner designates.
using Substeps = std::uint32_t;
/// A duration authored in real time (content, sleep, rot): only ever the input of
/// to_substeps, so the clock itself never counts seconds.
using Seconds = std::uint32_t;
/// An index into the Dead's slots: one slot per kSlotSubsteps substeps (D-031, ADR-0016).
using Slot = std::uint32_t;

/// A substep is 250 game milliseconds.
inline constexpr std::uint32_t kSubstepMs = 250;
inline constexpr Substeps kSubstepsPerSecond = 1000 / kSubstepMs;
/// One of today's whole-second slots: the even substeps carry them.
inline constexpr Substeps kSlotSubsteps = 2;
inline constexpr Substeps kSubstepsPerWalk = 12;
inline constexpr std::uint32_t kSecondsPerHour = 60 * 60;
inline constexpr Substeps kSubstepsPerHour = kSecondsPerHour * kSubstepsPerSecond;

/// Default cadence of the heavy systems (scent, the Dead): one update per walking
/// step, whatever the player does (D-015, ADR-0016). WorldParams::update_period.
inline constexpr Substeps kUpdatePeriodSubsteps = kSubstepsPerWalk;

/// A duration authored in whole seconds, as substeps. Max: about 1.07e9 s (34 game
/// years) before the uint32 wraps; content durations are hours and days.
[[nodiscard]] constexpr Substeps to_substeps(Seconds s) noexcept {
    return s * kSubstepsPerSecond;
}
/// A duration authored in milliseconds, as substeps, rounded up: a non-zero duration
/// never becomes 0. Max: about 1.07e12 ms (34 game years), as to_substeps.
[[nodiscard]] constexpr Substeps ms_to_substeps(std::uint64_t ms) noexcept {
    return static_cast<Substeps>((ms + kSubstepMs - 1) / kSubstepMs);
}
/// Substeps as game milliseconds, for display.
[[nodiscard]] constexpr std::uint64_t to_ms(std::uint64_t substeps) noexcept {
    return substeps * kSubstepMs;
}

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
