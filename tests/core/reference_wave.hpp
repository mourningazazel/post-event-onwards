#pragma once

// PEO-078: the geodesic field as PEO-030 first built it, a push from the changed cells,
// kept as the test-only reference the dense pull in ScentWave must match on every
// update. The maths are unchanged from src/core/src/scent_wave.cpp at 7c80218.

#include "peo/core/grid.hpp"
#include "peo/core/scent_wave.hpp"
#include "peo/core/types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

namespace peo::test {

using core::Grid;
using core::Vec2i;
using core::WaveParams;

class ReferenceWave {
public:
    ReferenceWave(int width, int height, WaveParams params = {})
        : params_(params), width_(width),
          value_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), kUnreached),
          stamp_(value_.size(), 0) {}

    void deposit(Vec2i at, std::int32_t strength) {
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

    void update(const Grid<bool>& blocked) {
        ++updates_;
        for (int r = 0; r < params_.speed; ++r) {
            round(blocked);
        }
    }

    [[nodiscard]] std::int32_t sample(Vec2i at) const { return std::max(value_[index(at)] - age_line(), 0); }
    [[nodiscard]] const std::vector<std::int32_t>& values() const { return value_; }
    [[nodiscard]] std::uint32_t updates() const { return updates_; }

private:
    static constexpr std::int32_t kUnreached = std::numeric_limits<std::int32_t>::min() / 2;
    static constexpr std::size_t kOrthogonals = 4;

    [[nodiscard]] std::size_t index(Vec2i c) const {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(c.x);
    }
    [[nodiscard]] std::int32_t age_line() const {
        return params_.age_cost * static_cast<std::int32_t>(updates_);
    }

    void offer(std::uint32_t from, std::int32_t value, const Grid<bool>& blocked) {
        const std::int32_t candidate = value - params_.distance_cost;
        if (candidate <= age_line()) {
            return;
        }
        const Vec2i c{static_cast<int>(from % static_cast<std::uint32_t>(width_)),
                      static_cast<int>(from / static_cast<std::uint32_t>(width_))};
        const auto open = [&](Vec2i p) { return blocked.in_bounds(p) && !blocked.at(p); };
        std::array<bool, kOrthogonals> side{};
        for (std::size_t k = 0; k < kOrthogonals; ++k) {
            side[k] = open(c + core::kNeighbours4[k]);
        }
        for (std::size_t d = 0; d < std::size(core::kNeighbours8); ++d) {
            const std::size_t k = d / 2;
            if (d % 2 == 0 ? !side[k]
                           : !(side[k] && side[(k + 1) % kOrthogonals]) || !open(c + core::kNeighbours8[d])) {
                continue;
            }
            const std::size_t n = index(c + core::kNeighbours8[d]);
            if (candidate > value_[n]) {
                value_[n] = candidate;
                if (stamp_[n] != round_) {
                    stamp_[n] = round_;
                    next_.push_back(static_cast<std::uint32_t>(n));
                }
            }
        }
    }

    void round(const Grid<bool>& blocked) {
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

    WaveParams params_;
    int width_ = 0;
    std::vector<std::int32_t> value_;
    std::vector<std::uint32_t> stamp_;
    std::vector<std::uint32_t> active_;
    std::vector<std::uint32_t> next_;
    std::vector<std::int32_t> start_;
    std::uint32_t updates_ = 0;
    std::uint32_t round_ = 1;
};

} // namespace peo::test
