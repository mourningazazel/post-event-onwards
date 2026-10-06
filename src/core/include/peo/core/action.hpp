#pragma once

#include "peo/core/types.hpp"

namespace peo::core {

/// What the player does with one turn. The world only advances when it is
/// given an Action (D-002: turn-based), so every input maps to exactly one.
enum class ActionKind : std::uint8_t {
    Step, ///< move one cell in `dir`; into a wall it becomes a wait
    Wait, ///< stay put; the world still takes its turn
};

/// A walking step and a wait each take 12 substeps, 3 game seconds (D-015, ADR-0016).
inline constexpr Substeps kStepSubsteps = kSubstepsPerWalk;
inline constexpr Substeps kWaitSubsteps = kSubstepsPerWalk;
/// A running step takes half a walking step: two cells per update (D-015).
inline constexpr Substeps kRunStepSubsteps = kSubstepsPerWalk / 2;

struct Action {
    ActionKind kind = ActionKind::Wait;
    Vec2i dir{};
    /// Game time the action takes. The world runs an update at each cadence
    /// boundary the action crosses (D-015). World treats 0 as one slot
    /// (kSlotSubsteps). Odd durations are legal (the between layer, ADR-0016), but
    /// no game input makes one.
    Substeps substeps = kWaitSubsteps;

    [[nodiscard]] static constexpr Action step(Vec2i d, Substeps s = kStepSubsteps) noexcept {
        return {ActionKind::Step, d, s};
    }
    [[nodiscard]] static constexpr Action wait(Substeps s = kWaitSubsteps) noexcept {
        return {ActionKind::Wait, {}, s};
    }
};

} // namespace peo::core
