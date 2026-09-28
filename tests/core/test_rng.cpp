#include "peo/core/rng.hpp"

#include <doctest/doctest.h>

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
}
