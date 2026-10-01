#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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

/// The geodesic field's numbers (D-024). A deposit of `strength` reads
/// strength - distance_cost x route - age_cost x age, and is gone when that reaches
/// zero; `speed` is how many cells the front advances per update.
struct WaveParams {
    std::int32_t strength = kWaveReachCells * (kWaveDistanceCost + kWaveAgeCost);
    std::int32_t distance_cost = kWaveDistanceCost;
    std::int32_t age_cost = kWaveAgeCost;
    int speed = 1;
};

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
    /// wherever that improves them and stays above the age line.
    void update(const Grid<bool>& blocked);

    /// Add a deposit that belonged before the update this wave has just run (the
    /// World speculates the update with no deposit and patches the player's in at
    /// commit, PEO-030). `before` is the wave before that update. Bit-exact against
    /// deposit() then update() for speed 1: a round is a max over offers made from
    /// round-start values, so the deposits' offers can be added after the others.
    void patch_deposit(const ScentWave& before, Vec2i at, std::int32_t strength, const Grid<bool>& blocked);

    /// Make this wave equal to `source` (values, age line and what changes next round).
    /// Partners (the same nonzero token, the same size: a wave and a copy of it, as the
    /// World's and its Speculation's are after the first speculate) copy only the tiles
    /// either has written since this one was last synced; anything else copies the whole
    /// field (PEO-078). Returns the tiles copied.
    std::size_t sync_from(const ScentWave& source);
    /// Identity for sync_from: copies share it. The World gives each stage's wave a new one.
    void set_token(std::uint64_t token) noexcept { partner_token_ = token; }

    /// Scent at `at` now: its stored value less the age line, never below 0.
    [[nodiscard]] std::int32_t sample(Vec2i at) const noexcept;

    /// The neighbouring cell (8-connected) with the strongest scent strictly
    /// stronger than `from`, as ScentField's. With `blocked`, a diagonal counts only
    /// when both orthogonal cells beside it are open (PEO-044).
    [[nodiscard]] std::optional<Vec2i>
    strongest_neighbour(Vec2i from, const Grid<bool>* blocked = nullptr) const noexcept;

    /// Where a calm Dead at `from` would step (PEO-079): always equal to
    /// strongest_neighbour(from, &blocked) for the blocked grid this wave was built
    /// from. Between updates the field is fixed, so the answer is a property of the
    /// cell, not of the unit: one direction byte per cell, written once per update
    /// over the cells whose neighbourhood changed, read here in O(1). The byte is
    /// stored without the age line, which never reorders neighbours; this checks
    /// only that the target still reads above it. Empty before the first update(),
    /// which is when the wave learns its blocked grid.
    [[nodiscard]] std::optional<Vec2i> flow_target(Vec2i from) const noexcept;

    [[nodiscard]] const WaveParams& params() const noexcept { return params_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    /// Updates run: the age line is age_cost x this.
    [[nodiscard]] std::uint32_t updates() const noexcept { return updates_; }
    /// Stored values, row-major, age folded in. For tests and equivalence.
    [[nodiscard]] const std::vector<std::int32_t>& values() const noexcept { return buf_[cur_]; }
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
    void build_masks(const Grid<bool>& blocked);
    /// A cell's value changed in the current buffer outside a round: its tile is
    /// pulled next round, and a partner learns of it.
    void touched(std::size_t tile) noexcept;
    void expand_active();
    void round();
    [[nodiscard]] bool pull_tile(std::size_t tile);
    [[nodiscard]] bool pull_border_cell(int x, int y);
    [[nodiscard]] bool open_at(int x, int y) const noexcept;

    WaveParams params_;
    int width_ = 0;
    int height_ = 0;
    int tiles_x_ = 0;
    int tiles_y_ = 0;
    /// The two buffers; buf_[cur_] is the field now.
    std::array<std::vector<std::int32_t>, 2> buf_;
    std::size_t cur_ = 0; // 0 or 1
    /// Per cell: 1 when open (built from the blocked grid on first use), and a 4-bit
    /// mask of the diagonals that may offer into it (both cells beside the step open).
    std::vector<std::uint8_t> open_;
    std::vector<std::uint8_t> diag_;
    bool masks_built_ = false;
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
