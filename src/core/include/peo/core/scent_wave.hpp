#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"

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
/// current age line. Work per update is the cells that changed, bounded by reach,
/// not the map. Deterministic, integer, no allocation once warm.
class ScentWave {
public:
    ScentWave(int width, int height, WaveParams params = {});

    /// Leave `strength` at `at` now: it spreads from the next update on.
    void deposit(Vec2i at, std::int32_t strength) noexcept;

    /// One update: the age line moves on, then `speed` rounds carry every changed
    /// cell's value, less distance_cost, to its open neighbours (no corner cutting),
    /// wherever that improves them and stays above the age line.
    void update(const Grid<bool>& blocked) noexcept;

    /// Add a deposit that belonged before the update this wave has just run (the
    /// World speculates the update with no deposit and patches the player's in at
    /// commit, PEO-030). `before` is the wave before that update. Bit-exact against
    /// deposit() then update() for speed 1: a round is a max over offers made from
    /// round-start values, so the deposits' offers can be added after the others.
    void patch_deposit(const ScentWave& before, Vec2i at, std::int32_t strength,
                       const Grid<bool>& blocked) noexcept;

    /// Scent at `at` now: its stored value less the age line, never below 0.
    [[nodiscard]] std::int32_t sample(Vec2i at) const noexcept;

    /// The neighbouring cell (8-connected) with the strongest scent strictly
    /// stronger than `from`, as ScentField's. With `blocked`, a diagonal counts only
    /// when both orthogonal cells beside it are open (PEO-044).
    [[nodiscard]] std::optional<Vec2i>
    strongest_neighbour(Vec2i from, const Grid<bool>* blocked = nullptr) const noexcept;

    [[nodiscard]] const WaveParams& params() const noexcept { return params_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    /// Updates run: the age line is age_cost x this.
    [[nodiscard]] std::uint32_t updates() const noexcept { return updates_; }
    /// Stored values, row-major, age folded in. For tests and equivalence.
    [[nodiscard]] const std::vector<std::int32_t>& values() const noexcept { return value_; }
    /// Cells that will carry their value on at the next update.
    [[nodiscard]] std::size_t active_cells() const noexcept { return active_.size(); }

private:
    void round(const Grid<bool>& blocked) noexcept;
    [[nodiscard]] std::size_t index(Vec2i c) const noexcept {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(c.x);
    }
    [[nodiscard]] std::int32_t age_line() const noexcept {
        return params_.age_cost * static_cast<std::int32_t>(updates_);
    }
    /// Offer `value` less distance_cost to the open neighbours of `from`.
    void offer(std::uint32_t from, std::int32_t value, const Grid<bool>& blocked) noexcept;

    WaveParams params_;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::int32_t> value_;
    /// A cell is in active_ when its stamp equals round_; so no cell is listed twice.
    std::vector<std::uint32_t> stamp_;
    std::vector<std::uint32_t> active_;
    std::vector<std::uint32_t> next_;
    /// Scratch: the active cells' values as a round starts.
    std::vector<std::int32_t> start_;
    std::uint32_t updates_ = 0;
    std::uint32_t round_ = 1; // stamps start at 0, so nothing begins listed
};

} // namespace peo::core
