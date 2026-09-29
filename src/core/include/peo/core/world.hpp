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

/// Smallest stage side: a border wall on each side of one open cell. World clamps
/// smaller stage_width / stage_height up to it rather than failing.
inline constexpr int kMinStageSide = 3;

/// Tunables for a World. Defaults match the first playable loop. Preconditions,
/// enforced by clamping in World (core has no exceptions): each stage side is at
/// least kMinStageSide; at most one of the Dead per open cell other than the
/// player's entry is spawned, and never fewer than zero.
struct WorldParams {
    int initial_dead = 40;
    float player_scent = kPlayerScent;
    ScentParams scent{};
    /// Stage size in cells. Every turn sweeps the whole scent field, so a turn
    /// costs O(width * height); tests use a small stage to stay in budget.
    int stage_width = StageSpec{}.width;
    int stage_height = StageSpec{}.height;
};

/// The next turn computed ahead, assuming the player waits and deposits nothing
/// (PEO-007). The scent is post-step_linear and unclamped so commit() can patch
/// the real deposit in; `before` keeps the pre-turn horde so the few Dead near the
/// player can be re-decided. Valid only for the turn and stage it was made on.
/// Reusable: speculate(out) and commit() recycle its buffers, so a frontend that
/// keeps one Speculation allocates nothing per turn.
struct Speculation {
    ScentField scent{1, 1};
    /// Scratch: `scent` after clamp_floor, which the Dead decide on.
    ScentField clamped{1, 1};
    std::vector<Dead> horde;
    std::vector<Dead> before;
    Tick turn = 0;
    std::uint32_t stage_index = 0;
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

    /// Compute the next turn assuming a Wait with no deposit. Pure: safe to run on
    /// another thread while nothing mutates this World.
    [[nodiscard]] Speculation speculate() const;
    /// As speculate(), into `out`, reusing its buffers.
    void speculate(Speculation& out) const;

    /// Spend one turn from a speculation: bit-identical to step(action). A stale
    /// speculation (other turn or stage) falls back to step(). Consumes `spec`:
    /// afterwards it holds recycled buffers, ready for the next speculate(spec).
    void commit(Speculation& spec, Action action);

    /// Same stage, turn, player, horde and scent bits. For tests.
    [[nodiscard]] static bool equivalent(const World& a, const World& b) noexcept;

    [[nodiscard]] const Stage& stage() const noexcept { return stage_; }
    [[nodiscard]] const ScentField& scent() const noexcept { return scent_; }
    [[nodiscard]] const std::vector<Dead>& horde() const noexcept { return horde_; }
    [[nodiscard]] Vec2i player() const noexcept { return player_; }
    [[nodiscard]] Tick turn() const noexcept { return turn_; }
    [[nodiscard]] std::uint32_t stage_index() const noexcept { return stage_index_; }

private:
    void apply_action(Action action) noexcept;
    void finish_turn();

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
