#include "peo/core/desire.hpp"

#include <algorithm>
#include <limits>

namespace peo::core {

namespace {
/// round(65536 x 2^(i/16)) for i = 0..15.
constexpr std::array<std::uint32_t, kOctave> kOctaveSteps{65536,  68438,  71468,  74632, 77936,  81386,
                                                          84990,  88752,  92682,  96785, 101070, 105545,
                                                          110218, 115098, 120194, 125515};
/// Log-odds range of a choice: an int8 less the closed marker.
constexpr std::int32_t kMaxLogOdds = std::numeric_limits<std::int8_t>::max();
/// weight_of shifts the octave index up from the lowest octave, then this far down, so
/// a level neighbour (log-odds 0, octave 8) weighs exactly kOctaveSteps[0].
constexpr int kWeightShift = 8;
constexpr std::int32_t kLogOddsBias = 128;
/// Scale terms by a 16-bit reciprocal instead of dividing per neighbour: x / d is
/// taken as (x x ceil(65536 / d)) >> 16, so exact multiples of d come out exact and the
/// rest round toward minus infinity.
constexpr int kReciprocalShift = 16;
constexpr std::int64_t kReciprocalOne = std::int64_t{1} << kReciprocalShift;
} // namespace

std::uint32_t weight_of(std::int8_t e) noexcept {
    if (e == DesireField::kNoChoice) {
        return 0;
    }
    const auto biased = static_cast<std::uint32_t>(std::int32_t{e} + kLogOddsBias); // 1..255
    return (kOctaveSteps[biased % kOctave] << (biased / kOctave)) >> kWeightShift;
}

DesireField::DesireField(int width, int height)
    : width_(width), height_(height),
      log_odds_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kDrawChoices, 0),
      crowd_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0),
      sums_(static_cast<std::size_t>(width + 1) * static_cast<std::size_t>(height + 1), 0),
      moves_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0) {}

void DesireField::build_moves(const Grid<bool>& blocked, const ScentWave& scent) {
    if (moves_built_ && scent.token() == moves_token_) {
        return;
    }
    moves_token_ = scent.token();
    const auto open = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < width_ && y < height_ && !blocked.at(x, y);
    };
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            std::uint8_t m = 0;
            for (std::size_t d = 0; d < std::size(kNeighbours8); ++d) {
                const int nx = x + kNeighbours8[d].x;
                const int ny = y + kNeighbours8[d].y;
                const bool diagonal = kNeighbours8[d].x != 0 && kNeighbours8[d].y != 0;
                if (open(nx, ny) && (!diagonal || (open(nx, y) && open(x, ny)))) {
                    m = static_cast<std::uint8_t>(m | (1U << d));
                }
            }
            moves_[index({x, y})] = m;
        }
    }
    moves_built_ = true;
}

void DesireField::build(const ScentWave& scent, const Grid<bool>& blocked, const Grid<std::uint8_t>& occupied,
                        const DeadDrawParams& params, Executor* executor) {
    build_moves(blocked, scent);
    // The occupancy snapshot, as each cell's count of Dead in the box of half-size r
    // round it (from a summed-area table): a neighbour's company is then one read.
    const auto stride = static_cast<std::size_t>(width_ + 1);
    for (int y = 0; y < height_; ++y) {
        std::int32_t row = 0;
        for (int x = 0; x < width_; ++x) {
            row += occupied.at(x, y);
            sums_[static_cast<std::size_t>(y + 1) * stride + static_cast<std::size_t>(x + 1)] =
                sums_[static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x + 1)] + row;
        }
    }
    const int r = std::max(params.company_radius, 0);
    const auto sum = [&](int x, int y) {
        return sums_[static_cast<std::size_t>(std::clamp(y, 0, height_)) * stride +
                     static_cast<std::size_t>(std::clamp(x, 0, width_))];
    };
    run_ranges(executor, static_cast<std::size_t>(height_), [&](std::size_t begin, std::size_t end) {
        for (auto y = static_cast<int>(begin); y < static_cast<int>(end); ++y) {
            for (int x = 0; x < width_; ++x) {
                const std::int32_t n = sum(x + r + 1, y + r + 1) - sum(x - r, y + r + 1) -
                                       sum(x + r + 1, y - r) + sum(x - r, y - r);
                crowd_[index({x, y})] = static_cast<std::uint16_t>(n);
            }
        }
    });
    // Each band of rows writes only its own cells.
    run_ranges(executor, static_cast<std::size_t>(height_), [&](std::size_t begin, std::size_t end) {
        build_rows(scent, params, 0, width_, static_cast<int>(begin), static_cast<int>(end));
    });
}

void DesireField::rebuild(const ScentWave& scent, const Grid<bool>& blocked, const DeadDrawParams& params,
                          int x0, int y0, int x1, int y1) {
    build_moves(blocked, scent);
    build_rows(scent, params, std::max(x0, 0), std::min(x1, width_), std::max(y0, 0), std::min(y1, height_));
}

void DesireField::build_rows(const ScentWave& scent, const DeadDrawParams& params, int x0, int x1, int y0,
                             int y1) {
    const std::vector<std::int32_t>& values = scent.values();
    const std::int64_t line = std::int64_t{scent.params().age_cost} * scent.updates();
    const std::int64_t strength = std::max<std::int32_t>(scent.params().strength, 1);
    const std::int64_t cost = std::max<std::int32_t>(scent.params().distance_cost, 1);
    const std::int64_t per_cost = (kReciprocalOne + cost - 1) / cost;
    const int r = std::max(params.company_radius, 0);
    const std::int64_t box = std::int64_t{2 * r + 1} * (2 * r + 1);
    const std::int64_t per_box = (kReciprocalOne + box - 1) / box;
    const std::int64_t gain = params.company_gain;
    const std::int64_t span = std::int64_t{params.lean_full} - params.lean_edge;
    const auto stay = static_cast<std::int8_t>(std::clamp(params.stay, -kMaxLogOdds, kMaxLogOdds));
    const auto w = static_cast<std::ptrdiff_t>(width_);
    std::array<std::ptrdiff_t, kDrawChoices - 1> step{};    // to the neighbour
    std::array<std::ptrdiff_t, kDrawChoices - 1> company{}; // to its box's centre
    for (std::size_t d = 0; d < step.size(); ++d) {
        step[d] = kNeighbours8[d].y * w + kNeighbours8[d].x;
        company[d] = step[d] * (r + 1);
    }
    const auto sample = [&](std::size_t i) { return std::max<std::int64_t>(values[i] - line, 0); };
    const auto lined =
        static_cast<std::int32_t>(std::min<std::int64_t>(line, std::numeric_limits<std::int32_t>::max()));
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t i = index({x, y});
            std::int8_t* out = &log_odds_[i * kDrawChoices];
            out[kStayChoice] = stay;
            const std::uint8_t moves = moves_[i];
            // The company box lies r + 1 cells out: inside the map for every direction
            // when the cell is more than 2r from each edge.
            const bool inner = x > 2 * r && y > 2 * r && x < width_ - 2 * r - 1 && y < height_ - 2 * r - 1;
            const auto crowd_at = [&](std::size_t d) -> std::int64_t {
                if (inner) {
                    return crowd_[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i) + company[d])];
                }
                const int cx = x + kNeighbours8[d].x * (r + 1);
                const int cy = y + kNeighbours8[d].y * (r + 1);
                return cx >= 0 && cy >= 0 && cx < width_ && cy < height_ ? crowd_[index({cx, cy})] : 0;
            };
            // Flat: no scent here or at any open neighbour, and no company. Every open
            // choice is level, so the maths is skipped (most of a stage, far from the
            // player and the horde).
            bool flat = values[i] <= lined && (gain == 0 || crowd_[i] == 0);
            for (std::size_t d = 0; d < step.size() && flat; ++d) {
                if ((moves >> d) & 1U) {
                    flat =
                        values[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i) + step[d])] <= lined &&
                        (gain == 0 || crowd_at(d) == 0);
                }
            }
            if (flat) {
                for (std::size_t d = 0; d < step.size(); ++d) {
                    out[d] = ((moves >> d) & 1U) != 0 ? std::int8_t{0} : kNoChoice;
                }
                continue;
            }
            const std::int64_t here = sample(i);
            // The lean rises with the scent where the unit stands (D-038 B).
            const std::int64_t lean = params.lean_edge + span * here / strength;
            for (std::size_t d = 0; d < step.size(); ++d) {
                if (((moves >> d) & 1U) == 0) {
                    out[d] = kNoChoice;
                    continue;
                }
                const auto n = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i) + step[d]);
                std::int64_t e = (lean * (sample(n) - here) * per_cost) >> kReciprocalShift;
                if (gain != 0) {
                    e += (gain * crowd_at(d) * per_box) >> kReciprocalShift;
                }
                out[d] = static_cast<std::int8_t>(std::clamp<std::int64_t>(e, -kMaxLogOdds, kMaxLogOdds));
            }
        }
    }
}

std::array<std::uint32_t, kDrawChoices> DesireField::weights(Vec2i at) const noexcept {
    std::array<std::uint32_t, kDrawChoices> w{};
    const std::int8_t* in = &log_odds_[index(at) * kDrawChoices];
    for (std::size_t c = 0; c < kDrawChoices; ++c) {
        w[c] = weight_of(in[c]);
    }
    return w;
}

} // namespace peo::core
