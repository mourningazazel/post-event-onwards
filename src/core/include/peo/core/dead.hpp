#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/scent.hpp"
#include "peo/core/types.hpp"

#include <vector>

namespace peo::core {

/// One of the Dead: a walking corpse that cannot see but never stops smelling.
/// Deliberately tiny (struct-of-arrays comes later if the profiler asks for
/// it); thousands of these should step per turn. A large group is a horde.
struct Dead {
    Vec2i pos{};
    /// Ticks until this unit may move again; models slow vs fast hordes.
    std::uint8_t cooldown = 0;
    std::uint8_t speed = 1;
};

/// Moves one of the Dead one cell up the scent gradient, or ticks its cooldown.
/// Reads only the scent within one cell of its position. Returns whether it moved.
bool step_dead(Dead& unit, const ScentField& scent, const Grid<bool>& blocked) noexcept;

/// Moves every one of the Dead one cell up the scent gradient. Those that
/// smell nothing stronger stay put. Returns how many moved.
std::size_t step_horde(std::vector<Dead>& horde, const ScentField& scent, const Grid<bool>& blocked) noexcept;

} // namespace peo::core
