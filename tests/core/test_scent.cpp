#include "peo/core/scent.hpp"

#include <doctest/doctest.h>

using namespace peo::core;

TEST_SUITE("scent") {
    TEST_CASE("deposit then sample") {
        ScentField f(10, 10);
        f.deposit({5, 5}, 1.0F);
        CHECK(f.sample({5, 5}) == doctest::Approx(1.0));
        CHECK(f.sample({4, 5}) == doctest::Approx(0.0));
        CHECK(f.sample({99, 99}) == doctest::Approx(0.0)); // out of bounds is silent
    }

    TEST_CASE("one step spreads to orthogonal neighbours only") {
        ScentField f(10, 10, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        f.deposit({5, 5}, 1.0F);
        f.step();
        CHECK(f.sample({5, 5}) == doctest::Approx(0.6));
        CHECK(f.sample({5, 4}) == doctest::Approx(0.1));
        CHECK(f.sample({6, 5}) == doctest::Approx(0.1));
        CHECK(f.sample({6, 6}) == doctest::Approx(0.0)); // diagonal untouched
    }

    TEST_CASE("mass is conserved without decay while the front is interior") {
        // Scent spreads one cell per step. From (10,10) on a 20x20 field the
        // nearest off-map cell is x = 20, ten steps out: nine steps stay interior.
        constexpr int kInteriorSteps = 9;
        ScentField f(20, 20, {.diffusion = 0.3F, .decay = 0.0F, .floor = 0.0F});
        f.deposit({10, 10}, 4.0F);
        for (int i = 0; i < kInteriorSteps; ++i) {
            f.step();
        }
        CHECK(f.total() == doctest::Approx(4.0).epsilon(0.001));
        // One more step and the front touches the edge, which absorbs.
        const float interior = f.total();
        f.step();
        CHECK(f.total() < interior);
    }

    TEST_CASE("decay removes mass") {
        ScentField f(20, 20, {.diffusion = 0.0F, .decay = 0.5F, .floor = 0.0F});
        f.deposit({10, 10}, 1.0F);
        f.step();
        CHECK(f.total() == doctest::Approx(0.5));
    }

    TEST_CASE("walls absorb scent and pass none on") {
        const ScentParams params{.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F};
        ScentField open(5, 5, params);
        ScentField f(5, 5, params);
        Grid<bool> walls(5, 5, false);
        walls.at(3, 2) = true;
        open.deposit({2, 2}, 1.0F);
        f.deposit({2, 2}, 1.0F);
        open.step();
        f.step(&walls);
        CHECK(f.sample({3, 2}) == 0.0F); // the wall itself never holds scent
        CHECK(f.sample({1, 2}) == doctest::Approx(0.1));
        // No pile-up: the cell beside the wall is exactly what it is in the open.
        CHECK(f.sample({2, 2}) == open.sample({2, 2}));
        // The share sent into the wall (0.4 / 4) is gone.
        CHECK(f.total() == doctest::Approx(0.9));
    }

    TEST_CASE("strongest neighbour climbs the gradient") {
        ScentField f(7, 7, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        f.deposit({5, 3}, 1.0F);
        for (int i = 0; i < 5; ++i) {
            f.step();
        }
        Vec2i p{1, 3};
        for (int i = 0; i < 4; ++i) {
            const auto next = f.strongest_neighbour(p);
            REQUIRE(next.has_value());
            p = *next;
        }
        CHECK(p == Vec2i{5, 3});
        CHECK_FALSE(f.strongest_neighbour(p).has_value()); // at the peak
    }

    TEST_CASE("floor clamps tiny values to zero") {
        ScentField f(3, 3, {.diffusion = 0.0F, .decay = 0.9F, .floor = 0.05F});
        f.deposit({1, 1}, 0.1F);
        f.step();
        CHECK(f.sample({1, 1}) == doctest::Approx(0.0));
    }
}
