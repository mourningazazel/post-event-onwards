#include "peo/core/scent_wave.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <limits>

namespace peo::core {

namespace {
/// Stored value of a cell nothing has reached: far below any age line.
constexpr std::int32_t kUnreached = std::numeric_limits<std::int32_t>::min() / 2;

/// kNeighbours8 alternates orthogonal and diagonal (N, NE, E, SE, S, SW, W, NW): the
/// diagonal 2k+1 lies between orthogonals k and k+1 of kNeighbours4 (as scent.cpp).
constexpr std::size_t kOrthogonals = 4;
} // namespace

ScentWave::ScentWave(int width, int height, WaveParams params)
    : params_(params), width_(width), height_(height),
      value_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), kUnreached),
      stamp_(value_.size(), 0) {
    // Bound the lists once: at most every cell is listed.
    active_.reserve(value_.size());
    next_.reserve(value_.size());
    start_.reserve(value_.size());
}

void ScentWave::deposit(Vec2i at, std::int32_t strength) noexcept {
    const std::size_t i = index(at);
    const std::int32_t v = age_line() + strength;
    if (v > value_[i]) {
        value_[i] = v;
        if (stamp_[i] != round_) {
            stamp_[i] = round_;
            active_.push_back(static_cast<std::uint32_t>(i));
        }
    }
}

void ScentWave::offer(std::uint32_t from, std::int32_t value, const Grid<bool>& blocked) noexcept {
    const std::int32_t candidate = value - params_.distance_cost;
    if (candidate <= age_line()) {
        return; // spent: nothing past the reach is ever written
    }
    const Vec2i c{static_cast<int>(from % static_cast<std::uint32_t>(width_)),
                  static_cast<int>(from / static_cast<std::uint32_t>(width_))};
    const auto open = [&](Vec2i p) { return blocked.in_bounds(p) && !blocked.at(p); };
    std::array<bool, kOrthogonals> side{};
    for (std::size_t k = 0; k < kOrthogonals; ++k) {
        side[k] = open(c + kNeighbours4[k]);
    }
    for (std::size_t d = 0; d < std::size(kNeighbours8); ++d) {
        const std::size_t k = d / 2;
        // An orthogonal step needs its cell open; a diagonal both cells beside it too.
        if (d % 2 == 0 ? !side[k]
                       : !(side[k] && side[(k + 1) % kOrthogonals]) || !open(c + kNeighbours8[d])) {
            continue;
        }
        const std::size_t n = index(c + kNeighbours8[d]);
        if (candidate > value_[n]) {
            value_[n] = candidate;
            if (stamp_[n] != round_) {
                stamp_[n] = round_;
                next_.push_back(static_cast<std::uint32_t>(n));
            }
        }
    }
}

// A round offers the values active cells had when it started, so a cell improved
// during the round passes that on only next round: the result is a max over offers
// and does not depend on list order. That is what makes patch_deposit exact.
void ScentWave::round(const Grid<bool>& blocked) noexcept {
    start_.resize(active_.size());
    for (std::size_t k = 0; k < active_.size(); ++k) {
        start_[k] = value_[active_[k]];
    }
    ++round_;
    next_.clear();
    for (std::size_t k = 0; k < active_.size(); ++k) {
        offer(active_[k], start_[k], blocked);
    }
    std::swap(active_, next_);
}

void ScentWave::update(const Grid<bool>& blocked) noexcept {
    ++updates_;
    for (int r = 0; r < params_.speed; ++r) {
        round(blocked);
    }
}

void ScentWave::patch_deposit(const ScentWave& before, Vec2i at, std::int32_t strength,
                              const Grid<bool>& blocked) noexcept {
    const std::size_t i = index(at);
    // As deposit() would have made it, before the update: only a deposit that beat
    // the cell's value then took part in the update's round.
    const std::int32_t v = before.age_line() + strength;
    if (v <= before.value_[i]) {
        return;
    }
    value_[i] = std::max(value_[i], v);
    // active_ is the round's output here, stamped round_: offer() adds to it as the
    // round would have. next_ is the output list during a round; alias it for offer().
    std::swap(active_, next_);
    offer(static_cast<std::uint32_t>(i), v, blocked);
    std::swap(active_, next_);
}

std::int32_t ScentWave::sample(Vec2i at) const noexcept {
    return std::max(value_[index(at)] - age_line(), 0);
}

std::optional<Vec2i> ScentWave::strongest_neighbour(Vec2i from, const Grid<bool>* blocked) const noexcept {
    std::int32_t best = sample(from);
    std::optional<Vec2i> result;
    const auto open = [&](Vec2i c) {
        return c.x >= 0 && c.y >= 0 && c.x < width_ && c.y < height_ && !(blocked && blocked->at(c));
    };
    std::array<bool, kOrthogonals> side{};
    for (std::size_t k = 0; k < kOrthogonals; ++k) {
        side[k] = open(from + kNeighbours4[k]);
    }
    for (std::size_t d = 0; d < std::size(kNeighbours8); ++d) {
        const std::size_t k = d / 2;
        const Vec2i n = from + kNeighbours8[d];
        if (d % 2 == 0 ? !side[k]
                       : !(side[k] && side[(k + 1) % kOrthogonals]) || (blocked && blocked->at(n))) {
            continue;
        }
        const std::int32_t v = sample(n);
        if (v > best) {
            best = v;
            result = n;
        }
    }
    return result;
}

} // namespace peo::core
