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
    /// Game seconds between updates of scent and the Dead (D-015). ScentParams and
    /// player_scent are per update, so the defaults need no retune.
    Seconds update_period = kUpdatePeriodSeconds;
};

/// Game seconds the player spent on one tile since the last update. The log of
/// these is applied as scent at the next update (D-015).
struct Occupancy {
    Vec2i tile{};
    Seconds seconds = 0;
};

/// The next update computed ahead, with no player deposit (PEO-007, D-015). The
/// scent is post-step_linear and unclamped so commit() can patch the logged
/// deposits in; `before` keeps the pre-update horde so the few Dead near a logged
/// tile can be re-decided. Valid for the update and stage it was made on, across
/// any number of actions that cross no update boundary.
/// Reusable: speculate(out) and commit() recycle its buffers, so a frontend that
/// keeps one Speculation allocates nothing per update.
struct Speculation {
    ScentField scent{1, 1};
    /// Scratch: `scent` after clamp_floor, which the Dead decide on.
    ScentField clamped{1, 1};
    std::vector<Dead> horde;
    std::vector<Dead> before;
    Tick update = 0;
    std::uint32_t stage_index = 0;
};

/// Owns the whole simulation: stage, scent, the Dead and the player. Time moves
/// only through step() (D-002: the world waits for the player). Game time is kept
/// in seconds; each action takes some, and scent and the Dead update once per
/// update_period (D-015). No wall clock, no thread, no SDL: deterministic from the
/// seed and the actions.
class World {
public:
    explicit World(Seed seed, WorldParams params = {});

    /// Build stage `index` from the world seed; reset the turn count and the clock.
    void load_stage(std::uint32_t index);

    /// Spend one action: apply it at once (the player is on the new tile for its
    /// whole duration), then run the clock forward by its seconds, logging them on
    /// the player's tile and running an update at each update_period boundary.
    void step(Action action);

    /// Compute the next update with no player deposit. Pure: safe to run on
    /// another thread while nothing mutates this World.
    [[nodiscard]] Speculation speculate() const;
    /// As speculate(), into `out`, reusing its buffers.
    void speculate(Speculation& out) const;

    /// Spend one action using a speculation: bit-identical to step(action). If the
    /// action crosses no update boundary, `spec` is left untouched and still valid.
    /// At the first boundary it is consumed (it then holds recycled buffers, ready
    /// for the next speculate(spec)); further boundaries run the plain way. A stale
    /// speculation (other update or stage) falls back to step().
    void commit(Speculation& spec, Action action);

    /// Add `amount` of scent at `at` now, outside the clock. A test hook for
    /// benchmark emitters (PEO-043); not a game mechanic. It changes the field a
    /// speculation was built from without making it stale, so speculate after it.
    void deposit(Vec2i at, float amount) noexcept { scent_.deposit(at, amount); }

    /// Same stage, turn, clock, occupancy log, player, horde and scent bits. For tests.
    [[nodiscard]] static bool equivalent(const World& a, const World& b) noexcept;

    [[nodiscard]] const Stage& stage() const noexcept { return stage_; }
    [[nodiscard]] const ScentField& scent() const noexcept { return scent_; }
    [[nodiscard]] const std::vector<Dead>& horde() const noexcept { return horde_; }
    [[nodiscard]] Vec2i player() const noexcept { return player_; }
    /// Actions taken on this stage (the HUD's turn counter).
    [[nodiscard]] Tick turn() const noexcept { return turn_; }
    /// Game seconds elapsed on this stage.
    [[nodiscard]] Seconds seconds() const noexcept { return seconds_; }
    /// Updates of scent and the Dead run on this stage.
    [[nodiscard]] Tick updates() const noexcept { return updates_; }
    /// Seconds per tile since the last update, in first-visit order.
    [[nodiscard]] const std::vector<Occupancy>& occupancy() const noexcept { return log_; }
    [[nodiscard]] std::uint32_t stage_index() const noexcept { return stage_index_; }

private:
    void apply_action(Action action) noexcept;
    /// Run the clock forward `duration` seconds. At the first boundary, finish the
    /// update from `spec` if given; any other boundary runs run_update().
    void advance(Seconds duration, Speculation* spec);
    void log_seconds(Seconds s);
    void deposit_log(ScentField& field) const noexcept;
    void run_update();
    void finish_from(Speculation& spec);
    void finish_turn();

    Seed seed_;
    WorldParams params_;
    std::uint32_t stage_index_ = 0;
    Stage stage_;
    ScentField scent_{1, 1};
    std::vector<Dead> horde_;
    /// Dead per tile (D-031): at most 1 for calm Dead.
    Grid<std::uint8_t> occupied_{1, 1};
    Vec2i player_{};
    Rng rng_{1};
    Tick turn_ = 0;
    Seconds seconds_ = 0;
    Tick updates_ = 0;
    std::vector<Occupancy> log_;
};

} // namespace peo::core
