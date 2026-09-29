#include "peo/core/dead.hpp"

namespace peo::core {

bool step_dead(Dead& unit, const ScentField& scent, const Grid<bool>& blocked) noexcept {
    if (unit.cooldown > 0) {
        --unit.cooldown;
        return false;
    }
    if (const auto next = scent.strongest_neighbour(unit.pos, &blocked)) {
        unit.pos = *next;
        unit.cooldown = unit.speed;
        return true;
    }
    return false;
}

std::size_t step_horde(std::vector<Dead>& horde, const ScentField& scent,
                       const Grid<bool>& blocked) noexcept {
    std::size_t moved = 0;
    for (Dead& unit : horde) {
        moved += step_dead(unit, scent, blocked) ? 1U : 0U;
    }
    return moved;
}

} // namespace peo::core
