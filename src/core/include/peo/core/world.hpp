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
    /// Game seconds between scent updates (D-015). ScentParams and player_scent are
    /// per update, so the defaults need no retune.
    Seconds update_period = kUpdatePeriodSeconds;
    /// Game seconds in one cycle of the Dead's slots (D-031).
    Seconds dead_cycle = kDeadCycleSeconds;
};

/// Game seconds the player spent on one tile since the last update. The log of
/// these is applied as scent at the next update (D-015).
struct Occupancy {
    Vec2i tile{};
    Seconds seconds = 0;
};

/// The next scent update computed ahead (PEO-007, D-015): the whole-field sweep,
/// post-step_linear and unclamped, so commit() only patches the logged deposits in
/// and clamps. The Dead are not in it: between updates they read only the last
/// update's scent, never the player, so commit() runs their seconds live, exactly as
/// step() does (D-031). Valid for the update and stage it was made on, across any
/// number of actions that cross no update boundary.
/// Reusable: speculate(out) and commit() recycle its buffer, so a frontend that
/// keeps one Speculation allocates nothing per update.
struct Speculation {
    ScentField scent{1, 1};
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
    /// whole duration), then run the clock forward second by second: the Dead in
    /// that second's slot decide, last second's moves land, and at each
    /// update_period boundary the scent updates with the logged seconds (D-031).
    void step(Action action);

    /// Compute the next scent update's sweep. Pure: safe to run on another thread
    /// while nothing mutates this World.
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

    /// Same stage, turn, clock, occupancy log, player, horde, scent bits and the
    /// Dead's pending moves, reservations and slots. For tests.
    [[nodiscard]] static bool equivalent(const World& a, const World& b) noexcept;

    [[nodiscard]] const Stage& stage() const noexcept { return stage_; }
    [[nodiscard]] const ScentField& scent() const noexcept { return scent_; }
    [[nodiscard]] const std::vector<Dead>& horde() const noexcept { return horde_; }
    [[nodiscard]] Vec2i player() const noexcept { return player_; }
    /// Actions taken on this stage (the HUD's turn counter).
    [[nodiscard]] Tick turn() const noexcept { return turn_; }
    /// Game seconds elapsed on this stage.
    [[nodiscard]] Seconds seconds() const noexcept { return seconds_; }
    /// Scent updates run on this stage.
    [[nodiscard]] Tick updates() const noexcept { return updates_; }
    /// Seconds per tile since the last update, in first-visit order.
    [[nodiscard]] const std::vector<Occupancy>& occupancy() const noexcept { return log_; }
    [[nodiscard]] std::uint32_t stage_index() const noexcept { return stage_index_; }

private:
    /// A move one of the Dead decided: it lands one second later (D-031).
    struct Move {
        std::uint32_t unit = 0;
        Vec2i to{};
    };

    void apply_action(Action action) noexcept;
    /// Run the clock forward `duration` seconds, one second at a time. At the first
    /// update boundary, finish the update from `spec` if given; any other boundary
    /// runs run_update().
    void advance(Seconds duration, Speculation* spec);
    /// The Dead's part of one second: the poll at a cycle start, the slot's
    /// decisions, then last second's landings.
    void dead_second(Seconds t);
    /// Give every unit with a stronger neighbour its slots for cycle `cycle`.
    void poll(std::uint64_t cycle);
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
    /// Tiles a decided move will land on next second.
    Grid<bool> reserved_{1, 1};
    /// Per unit: 1 while it has a move pending (it skips its slots until it lands).
    std::vector<std::uint8_t> moving_;
    /// This cycle's slots bucketed by second: the units in second s are
    /// slot_units_[slot_begin_[s], slot_begin_[s + 1]), in ascending index.
    std::vector<std::uint32_t> slot_begin_;
    std::vector<std::uint32_t> slot_units_;
    /// Scratch for poll(): each unit's plan this cycle.
    std::vector<SlotPlan> plans_;
    /// Moves decided this second, and those decided last second, which land now.
    std::vector<Move> deciding_;
    std::vector<Move> landing_;
    /// Mixed into the slot hashes: the stage's own seed.
    std::uint64_t stage_salt_ = 0;
    Vec2i player_{};
    Rng rng_{1};
    Tick turn_ = 0;
    Seconds seconds_ = 0;
    Tick updates_ = 0;
    std::vector<Occupancy> log_;
};

} // namespace peo::core
