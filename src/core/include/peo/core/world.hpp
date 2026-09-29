#pragma once

#include "peo/core/action.hpp"
#include "peo/core/dead.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent.hpp"
#include "peo/core/stage.hpp"
#include "peo/core/types.hpp"

#include <cstdint>
#include <vector>

namespace peo::core {

/// Tunables for a World. Defaults match the first playable loop.
struct WorldParams {
    int initial_dead = 40;
    float player_scent = 1.0F;
    ScentParams scent{};
};

/// Owns the whole simulation: stage, scent, the Dead and the player. Time moves
/// only through step() (D-002: turn-based; the world waits for the player).
/// No clock, no thread, no SDL: deterministic from the seed and the actions.
class World {
public:
    explicit World(Seed seed, WorldParams params = {});

    /// Build stage `index` from the world seed and reset the turn counter.
    void load_stage(std::uint32_t index);

    /// Spend one turn: apply the action, then the world reacts (scent, the Dead).
    void step(Action action);

    [[nodiscard]] const Stage& stage() const noexcept { return stage_; }
    [[nodiscard]] const ScentField& scent() const noexcept { return scent_; }
    [[nodiscard]] const std::vector<Dead>& horde() const noexcept { return horde_; }
    [[nodiscard]] Vec2i player() const noexcept { return player_; }
    [[nodiscard]] Tick turn() const noexcept { return turn_; }
    [[nodiscard]] std::uint32_t stage_index() const noexcept { return stage_index_; }

private:
    Seed seed_;
    WorldParams params_;
    std::uint32_t stage_index_ = 0;
    Stage stage_;
    ScentField scent_{1, 1};
    std::vector<Dead> horde_;
    Vec2i player_{};
    Rng rng_{1};
    Tick turn_ = 0;
};

} // namespace peo::core
