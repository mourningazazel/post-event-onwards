#include "peo/core/rng.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>

using peo::core::hash_u64;
using peo::core::hash_unit;
using peo::core::Rng;

TEST_SUITE("rng") {
    TEST_CASE("same seed gives same sequence") {
        Rng a(42);
        Rng b(42);
        for (int i = 0; i < 1000; ++i) {
            CHECK(a.next_u64() == b.next_u64());
        }
    }

    TEST_CASE("different seeds diverge") {
        Rng a(1);
        Rng b(2);
        CHECK(a.next_u64() != b.next_u64());
    }

    TEST_CASE("range is inclusive and bounded") {
        Rng rng(7);
        bool saw_lo = false;
        bool saw_hi = false;
        for (int i = 0; i < 10000; ++i) {
            const int v = rng.range(-3, 3);
            CHECK(v >= -3);
            CHECK(v <= 3);
            saw_lo |= v == -3;
            saw_hi |= v == 3;
        }
        CHECK(saw_lo);
        CHECK(saw_hi);
    }

    TEST_CASE("unit is in [0,1)") {
        Rng rng(99);
        for (int i = 0; i < 10000; ++i) {
            const float u = rng.unit();
            CHECK(u >= 0.0F);
            CHECK(u < 1.0F);
        }
    }

    TEST_CASE("hash_u64 is a pure function of all three words") {
        constexpr std::uint64_t kA = 7;
        constexpr std::uint64_t kB = 11;
        constexpr std::uint64_t kC = 13;
        const std::uint64_t h = hash_u64(kA, kB, kC);
        CHECK(hash_u64(kA, kB, kC) == h);
        CHECK(hash_u64(kA + 1, kB, kC) != h);
        CHECK(hash_u64(kA, kB + 1, kC) != h);
        CHECK(hash_u64(kA, kB, kC + 1) != h);
        CHECK(hash_u64(kB, kA, kC) != h); // order matters
    }

    TEST_CASE("hash_unit is uniform in [0,1) over consecutive counters") {
        constexpr int kDraws = 10000;
        constexpr double kTolerance = 0.02;
        double sum = 0.0;
        for (int i = 0; i < kDraws; ++i) {
            const float u = hash_unit(1, 2, static_cast<std::uint64_t>(i));
            REQUIRE(u >= 0.0F);
            REQUIRE(u < 1.0F);
            sum += static_cast<double>(u);
        }
        CHECK(std::abs(sum / kDraws - 0.5) < kTolerance);
    }
}
