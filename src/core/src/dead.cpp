#include "peo/core/dead.hpp"

#include <algorithm>

namespace peo::core {

bool step_dead(Dead& unit, const ScentField& scent, const Grid<bool>& blocked, Seconds period) noexcept {
    if (unit.cooldown_s > 0) {
        unit.cooldown_s -= static_cast<std::uint16_t>(std::min<Seconds>(unit.cooldown_s, period));
        return false;
    }
    if (const auto next = scent.strongest_neighbour(unit.pos, &blocked)) {
        unit.pos = *next;
        unit.cooldown_s = unit.step_seconds;
        return true;
    }
    return false;
}

std::size_t step_horde(std::vector<Dead>& horde, const ScentField& scent, const Grid<bool>& blocked,
                       Seconds period) noexcept {
    std::size_t moved = 0;
    for (Dead& unit : horde) {
        moved += step_dead(unit, scent, blocked, period) ? 1U : 0U;
    }
    return moved;
}

} // namespace peo::core
