#include "peo/core/horde.hpp"

namespace peo::core {

std::size_t step_horde(std::vector<Hordeling>& horde, const ScentField& scent,
                       const Grid<bool>& blocked) noexcept {
    std::size_t moved = 0;
    for (Hordeling& unit : horde) {
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
