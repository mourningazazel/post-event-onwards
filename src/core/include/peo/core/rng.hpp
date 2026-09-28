#pragma once

#include "peo/core/types.hpp"

#include <cstdint>
#include <limits>

namespace peo::core {

/// xoshiro256** — small, fast, and deterministic across platforms. Never use
/// std::mt19937 or rand() in the simulation: they are slower and their sequences
/// differ between standard library implementations.
class Rng {
public:
    explicit Rng(Seed seed) noexcept { reseed(seed); }

    void reseed(Seed seed) noexcept {
        // splitmix64 expands the seed into four non-zero state words.
        for (auto& word : state_) {
            seed += 0x9E3779B97F4A7C15ULL;
            std::uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            word = z ^ (z >> 31);
        }
    }

    std::uint64_t next_u64() noexcept {
        const std::uint64_t result = rotl(state_[1] * 5, 7) * 9;
        const std::uint64_t t = state_[1] << 17;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = rotl(state_[3], 45);
        return result;
    }

    /// Uniform integer in [lo, hi] inclusive. Requires lo <= hi.
    int range(int lo, int hi) noexcept {
        const auto span = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(next_u64() % span);
    }

    /// Uniform float in [0, 1).
    float unit() noexcept {
        constexpr std::uint64_t kMantissaBits = 24;
        return static_cast<float>(next_u64() >> (64 - kMantissaBits)) /
               static_cast<float>(1U << kMantissaBits);
    }

    /// True with probability p (clamped to [0, 1]).
    bool chance(float p) noexcept { return unit() < p; }

private:
    static constexpr std::uint64_t rotl(std::uint64_t v, int k) noexcept {
        return (v << k) | (v >> (64 - k));
    }

    std::uint64_t state_[4]{};
};

} // namespace peo::core
