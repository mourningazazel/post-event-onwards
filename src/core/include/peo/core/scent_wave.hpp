#pragma once

#include "peo/core/executor.hpp"
#include "peo/core/field_backend.hpp"
#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"
#include "peo/core/wind.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace peo::core {

/// Scent lost per cell of route, and per update of age (D-024). Equal costs mean a
/// trail one update old reads like one a cell further away.
inline constexpr std::int32_t kWaveDistanceCost = 8;
inline constexpr std::int32_t kWaveAgeCost = 8;
/// How far a fresh deposit is read, in cells of route, when the player stands still.
inline constexpr std::int32_t kWaveReachCells = 60;
/// The pull works in tiles of this many cells (PEO-078): a wide, short tile keeps each
/// row segment long enough to vectorise and the active set tight round a front.
inline constexpr int kWaveTileWidth = 128;
inline constexpr int kWaveTileHeight = 8;

/// The row kernels a wave can run (PEO-085): one source, built plain everywhere and for
/// AVX2 on x86-64. A wave starts on the best this CPU has, checked once per process;
/// both give the same bits.
enum class WaveKernel : std::uint8_t {
    Plain, ///< auto-vectorised for the build's baseline (NEON on arm64, SSE2 on x86-64)
    Avx2,  ///< x86-64 with AVX2, which has the 32-bit integer max SSE2 lacks
};

/// The geodesic field's numbers (D-024). A deposit of `strength` reads
/// strength - distance_cost x route - age_cost x age, and is gone when that reaches
/// zero; `speed` is how many cells the front advances per update.
struct WaveParams {
    std::int32_t strength = kWaveReachCells * (kWaveDistanceCost + kWaveAgeCost);
    std::int32_t distance_cost = kWaveDistanceCost;
    std::int32_t age_cost = kWaveAgeCost;
    int speed = 1;
    /// Rounds after the `speed` ones in which scent runs only downwind, from outdoor
    /// cells (PEO-048): the front outruns a walker with the wind behind them.
    int gust = 0;
    /// Scent lost upward outdoors in wind: a step's cost rises by this x the wind's
    /// intensity, scaled by openness (PEO-048).
    std::int32_t wind_loss = 0;
};
static_assert(kWindFull == kWaveDistanceCost, "a full wind makes a straight downwind step cost the floor");

/// Stored value of a cell nothing has reached: far below any age line, and far enough
/// above the type's minimum that subtracting a distance cost cannot overflow.
inline constexpr std::int32_t kWaveUnreached = std::numeric_limits<std::int32_t>::min() / 2;

/// How many updates a wave with `params` can run before its int32 age line (age_cost x
/// updates) breaks (PEO-077). The line must stay at or below -kWaveUnreached, or an
/// unreached cell's sample() underflows, and at or below INT32_MAX - strength, or a fresh
/// deposit overflows. A wave lives one stage (World::load_stage builds a new one); if one
/// ever has to outlive that, subtract the age line from its stored values first
/// (ADR-0011's global clock), an O(cells) pass.
[[nodiscard]] constexpr std::uint32_t max_wave_updates(const WaveParams& params) noexcept {
    if (params.age_cost <= 0) {
        return std::numeric_limits<std::uint32_t>::max(); // the line never moves
    }
    const std::int64_t below_unreached = -std::int64_t{kWaveUnreached};
    const std::int64_t below_overflow =
        std::int64_t{std::numeric_limits<std::int32_t>::max()} - params.strength;
    const std::int64_t line = below_unreached < below_overflow ? below_unreached : below_overflow;
    return static_cast<std::uint32_t>(line / params.age_cost);
}

/// The default numbers' bound: 2^27 updates, about 12 game years at one update per 3 s
/// (ADR-0016).
inline constexpr std::uint32_t kMaxWaveUpdates = max_wave_updates(WaveParams{});
/// The least game time a wave must be able to live: one year, ample for a stage.
inline constexpr std::uint64_t kMsPerDay = 24ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kDaysPerYear = 365;
inline constexpr std::uint64_t kMinWaveLifeMs = kDaysPerYear * kMsPerDay;
static_assert(to_ms(std::uint64_t{kMaxWaveUpdates} * kUpdatePeriodSubsteps) >= kMinWaveLifeMs,
              "kMaxWaveUpdates: with these WaveParams defaults the scent wave's int32 age line breaks "
              "within kMinWaveLifeMs; lower age_cost or strength, or rebase the line (PEO-077)");

/// The world's scent (D-024): an integer geodesic field. Each cell holds the best
/// strength - distance_cost x route - age_cost x age over the deposits that reached
/// it, propagated `speed` cells per update along walkable routes that never cut a
/// wall corner. Values are stored with the age folded in (age_cost x the update
/// they were made on), so nothing is rewritten to age them: sample() subtracts the
/// current age line.
///
/// A round is computed as a pull (PEO-078): every open cell takes the best offer of
/// its eight neighbours from the round-start buffer, if it beats its own value and
/// the age line, into the second buffer, and the buffers flip. That equals pushing
/// from the changed cells (values only rise, so an unchanged cell has made every
/// offer it can). Only tiles that changed last round, and their neighbours, are
/// pulled; a tile nobody pulls holds the same values in both buffers. The kernel is
/// branch-free and integer, so it vectorises and every build gives the same bits.
/// A wave is updated against one blocked grid for its life (a stage): its open and
/// corner masks are built from the first grid it sees. Deterministic, no allocation
/// once built.
class ScentWave {
public:
    ScentWave(int width, int height, WaveParams params = {});

    /// Leave `strength` at `at` now: it spreads from the next update on.
    void deposit(Vec2i at, std::int32_t strength) noexcept;

    /// One update: the age line moves on, then `speed` rounds carry every changed
    /// cell's value, less distance_cost, to its open neighbours (no corner cutting),
    /// wherever that improves them and stays above the age line. Under a wind
    /// (set_wind) each step's cost moves with the wind, scaled by the sending cell's
    /// openness, and `gust` more rounds follow that carry scent only downwind and only
    /// from outdoor cells. `openness` (default: all outdoors) is learned with `blocked`.
    void update(const Grid<bool>& blocked, const Grid<std::uint8_t>* openness = nullptr);

    /// Add a deposit that belonged before the update this wave has just run (the
    /// World speculates the update with no deposit and patches the player's in at
    /// commit, PEO-030). `before` is the wave before that update. Bit-exact against
    /// deposit() then update() for speed 1: a round is a max over offers made from
    /// round-start values, so the deposits' offers can be added after the others.
    /// Under a wind, gust rounds and all, the deposit is run alone through the update's
    /// rounds on a block round it and maxed in: exact for any speed (PEO-048).
    void patch_deposit(const ScentWave& before, Vec2i at, std::int32_t strength, const Grid<bool>& blocked,
                       const Grid<std::uint8_t>* openness = nullptr);

    /// The stage's wind (PEO-048), for this wave's life. Calm (the default) keeps the
    /// windless kernel, bit-identical to a wave that never had a wind.
    void set_wind(Wind wind) noexcept;

    /// Where the rounds' tiles and the direction-byte refresh run (D-035, PEO-080): any
    /// executor gives the same bits as none (serial), which is the default.
    void set_executor(Executor* executor) noexcept { executor_ = executor; }
    /// Where a calm update may run instead of the CPU pull (ADR-0014, PEO-081): any
    /// backend gives the same bits, calm or windy (PEO-087); null, the default, is the
    /// CPU pull.
    void set_field_backend(FieldBackend* backend) noexcept { backend_ = backend; }
    [[nodiscard]] const WindTable& wind() const noexcept { return wind_; }

    /// Make this wave equal to `source` (values, age line and what changes next round).
    /// Partners (the same nonzero token, the same size: a wave and a copy of it, as the
    /// World's and its Speculation's are after the first speculate) copy only the tiles
    /// either has written since this one was last synced; anything else copies the whole
    /// field (PEO-078). Returns the tiles copied.
    std::size_t sync_from(const ScentWave& source);
    /// Identity for sync_from: copies share it. The World gives each stage's wave a new one.
    void set_token(std::uint64_t token) noexcept { partner_token_ = token; }
    [[nodiscard]] std::uint64_t token() const noexcept { return partner_token_; }
    /// A token no other wave in this process has had (PEO-085): two Worlds never share
    /// one, so a Speculation moved between them cold-copies. Never 0.
    [[nodiscard]] static std::uint64_t new_token() noexcept;

    /// Whether this process can run `kernel`: Plain always, Avx2 on a CPU that has it.
    [[nodiscard]] static bool kernel_available(WaveKernel kernel) noexcept;
    /// Run this wave's rounds and direction bytes on `kernel` (for tests: the plain and
    /// AVX2 copies are compared). It must be available.
    void use_kernel(WaveKernel kernel) noexcept;

    /// Scent at `at` now: its stored value less the age line, never below 0.
    [[nodiscard]] std::int32_t sample(Vec2i at) const noexcept;

    /// The neighbouring cell (8-connected, on the map) whose sample() is the greatest
    /// and strictly greater than `from`'s; of equals, the first in kNeighbours8 order.
    /// Empty when no neighbour is stronger. With `blocked`, a blocked cell never counts,
    /// and a diagonal counts only when both orthogonal cells beside it are open (PEO-044).
    [[nodiscard]] std::optional<Vec2i>
    strongest_neighbour(Vec2i from, const Grid<bool>* blocked = nullptr) const noexcept;

    /// Where a calm Dead at `from` would step (PEO-079): always equal to
    /// strongest_neighbour(from, &blocked) for the blocked grid this wave was built
    /// from. Between updates the field is fixed, so the answer is a property of the
    /// cell, not of the unit: one direction byte per cell, written once per update
    /// over the cells whose neighbourhood changed, read here in O(1). The byte is
    /// stored without the age line, which never reorders neighbours; this checks
    /// only that the target still reads above it. Empty until has_flow().
    [[nodiscard]] std::optional<Vec2i> flow_target(Vec2i from) const noexcept;
    /// Whether the wave has its blocked grid (from the first update(), patch_deposit()
    /// or a sync from a wave that has it), so flow_target() answers.
    [[nodiscard]] bool has_flow() const noexcept { return masks_built_; }

    [[nodiscard]] const WaveParams& params() const noexcept { return params_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    /// Updates run: the age line is age_cost x this.
    [[nodiscard]] std::uint32_t updates() const noexcept { return updates_; }
    /// Stored values, row-major, age folded in. For tests and equivalence.
    [[nodiscard]] const std::vector<std::int32_t>& values() const noexcept { return buf_[cur_]; }
    /// What a field backend must reproduce beside the values (PEO-081), for tests: per
    /// tile, changed in the last round and written since the last partner sync; per
    /// cell, the direction byte.
    [[nodiscard]] const std::vector<std::uint8_t>& changed_tiles() const noexcept { return changed_; }
    [[nodiscard]] const std::vector<std::uint8_t>& written_tiles() const noexcept { return written_mark_; }
    [[nodiscard]] const std::vector<std::uint8_t>& flow_bytes() const noexcept { return flow_; }
    /// Cells the next round will pull: those of the tiles that changed and their
    /// neighbours (PEO-078; before, the count of changed cells).
    [[nodiscard]] std::size_t active_cells() const noexcept;

private:
    [[nodiscard]] std::size_t index(Vec2i c) const noexcept {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(c.x);
    }
    [[nodiscard]] std::int32_t age_line() const noexcept {
        return params_.age_cost * static_cast<std::int32_t>(updates_);
    }
    [[nodiscard]] std::size_t tile_of(int x, int y) const noexcept {
        return static_cast<std::size_t>(y / kWaveTileHeight) * static_cast<std::size_t>(tiles_x_) +
               static_cast<std::size_t>(x / kWaveTileWidth);
    }
    /// A full round carries scent every way; a gust round only downwind (PEO-048).
    enum class Round : std::uint8_t { Full, Gust };
    void build_masks(const Grid<bool>& blocked, const Grid<std::uint8_t>* openness);
    /// The best offer into (x, y) this round under the wind: the scalar form of the
    /// windy kernel, for border cells and the patch block. `value(n)` reads a
    /// neighbour's round-start value.
    template <typename Value>
    [[nodiscard]] std::int32_t wind_offer(int x, int y, Round kind, Value value) const;
    [[nodiscard]] bool pull_border_cell_wind(int x, int y, Round kind);
    void patch_with_wind(Vec2i at, std::int32_t v);
    /// A cell's value changed in the current buffer: its tile is pulled next round, and
    /// a partner learns of it.
    void touched(std::size_t tile) noexcept;
    /// Rewrite the direction bytes of the cells in [x0, x1) x [y0, y1), clamped to the map.
    void refresh_flow(int x0, int y0, int x1, int y1) noexcept;
    void refresh_flow_cell(int x, int y) noexcept;
    /// A tile and the one-cell ring round it: the bytes a change inside the tile can move.
    [[nodiscard]] std::array<int, 4> flow_reach(std::size_t tile) const noexcept;
    void expand_active();
    void round(Round kind);
    /// Rewrite the direction bytes of every stale tile and the ring round it (all of
    /// them when `all`), split by tile so that no two pieces write one cell.
    void refresh_stale_flow(bool all);
    /// One update's rounds on the backend; false when it declines (the CPU pull runs).
    [[nodiscard]] bool run_on_backend();
    [[nodiscard]] bool pull_tile(std::size_t tile, Round kind);
    [[nodiscard]] bool pull_border_cell(int x, int y);
    [[nodiscard]] bool open_at(int x, int y) const noexcept;

    WaveParams params_;
    int width_ = 0;
    int height_ = 0;
    int tiles_x_ = 0;
    int tiles_y_ = 0;
    WaveKernel kernel_ = WaveKernel::Plain;
    /// The two buffers; buf_[cur_] is the field now.
    std::array<std::vector<std::int32_t>, 2> buf_;
    std::size_t cur_ = 0; // 0 or 1
    /// Per cell: 1 when open (built from the blocked grid on first use), and a 4-bit
    /// mask of the diagonals that may offer into it (both cells beside the step open).
    std::vector<std::uint8_t> open_;
    std::vector<std::uint8_t> diag_;
    /// Per cell: how far the wind reaches it, kOpennessIndoors to kOpennessOutdoors.
    std::vector<std::uint8_t> openness_;
    bool masks_built_ = false;
    /// The wind's table, and whether it does anything (else the windless kernel runs).
    WindTable wind_{};
    bool windy_ = false;
    /// Per tile, during an update with gust rounds: changed in the last full round or
    /// any gust round since. Those cells have offered only downwind, so their tiles
    /// stay active into the next update's first round.
    std::vector<std::uint8_t> carry_;
    /// The patch block's two buffers: (2 x (speed + gust) + 1)^2 cells, sized once.
    std::array<std::vector<std::int32_t>, 2> patch_buf_;
    /// Per cell: the kNeighbours8 index flow_target() steps to, or none (PEO-079). Exact
    /// for the current values whenever update() is not running: deposit() and
    /// patch_deposit() rewrite the bytes round what they raise, update() rewrites the
    /// tiles its rounds changed (stale_) after them, sync_from() copies.
    std::vector<std::uint8_t> flow_;
    std::vector<std::uint8_t> stale_mark_;
    std::vector<std::uint32_t> stale_;
    /// Per active tile, by position in active_: whether its pull changed it (written by
    /// the round's pieces, read after them in tile order). And the tiles whose bytes a
    /// refresh rewrites (stale or beside a stale one).
    std::vector<std::uint8_t> pulled_;
    std::vector<std::uint32_t> flow_tiles_;
    Executor* executor_ = nullptr;
    FieldBackend* backend_ = nullptr;
    /// Per tile, from the backend: changed in its last round, in any round.
    std::vector<std::uint8_t> backend_last_;
    std::vector<std::uint8_t> backend_any_;
    /// A backend update left the second buffer behind: the next CPU round copies the
    /// field into it first (a tile nobody pulls must hold the same values in both).
    bool other_behind_ = false;
    /// Per tile: changed last round (or by a deposit or patch since); and scratch for
    /// building the active list.
    std::vector<std::uint8_t> changed_;
    std::vector<std::uint8_t> mark_;
    std::vector<std::uint32_t> active_;
    /// Tiles written since this wave last matched its partner, and their marks.
    std::vector<std::uint32_t> written_;
    std::vector<std::uint8_t> written_mark_;
    /// Shared by a wave and its copies; 0 means none, so sync_from always copies all.
    std::uint64_t partner_token_ = 0;
    std::uint32_t updates_ = 0;
};

} // namespace peo::core
