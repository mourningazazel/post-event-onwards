#pragma once

#include "peo/core/action.hpp"
#include "peo/core/dead.hpp"
#include "peo/core/desire.hpp"
#include "peo/core/executor.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/save.hpp"
#include "peo/core/scent_wave.hpp"
#include "peo/core/stage.hpp"
#include "peo/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace peo::core {

/// Smallest stage side: a border wall on each side of one open cell. World clamps
/// smaller stage_width / stage_height up to it rather than failing.
inline constexpr int kMinStageSide = 3;

/// Tunables for a World. Defaults match the first playable loop. Preconditions,
/// enforced by clamping in World (core has no exceptions): each stage side is at
/// least kMinStageSide; at most one of the Dead per open cell other than the
/// player's entry is spawned, and never fewer than zero.
/// The default for WorldParams::parallel_decide_min (PEO-080). Measured on the M1 with 4
/// threads and a saturated field, a whole step gains 1.04x at 50,000 Dead (batches of
/// about 5,000 a second), 1.07x at 200,000 (about 20,000) and 1.2-1.5x past a million,
/// and is never slower; the split starts where it clearly pays.
inline constexpr std::size_t kParallelDecideMin = 16384;

struct WorldParams {
    int initial_dead = 40;
    /// The geodesic scent field (D-024): a walking step deposits scent.strength.
    WaveParams scent{};
    /// Stage size in cells. Every turn sweeps the whole scent field, so a turn
    /// costs O(width * height); tests use a small stage to stay in budget.
    int stage_width = StageSpec{}.width;
    int stage_height = StageSpec{}.height;
    /// Substeps between scent updates (D-015, ADR-0016). WaveParams are per update.
    /// World rounds it up to a multiple of kSlotSubsteps.
    Substeps update_period = kUpdatePeriodSubsteps;
    /// Substeps in one cycle of the Dead's slots (D-031). Rounded as update_period.
    Substeps dead_cycle = kDeadCycleSubsteps;
    /// The strongest wind a stage can draw (PEO-048): each stage blows its own way at an
    /// intensity in [0, wind_max], from its seed. 0, the default, is always calm, and
    /// then nothing differs from a world without wind. Clamped to kMaxWindStep.
    std::int32_t wind_max = 0;
    /// The smallest batch of the Dead deciding in one slot that is split across the
    /// executor (PEO-080): below it, waking workers costs more than the decisions.
    std::size_t parallel_decide_min = kParallelDecideMin;
    /// The calm Dead's weighted draw (D-038 B, PEO-009).
    DeadDrawParams draw{};
};

/// Substeps the player spent on one tile since the last update. The log of these is
/// applied as scent at the next update (D-015).
struct Occupancy {
    Vec2i tile{};
    Substeps substeps = 0;
};

/// A move one of the Dead decided: it lands one slot later (D-031).
struct DeadMove {
    std::uint32_t unit = 0;
    Vec2i to{};
};

/// The Dead between updates (D-031): where they stand, the tiles pending moves
/// have reserved, who is moving, this cycle's slots, and the moves decided this
/// slot and last. World owns one; speculate() runs a copy ahead.
struct HordeState {
    std::vector<Dead> horde;
    /// Dead per tile: at most 1 for calm Dead.
    Grid<std::uint8_t> occupied{1, 1};
    /// Tiles a decided move will land on next slot.
    Grid<bool> reserved{1, 1};
    /// Per unit: 1 while it has a move pending (it skips its slots until it lands).
    std::vector<std::uint8_t> moving;
    /// This cycle's slots bucketed: the units in slot s of the cycle are
    /// slot_units[slot_begin[s], slot_begin[s + 1]), in ascending index.
    std::vector<std::uint32_t> slot_begin;
    std::vector<std::uint32_t> slot_units;
    /// Scratch for the poll: each unit's plan this cycle.
    std::vector<SlotPlan> plans;
    /// Moves decided this slot, and those decided last slot, which land now.
    std::vector<DeadMove> deciding;
    std::vector<DeadMove> landing;
    /// Scratch for a split slot (PEO-080): each deciding unit's target, in order. Empty
    /// between uses, so copying the horde costs nothing for it.
    std::vector<std::optional<Vec2i>> intents;
};

/// The next update computed ahead (PEO-007, D-015, PEO-060). The scent: the wave's
/// update with no deposit, so commit() only patches the logged deposits in
/// (ScentWave::patch_deposit, PEO-030). The Dead: between updates they read only the
/// last update's scent, never the player, so their slots up to and including the
/// next boundary are fixed once an update commits; speculate() runs them ahead and
/// records each slot's decisions and any poll, and commit() replays them. Valid
/// for the update and stage it was made on, across any number of actions that
/// cross no update boundary.
/// Reusable: speculate(out) and commit() recycle its buffers, so a frontend that
/// keeps one Speculation allocates nothing per update. A Speculation serves one World:
/// after the first speculate on a stage its wave and the World's are partners (they
/// swap in finish_from), so a warm speculate copies only the tiles either changed
/// since they last matched, not the field (PEO-078).
struct Speculation {
    ScentWave scent{1, 1};
    /// The desire field for the update after the boundary (PEO-009), from `scent` and
    /// the occupancy the Dead's slots reach the boundary with.
    DesireField desire;
    /// Tiles the last speculate copied into `scent`: all of them when cold. Diagnostic.
    std::size_t synced_tiles = 0;
    Tick update = 0;
    std::uint32_t stage_index = 0;
    /// The World's epoch it was made in: a load or a restore starts a new one, so a
    /// speculation from before never commits after (PEO-091).
    std::uint64_t epoch = 0;
    /// The Dead's substeps (from, to] are recorded; only the even ones carry a slot.
    /// The slot at substep t (even) has its decisions in
    /// decided[decided_begin[i], decided_begin[i + 1]) with i = t / 2 - from / 2 - 1.
    Substeps from = 0;
    Substeps to = 0;
    std::vector<std::uint32_t> decided_begin;
    std::vector<DeadMove> decided;
    /// The poll at substep poll_at, if one falls in (from, to]; 0 when none.
    Substeps poll_at = 0;
    std::vector<std::uint32_t> poll_begin;
    std::vector<std::uint32_t> poll_units;
    /// Scratch: the World's HordeState, run ahead.
    HordeState ahead;
};

/// Owns the whole simulation: stage, scent, the Dead and the player. Time moves
/// only through step() (D-002: the world waits for the player). Game time is kept
/// in substeps (ADR-0016); each action takes some, and scent and the Dead update once per
/// update_period (D-015). No wall clock, no thread, no SDL: deterministic from the
/// seed and the actions.
class World {
public:
    explicit World(Seed seed, WorldParams params = {});

    /// Build stage `index` from the world seed; reset the turn count and the clock.
    void load_stage(std::uint32_t index);
    /// Play a hand-built stage (PEO-088): the same reset as load_stage (scent, wind from
    /// the seed, slots, desire field, clock) with `stage`, `player` and `horde` as given
    /// instead of generated. The stage has no exit, so it never advances. Each of the
    /// Dead must stand on its own open cell, not the player's; `stage.openness`, if not
    /// the stage's size, is taken as outdoors on every open cell.
    void load_layout(Stage stage, Vec2i player, std::vector<Dead> horde);

    /// Spend one action: apply it at once (the player is on the new tile for its
    /// whole duration), then run the clock forward substep by substep: on each even
    /// one the Dead in that slot decide and last slot's moves land, and at each
    /// update_period boundary the scent updates with the logged substeps (D-031).
    /// Odd substeps only pass (ADR-0016).
    void step(Action action);

    /// Compute the next update ahead: the scent sweep and the Dead's slots up to
    /// the boundary. Pure: safe to run on another thread while nothing mutates this
    /// World.
    [[nodiscard]] Speculation speculate() const;
    /// As speculate(), into `out`, reusing its buffers.
    void speculate(Speculation& out) const;

    /// Spend one action using a speculation: bit-identical to step(action). If the
    /// action crosses no update boundary, `spec` is left untouched and still valid.
    /// At the first boundary it is consumed (it then holds recycled buffers, ready
    /// for the next speculate(spec)); further boundaries run the plain way. A stale
    /// speculation (other update or stage) falls back to step().
    void commit(Speculation& spec, Action action);

    /// Add `strength` of scent at `at` now, outside the clock. A test hook for
    /// benchmark emitters (PEO-043); not a game mechanic. It changes the field a
    /// speculation was built from without making it stale, so speculate after it.
    void deposit(Vec2i at, std::int32_t strength) noexcept { scent_.deposit(at, strength); }

    /// Same stage, turn, clock, occupancy log, player, horde, scent bits and the
    /// Dead's pending moves, reservations and slots. For tests.
    [[nodiscard]] static bool equivalent(const World& a, const World& b) noexcept;

    /// This world as a save image (PEO-091, ADR-0002, ADR-0017), tagged with `build`.
    /// One section per system, each with its sync scope: WRLD (seed, params, stage,
    /// clock, the spawn stream), STAG (a hand-built stage only; a generated one is
    /// regenerated), SCNT (the field), HORD (the Dead, their pending moves and the
    /// desire snapshot), OCCL (the occupancy log), all World; CHAR (the player),
    /// Character. Deterministic: the same world gives the same bytes.
    [[nodiscard]] SaveImage save(std::string_view build) const;
    /// Become the world `image` holds, so that it continues bit-identical to the one
    /// saved. Every value is checked against the stage it builds; on anything but Ok
    /// this World is untouched and `issues` says what and where. Unknown sections are
    /// skipped with an issue. The executor and field backend are kept, never saved.
    [[nodiscard]] SaveStatus restore(const SaveImage& image, std::vector<SaveIssue>& issues);

    [[nodiscard]] const Stage& stage() const noexcept { return stage_; }
    /// What the calm Dead draw from until the next update (PEO-009).
    [[nodiscard]] const DesireField& desire() const noexcept { return desire_; }
    /// Where speculate(), the scent updates and big batches of the Dead's decisions run
    /// their pieces (D-035, PEO-080). Null, the default, is serial; every executor gives
    /// the same world. Not owned: it must outlive the World's use of it.
    void set_executor(Executor* executor) noexcept;
    /// Where speculate() may run the scent's calm update (ADR-0014, PEO-081): a GPU
    /// backend from the frontend; null, the default, is the CPU pull. Same world either
    /// way. Live updates stay on the CPU, so a turn never waits on the device.
    void set_field_backend(FieldBackend* backend) noexcept;
    /// This stage's wind (PEO-048), drawn from its seed.
    [[nodiscard]] const Wind& wind() const noexcept { return wind_; }
    [[nodiscard]] const ScentWave& scent() const noexcept { return scent_; }
    [[nodiscard]] const std::vector<Dead>& horde() const noexcept { return dead_.horde; }
    [[nodiscard]] Vec2i player() const noexcept { return player_; }
    /// Actions taken on this stage (the HUD's turn counter).
    [[nodiscard]] Tick turn() const noexcept { return turn_; }
    /// Substeps elapsed on this stage.
    [[nodiscard]] Substeps substeps() const noexcept { return substeps_; }
    /// Game milliseconds elapsed on this stage, for display.
    [[nodiscard]] std::uint64_t game_ms() const noexcept { return to_ms(substeps_); }
    /// Scent updates run on this stage.
    [[nodiscard]] Tick updates() const noexcept { return updates_; }
    /// Substeps per tile since the last update, in first-visit order.
    [[nodiscard]] const std::vector<Occupancy>& occupancy() const noexcept { return log_; }
    [[nodiscard]] std::uint32_t stage_index() const noexcept { return stage_index_; }

private:
    void apply_action(Action action) noexcept;
    /// Run the clock forward `duration` substeps, one at a time. On the even ones the
    /// Dead replay `spec`'s recorded slots where it has them and run live elsewhere;
    /// at the first update boundary the update finishes from `spec` if given; any
    /// other boundary runs run_update().
    void advance(Substeps duration, Speculation* spec);
    /// The Dead's part of one slot, live: the poll at a cycle start, the slot's
    /// decisions, then last slot's landings. With `record`, also append what was
    /// decided (speculate() running ahead).
    void dead_slot(HordeState& dead, Slot slot, Speculation* record) const;
    /// The Dead's part of the slot at substep t from `spec`'s record: its poll, its
    /// decisions, then the same landings as live.
    void replay_slot(Substeps t, const Speculation& spec);
    void log_substeps(Substeps s);
    /// Scent a tile logged for `held` substeps of the period deposits (D-015).
    [[nodiscard]] std::int32_t logged_strength(Substeps held) const noexcept;
    /// Slots in one cycle of the Dead's moves.
    [[nodiscard]] Slot cycle_slots() const noexcept { return params_.dead_cycle / kSlotSubsteps; }
    void deposit_log(ScentWave& field) const noexcept;
    void run_update();
    void finish_from(Speculation& spec);
    /// load_stage's and load_layout's shared parts: the field and wind for stage_, and
    /// the reset of the clock and the Dead once dead_.horde and its occupancy stand.
    void start_field(Seed stage_seed_value);
    void start_horde(Seed stage_seed_value);
    void speculate_scent(Speculation& out) const;
    void speculate_dead(Speculation& out) const;
    void finish_turn();
    /// The Dead as a save left them (PEO-091): positions and steps, the moves decided
    /// last slot (they land next), this cycle's slots polled again from the clock, and
    /// the desire snapshot. Call after the clock is restored.
    void restore_horde(std::vector<Dead> horde, std::vector<DeadMove> landing);

    Seed seed_;
    WorldParams params_;
    std::uint32_t stage_index_ = 0;
    Stage stage_;
    Wind wind_{};
    DesireField desire_;
    Executor* executor_ = nullptr;
    FieldBackend* field_backend_ = nullptr;
    ScentWave scent_{1, 1};
    HordeState dead_;
    /// Mixed into the slot hashes: the stage's own seed.
    std::uint64_t stage_salt_ = 0;
    Vec2i player_{};
    Rng rng_{1};
    Tick turn_ = 0;
    Substeps substeps_ = 0;
    Tick updates_ = 0;
    /// The stage came from load_layout, not generate_stage: a save carries it whole.
    bool hand_built_ = false;
    /// Bumped by every load and restore; never saved (Speculation::epoch).
    std::uint64_t epoch_ = 0;
    std::vector<Occupancy> log_;
};

} // namespace peo::core
