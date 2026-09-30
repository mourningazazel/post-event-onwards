#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/scent_wave.hpp"
#include "peo/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace peo::core {

/// Game seconds in one cycle of the Dead's moves (D-031). Nine against the 6 s
/// scent update keeps the moves out of step with the field they climb.
inline constexpr Seconds kDeadCycleSeconds = 9;

/// One of the Dead: a walking corpse that cannot see but never stops smelling.
/// Deliberately tiny (struct-of-arrays comes later if the profiler asks for
/// it); thousands of these should step per turn. A large group is a horde.
struct Dead {
    Vec2i pos{};
    /// Game seconds a step takes this unit: slow vs fast Dead. Speed is slots per
    /// cycle, dead_cycle / step_seconds (D-031).
    std::uint16_t step_seconds = kUpdatePeriodSeconds;
};

/// One unit's moves in one cycle (D-031): `count` slots, evenly spaced from `offset`.
struct SlotPlan {
    Seconds offset = 0;
    Seconds count = 0;
};

/// Slots for unit `index` in cycle `cycle`: floor(dead_cycle / step_seconds) of them,
/// one more with a chance equal to the fraction left over, at most dead_cycle. The
/// offset and the chance are hashes of (salt, cycle, index), never a shared stream,
/// so who moves when changes every cycle and does not depend on any other draw.
[[nodiscard]] SlotPlan plan_slots(std::uint64_t salt, std::uint64_t cycle, std::size_t index,
                                  Seconds step_seconds, Seconds dead_cycle) noexcept;

/// The second within the cycle of slot `j` of `plan`, in [0, dead_cycle).
[[nodiscard]] constexpr Seconds slot_second(SlotPlan plan, Seconds j, Seconds dead_cycle) noexcept {
    return (plan.offset + j * dead_cycle / plan.count) % dead_cycle;
}

/// Where a calm one of the Dead steps next (D-031): its strongest neighbour, as
/// strongest_neighbour() picks it, but only if no other Dead stands there and no
/// pending move has reserved it. A calm Dead never sidesteps to a weaker tile and
/// never climbs over another: when its best tile is taken it stays put.
[[nodiscard]] std::optional<Vec2i> decide_move(const Dead& unit, const ScentWave& scent,
                                               const Grid<bool>& blocked, const Grid<std::uint8_t>& occupied,
                                               const Grid<bool>& reserved) noexcept;

} // namespace peo::core
