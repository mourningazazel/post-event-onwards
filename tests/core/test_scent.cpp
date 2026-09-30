#include "peo/core/rng.hpp"
#include "peo/core/scent.hpp"

#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

using namespace peo::core;

namespace {
// Reach case (PEO-026). Measured, not estimated: after kReachSteps the value at
// kReachFarX is 4.55e-4, 45x kReachThreshold, and the trail strictly decreases
// past it. Do not scale this up to 80x45: ~200 ms per test under Debug+ASan.
constexpr int kReachWidth = 34;
constexpr int kReachHeight = 11;
constexpr Vec2i kReachSource{4, 5};
constexpr int kReachSteps = 40;
constexpr int kReachFarX = 19;
constexpr float kReachThreshold = 1e-5F;
} // namespace

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

    TEST_CASE("strongest neighbour never cuts a wall corner") {
        // PEO-044: centre (1,1) on a 3x3 field, the strongest cell diagonal at (2,2).
        // The orthogonal cells it passes are (2,1) and (1,2); both must be open.
        constexpr Vec2i kCentre{1, 1};
        constexpr Vec2i kDiagonal{2, 2};
        constexpr Vec2i kEast{2, 1};
        constexpr Vec2i kSouth{1, 2};
        ScentField f(3, 3);
        f.deposit(kCentre, 1.0F);
        f.deposit(kDiagonal, 5.0F); // the orthogonal cells stay 0: weaker than the centre
        Grid<bool> walls(3, 3, false);

        SUBCASE("both orthogonal cells blocked: no move") {
            walls.at(kEast) = true;
            walls.at(kSouth) = true;
            CHECK_FALSE(f.strongest_neighbour(kCentre, &walls).has_value());
        }
        SUBCASE("one orthogonal cell blocked: still no move") {
            walls.at(kSouth) = true;
            CHECK_FALSE(f.strongest_neighbour(kCentre, &walls).has_value());
            walls.at(kSouth) = false;
            walls.at(kEast) = true;
            CHECK_FALSE(f.strongest_neighbour(kCentre, &walls).has_value());
        }
        SUBCASE("both open: the diagonal") {
            CHECK(f.strongest_neighbour(kCentre, &walls) == kDiagonal);
        }
    }

    TEST_CASE("floor clamps tiny values to zero") {
        ScentField f(3, 3, {.diffusion = 0.0F, .decay = 0.9F, .floor = 0.05F});
        f.deposit({1, 1}, 0.1F);
        f.step();
        CHECK(f.sample({1, 1}) == doctest::Approx(0.0));
    }

    TEST_CASE("the scent front carries far") {
        ScentField f(kReachWidth, kReachHeight);
        for (int i = 0; i < kReachSteps; ++i) { // a player standing still
            f.deposit(kReachSource, kPlayerScent);
            f.step();
        }
        CHECK(f.sample({kReachFarX, kReachSource.y}) > kReachThreshold);
        for (int x = kReachSource.x + 1; x <= kReachFarX; ++x) {
            CAPTURE(x);
            CHECK(f.sample({x, kReachSource.y}) < f.sample({x - 1, kReachSource.y}));
        }
    }

    TEST_CASE("step is bit-identical across builds") {
        // D-002: the same inputs give the same bits on every compiler, CPU and
        // build type (PEO-028: -ffp-contract=off). Pinned exactly, not Approx: CI's
        // g++-13, clang++-18 and release jobs all run this and must agree.
        constexpr int kSize = 5;
        constexpr int kSteps = 4;
        constexpr std::array<std::uint32_t, kSize * kSize> kExpected{
            0x3F341C9CU, 0x410789E4U, 0x41FC98B7U, 0x4034B7DAU, 0x3E702626U, 0x410789E4U, 0x427C98B8U,
            0x435A6611U, 0x00000000U, 0x4034B7DAU, 0x42002CA8U, 0x43600BD0U, 0x44126439U, 0x435A6611U,
            0x41FC98B8U, 0x410789E4U, 0x427D88DFU, 0x43600BD0U, 0x427C98B7U, 0x410789E4U, 0x3F341C9CU,
            0x410789E4U, 0x42002CA8U, 0x410789E4U, 0x3F341C9CU,
        };
        ScentField f(kSize, kSize, {.diffusion = 0.5F, .decay = 0.01F, .floor = 1e-6F});
        Grid<bool> walls(kSize, kSize, false);
        walls.at(3, 1) = true;
        for (int i = 0; i < kSteps; ++i) {
            f.deposit({2, 2}, kPlayerScent);
            f.step(&walls);
        }
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(std::bit_cast<std::uint32_t>(f.sample({x, y})) ==
                      kExpected[static_cast<std::size_t>(y * kSize + x)]);
            }
        }
    }

    TEST_CASE("step_linear is linear: a deposit can be patched in afterwards") {
        // PEO-007. Deposit-then-step_linear versus step_linear-then-patch_deposit.
        // Outside the source and its four neighbours the two must agree bit for bit;
        // on those five cells, to float rounding (addition is not associative, so
        // exactness there is guaranteed by World using one order everywhere).
        constexpr int kTrials = 50;
        constexpr int kW = 9;
        constexpr int kH = 7;
        constexpr float kWallChance = 0.2F;
        constexpr float kMaxCell = 500.0F;
        constexpr float kRelTolerance = 1e-5F;
        const ScentParams params{.diffusion = 0.5F, .decay = 0.01F, .floor = 1e-6F};
        Rng rng(kTrials);
        for (int t = 0; t < kTrials; ++t) {
            Grid<bool> walls(kW, kH, false);
            ScentField base(kW, kH, params);
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    walls.at(x, y) = rng.chance(kWallChance);
                    base.deposit({x, y}, rng.unit() * kMaxCell);
                }
            }
            const Vec2i at{rng.range(0, kW - 1), rng.range(0, kH - 1)};
            walls.at(at) = false;
            const float amount = rng.unit() * kMaxCell;

            ScentField a = base;
            a.deposit(at, amount);
            a.step_linear(&walls);
            ScentField b = base;
            b.step_linear(&walls);
            b.patch_deposit(at, amount, &walls);

            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    CAPTURE(t);
                    CAPTURE(x);
                    CAPTURE(y);
                    const Vec2i c{x, y};
                    const bool touched = std::abs(c.x - at.x) + std::abs(c.y - at.y) <= 1;
                    if (touched) {
                        CHECK(std::abs(a.sample(c) - b.sample(c)) <= kRelTolerance * std::abs(a.sample(c)));
                    } else {
                        CHECK(std::bit_cast<std::uint32_t>(a.sample(c)) ==
                              std::bit_cast<std::uint32_t>(b.sample(c)));
                    }
                }
            }
        }
    }

    TEST_CASE("step equals step_linear then clamp_floor") {
        const ScentParams params{.diffusion = 0.5F, .decay = 0.01F, .floor = 0.05F};
        ScentField a(8, 8, params);
        a.deposit({3, 3}, 1.0F);
        ScentField b = a;
        for (int i = 0; i < 6; ++i) {
            a.step();
            b.step_linear();
            b.clamp_floor();
        }
        for (int i = 0; i < 64; ++i) {
            CHECK(std::bit_cast<std::uint32_t>(a.cells().data()[i]) ==
                  std::bit_cast<std::uint32_t>(b.cells().data()[i]));
        }
    }
}
