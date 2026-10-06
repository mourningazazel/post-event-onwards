#include "peo/core/dead.hpp"

#include "peo/core/rng.hpp"

#include <algorithm>

namespace peo::core {

SlotPlan plan_slots(std::uint64_t salt, std::uint64_t cycle, std::size_t index, Slot step_slots,
                    Slot cycle_slots) noexcept {
    // Two words per unit, so the offset and the chance are independent draws.
    const auto key = static_cast<std::uint64_t>(index) * 2;
    const Slot step = std::max<Slot>(step_slots, 1);
    Slot count = cycle_slots / step;
    const float fraction = static_cast<float>(cycle_slots % step) / static_cast<float>(step);
    if (hash_unit(salt, cycle, key + 1) < fraction) {
        ++count;
    }
    return {.offset = static_cast<Slot>(hash_u64(salt, cycle, key) % cycle_slots),
            .count = std::min(count, cycle_slots)};
}

std::uint64_t draw_word(std::uint64_t salt, Slot slot, std::size_t index) noexcept {
    return hash_u64(salt, slot, static_cast<std::uint64_t>(index));
}

std::size_t draw_choice(const std::array<std::uint32_t, kDrawChoices>& weights, std::uint64_t word) noexcept {
    std::uint64_t total = 0;
    for (const std::uint32_t w : weights) {
        total += w;
    }
    if (total == 0) {
        return kStayChoice;
    }
    std::uint64_t pick = word % total;
    for (std::size_t c = 0; c < kDrawChoices; ++c) {
        if (pick < weights[c]) {
            return c;
        }
        pick -= weights[c];
    }
    return kStayChoice; // unreachable: pick < total
}

std::optional<Vec2i> decide_move(const Dead& unit, const DesireField& desire,
                                 const Grid<std::uint8_t>& occupied, const Grid<bool>& reserved,
                                 std::uint64_t word) noexcept {
    const std::size_t choice = draw_choice(desire.weights(unit.pos), word);
    if (choice == kStayChoice) {
        return std::nullopt;
    }
    const Vec2i to = unit.pos + kNeighbours8[choice];
    if (occupied.at(to) != 0 || reserved.at(to)) {
        return std::nullopt;
    }
    return to;
}

} // namespace peo::core
