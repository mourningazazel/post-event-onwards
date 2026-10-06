#pragma once

#include "peo/core/desire.hpp"
#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace peo::core {

/// Substeps in one cycle of the Dead's moves (D-031, ADR-0016): nine slots against
/// the six of a scent update keeps the moves out of step with the field they climb.
inline constexpr Substeps kDeadCycleSubsteps = 18;

/// One of the Dead: a walking corpse that cannot see but never stops smelling.
/// Deliberately tiny (struct-of-arrays comes later if the profiler asks for
/// it); thousands of these should step per turn. A large group is a horde.
struct Dead {
    Vec2i pos{};
    /// Substeps a step takes this unit: slow vs fast Dead. Speed is slots per cycle,
    /// dead_cycle / step_substeps (D-031). World keeps it a multiple of kSlotSubsteps.
    std::uint16_t step_substeps = kUpdatePeriodSubsteps;
    /// The unit's attrition threshold (ADR-0009, PEO-094): it is gone once the
    /// attrition curve passes it (aftermath.hpp, revisit.hpp). It travels with the unit.
    std::uint16_t fate = 0;
};
static_assert(sizeof(Dead) == 12, "fate fills the padding: thousands of these step per turn");

/// What one of the Dead is doing. Calm (scent, company and stay, D-038) is the only state
/// today; alerted and following come with PEO-010. Readers that count states loop over
/// kDeadStateNames, so a new state shows up in them without change (PEO-088's siege runner).
enum class DeadState : std::uint8_t { Calm };
inline constexpr std::array<const char*, 1> kDeadStateNames{"calm"};
[[nodiscard]] constexpr DeadState state_of(const Dead& /*unit*/) noexcept {
    return DeadState::Calm;
}

/// One unit's moves in one cycle (D-031): `count` slots, evenly spaced from `offset`.
/// Everything here counts slots (one per kSlotSubsteps substeps, ADR-0016), which
/// keeps D-031's arithmetic, and so every draw, as it was in whole seconds.
struct SlotPlan {
    Slot offset = 0;
    Slot count = 0;
};

/// Slots for unit `index` in cycle `cycle`: floor(cycle_slots / step_slots) of them,
/// one more with a chance equal to the fraction left over, at most cycle_slots. The
/// offset and the chance are hashes of (salt, cycle, index), never a shared stream,
/// so who moves when changes every cycle and does not depend on any other draw.
[[nodiscard]] SlotPlan plan_slots(std::uint64_t salt, std::uint64_t cycle, std::size_t index, Slot step_slots,
                                  Slot cycle_slots) noexcept;

/// The slot within the cycle of move `j` of `plan`, in [0, cycle_slots).
[[nodiscard]] constexpr Slot slot_second(SlotPlan plan, Slot j, Slot cycle_slots) noexcept {
    return (plan.offset + j * cycle_slots / plan.count) % cycle_slots;
}

/// The draw's random word for unit `index` deciding at slot `slot` (PEO-009): a
/// counter-based hash with its own salt, never a shared stream, so a unit's draw does
/// not depend on any other unit or on the order they decide in.
[[nodiscard]] std::uint64_t draw_word(std::uint64_t salt, Slot slot, std::size_t index) noexcept;

/// The choice `word` picks from nine weights (kNeighbours8 order, then staying): each
/// in proportion to its weight. Staying when every weight is 0.
[[nodiscard]] std::size_t draw_choice(const std::array<std::uint32_t, kDrawChoices>& weights,
                                      std::uint64_t word) noexcept;

/// Where a calm one of the Dead steps next (D-038 B, PEO-009): one weighted draw over
/// its eight neighbours and staying, from the desire field at its tile. If it draws a
/// tile another Dead stands on or a pending move has reserved, it stays and does not
/// draw again (D-031): a calm Dead never sidesteps, never climbs over another.
[[nodiscard]] std::optional<Vec2i> decide_move(const Dead& unit, const DesireField& desire,
                                               const Grid<std::uint8_t>& occupied, const Grid<bool>& reserved,
                                               std::uint64_t word) noexcept;

} // namespace peo::core
