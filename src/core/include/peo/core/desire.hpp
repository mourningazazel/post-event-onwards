#pragma once

#include "peo/core/executor.hpp"
#include "peo/core/grid.hpp"
#include "peo/core/scent_wave.hpp"
#include "peo/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace peo::core {

/// Log-odds are counted in sixteenths of an octave (PEO-009): +16 doubles a weight.
inline constexpr std::int32_t kOctave = 16;
/// The draw's nine choices: kNeighbours8's eight, then staying.
inline constexpr std::size_t kDrawChoices = 9;
inline constexpr std::size_t kStayChoice = 8;

/// The calm Dead's draw (D-038 B, PEO-009); every log-odds in sixteenths of an octave.
/// Defaults pass the swarm scenarios (tests/core/test_swarm.cpp), whose measured shares
/// are in that file.
struct DeadDrawParams {
    /// How strongly a unit favours a neighbour whose scent is one distance_cost above
    /// its own: at the edge of reach (its scent near 0) and at full strength (its scent
    /// equal to the deposit strength); between, it rises in proportion to its scent, so
    /// near a fresh trail the Dead climb almost every step and far out they wander with
    /// a lean (D-038). A cell further along a trail is distance_cost + age_cost higher,
    /// so a step of trail counts twice.
    std::int32_t lean_edge = 0;
    std::int32_t lean_full = 3 * kOctave;
    /// Staying, against a level neighbour.
    std::int32_t stay = 0;
    /// A mild pull toward other Dead: the log-odds toward a direction whose box (half-size
    /// company_radius, centred company_radius + 1 cells out) is full of Dead, in
    /// proportion to how full it is. Per Dead it was far too strong: a packed crowd held
    /// its edge in place and never fanned out.
    std::int32_t company_gain = kOctave / 4;
    int company_radius = 3;
};

/// Every term of the calm Dead's draw that reads only the last update (scent and
/// company today), baked per cell after each update (D-037, PEO-009): nine log-odds
/// a cell, so a decision is a lookup and a draw. Built from the field and a snapshot of
/// the occupancy at the update; walls and cut corners (PEO-044) weigh nothing. The
/// lean reads the scent's absolute strength, which the age line lowers every update,
/// so every reached cell is rebuilt each update. Integer only (D-021).
class DesireField {
public:
    DesireField() = default;
    DesireField(int width, int height);

    /// Rebuild every cell, by bands of rows on `executor` (any executor, same bits).
    void build(const ScentWave& scent, const Grid<bool>& blocked, const Grid<std::uint8_t>& occupied,
               const DeadDrawParams& params, Executor* executor);
    /// Rebuild the cells in [x0, x1) x [y0, y1), clamped, with the occupancy of the last
    /// build: after patch_deposit raised the field there (PEO-009's commit patch).
    void rebuild(const ScentWave& scent, const Grid<bool>& blocked, const DeadDrawParams& params, int x0,
                 int y0, int x1, int y1);

    /// The nine weights at `at`, kNeighbours8 order then staying: 0 for a wall or a cut
    /// corner, otherwise 2^(log-odds / 16) scaled so a level neighbour weighs 65536.
    [[nodiscard]] std::array<std::uint32_t, kDrawChoices> weights(Vec2i at) const noexcept;
    /// The log-odds behind weights(); kNoChoice where the choice is closed.
    [[nodiscard]] std::int8_t log_odds(Vec2i at, std::size_t choice) const noexcept {
        return log_odds_[index(at) * kDrawChoices + choice];
    }
    static constexpr std::int8_t kNoChoice = -128;

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    /// Memory a cell costs: its nine log-odds, its crowd count, its share of the
    /// occupancy sums and its move mask.
    [[nodiscard]] static constexpr std::size_t bytes_per_cell() noexcept {
        return kDrawChoices * sizeof(std::int8_t) + sizeof(std::uint16_t) + sizeof(std::int32_t) +
               sizeof(std::uint8_t);
    }
    [[nodiscard]] bool operator==(const DesireField& other) const noexcept {
        return width_ == other.width_ && height_ == other.height_ && log_odds_ == other.log_odds_;
    }

private:
    [[nodiscard]] std::size_t index(Vec2i c) const noexcept {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(c.x);
    }
    void build_rows(const ScentWave& scent, const DeadDrawParams& params, int x0, int x1, int y0, int y1);
    /// The moves the stage allows from each cell (bit d: neighbour d open, and for a
    /// diagonal both cells beside it, PEO-044). Kept while the scent wave's token says
    /// the stage is the same (a Speculation's field outlives stages and may serve
    /// another World); a wave with no token keeps the first grid's.
    void build_moves(const Grid<bool>& blocked, const ScentWave& scent);

    int width_ = 0;
    int height_ = 0;
    std::vector<std::int8_t> log_odds_;
    /// Per cell, the Dead in the box of half-size company_radius round it, at the last
    /// build; and the summed-area table that counts them, (width + 1) x (height + 1).
    std::vector<std::uint16_t> crowd_;
    std::vector<std::int32_t> sums_;
    std::vector<std::uint8_t> moves_;
    bool moves_built_ = false;
    std::uint64_t moves_token_ = 0;
};

/// A weight of log-odds `e` (sixteenths of an octave), from a fixed table: 0 for
/// kNoChoice, 65536 for 0, about 256 at -127 and 16 million at +127.
[[nodiscard]] std::uint32_t weight_of(std::int8_t e) noexcept;

} // namespace peo::core
