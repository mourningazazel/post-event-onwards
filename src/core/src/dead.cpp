#include "peo/core/dead.hpp"

#include "peo/core/rng.hpp"

#include <algorithm>

namespace peo::core {

SlotPlan plan_slots(std::uint64_t salt, std::uint64_t cycle, std::size_t index, Seconds step_seconds,
                    Seconds dead_cycle) noexcept {
    // Two words per unit, so the offset and the chance are independent draws.
    const auto key = static_cast<std::uint64_t>(index) * 2;
    const Seconds step = std::max<Seconds>(step_seconds, 1);
    Seconds count = dead_cycle / step;
    const float fraction = static_cast<float>(dead_cycle % step) / static_cast<float>(step);
    if (hash_unit(salt, cycle, key + 1) < fraction) {
        ++count;
    }
    return {.offset = static_cast<Seconds>(hash_u64(salt, cycle, key) % dead_cycle),
            .count = std::min(count, dead_cycle)};
}

std::optional<Vec2i> decide_move(const Dead& unit, const ScentWave& scent, const Grid<bool>& blocked,
                                 const Grid<std::uint8_t>& occupied, const Grid<bool>& reserved) noexcept {
    // A wave only deposited into has not seen its grid, so it has no direction bytes yet.
    const std::optional<Vec2i> best =
        scent.has_flow() ? scent.flow_target(unit.pos) : scent.strongest_neighbour(unit.pos, &blocked);
    if (!best || occupied.at(*best) != 0 || reserved.at(*best)) {
        return std::nullopt;
    }
    return best;
}

} // namespace peo::core
