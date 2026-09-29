#pragma once

#include "peo/core/types.hpp"

namespace peo::core {

/// What the player does with one turn. The world only advances when it is
/// given an Action (D-002: turn-based), so every input maps to exactly one.
enum class ActionKind : std::uint8_t {
    Step, ///< move one cell in `dir`; into a wall it becomes a wait
    Wait, ///< stay put; the world still takes its turn
};

/// A walking step and a wait each take six game seconds (D-014, D-015).
inline constexpr Seconds kStepSeconds = 6;
inline constexpr Seconds kWaitSeconds = 6;

struct Action {
    ActionKind kind = ActionKind::Wait;
    Vec2i dir{};
    /// Game time the action takes. The world runs an update at each cadence
    /// boundary the action crosses (D-015). World treats 0 as 1.
    Seconds seconds = kWaitSeconds;

    [[nodiscard]] static constexpr Action step(Vec2i d, Seconds s = kStepSeconds) noexcept {
        return {ActionKind::Step, d, s};
    }
    [[nodiscard]] static constexpr Action wait(Seconds s = kWaitSeconds) noexcept {
        return {ActionKind::Wait, {}, s};
    }
};

} // namespace peo::core
