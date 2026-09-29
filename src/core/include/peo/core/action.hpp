#pragma once

#include "peo/core/types.hpp"

namespace peo::core {

/// What the player does with one turn. The world only advances when it is
/// given an Action (D-002: turn-based), so every input maps to exactly one.
enum class ActionKind : std::uint8_t {
    Step, ///< move one cell in `dir`; into a wall it becomes a wait
    Wait, ///< stay put; the world still takes its turn
};

struct Action {
    ActionKind kind = ActionKind::Wait;
    Vec2i dir{};

    [[nodiscard]] static constexpr Action step(Vec2i d) noexcept { return {ActionKind::Step, d}; }
    [[nodiscard]] static constexpr Action wait() noexcept { return {ActionKind::Wait, {}}; }
};

} // namespace peo::core
