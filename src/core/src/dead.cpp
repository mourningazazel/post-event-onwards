#include "peo/core/dead.hpp"

namespace peo::core {

std::size_t step_horde(std::vector<Dead>& horde, const ScentField& scent,
                       const Grid<bool>& blocked) noexcept {
    std::size_t moved = 0;
    for (Dead& unit : horde) {
        if (unit.cooldown > 0) {
            --unit.cooldown;
            continue;
        }
        if (const auto next = scent.strongest_neighbour(unit.pos, &blocked)) {
            unit.pos = *next;
            unit.cooldown = unit.speed;
            ++moved;
        }
    }
    return moved;
}

} // namespace peo::core
