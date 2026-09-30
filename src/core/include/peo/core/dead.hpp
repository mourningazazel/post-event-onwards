#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/scent.hpp"
#include "peo/core/types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace peo::core {

/// One of the Dead: a walking corpse that cannot see but never stops smelling.
/// Deliberately tiny (struct-of-arrays comes later if the profiler asks for
/// it); thousands of these should step per turn. A large group is a horde.
struct Dead {
    Vec2i pos{};
    /// Game seconds until this unit may move again (D-015).
    std::uint16_t cooldown_s = 0;
    /// Game seconds a step takes this unit: slow vs fast Dead. At most one step per
    /// update in PEO-040, so anything under the update period acts as one period.
    std::uint16_t step_seconds = kUpdatePeriodSeconds;
};

/// Where a calm one of the Dead steps next (D-031): its strongest neighbour, as
/// strongest_neighbour() picks it, but only if no other Dead stands there and no
/// pending move has reserved it. A calm Dead never sidesteps to a weaker tile and
/// never climbs over another: when its best tile is taken it stays put.
[[nodiscard]] std::optional<Vec2i> decide_move(const Dead& unit, const ScentField& scent,
                                               const Grid<bool>& blocked, const Grid<std::uint8_t>& occupied,
                                               const Grid<bool>& reserved) noexcept;

/// One update of `period` seconds for one of the Dead: if cooling down, spend up to
/// `period` of it and stay put; else move one cell up the scent gradient and start
/// a step_seconds cooldown. Reads only the scent within one cell of its position.
/// Returns whether it moved.
bool step_dead(Dead& unit, const ScentField& scent, const Grid<bool>& blocked, Seconds period) noexcept;

/// Moves every one of the Dead one cell up the scent gradient. Those that
/// smell nothing stronger stay put. Returns how many moved.
std::size_t step_horde(std::vector<Dead>& horde, const ScentField& scent, const Grid<bool>& blocked,
                       Seconds period) noexcept;

} // namespace peo::core
