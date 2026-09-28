#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/scent.hpp"
#include "peo/core/types.hpp"

#include <vector>

namespace peo::core {

/// One horde member. Deliberately tiny (struct-of-arrays comes later if the
/// profiler asks for it); thousands of these should tick per frame.
struct Hordeling {
    Vec2i pos{};
    /// Ticks until this unit may move again; models slow vs fast hordes.
    std::uint8_t cooldown = 0;
    std::uint8_t speed = 1;
};

/// Moves every hordeling one step up the scent gradient. Units that smell
/// nothing stronger stay put. Returns how many units moved.
std::size_t step_horde(std::vector<Hordeling>& horde, const ScentField& scent,
                       const Grid<bool>& blocked) noexcept;

} // namespace peo::core
