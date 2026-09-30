#include "peo/core/rng.hpp"
#include "peo/core/scent_wave.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace peo::core;

namespace {

constexpr int kSide = 41;
constexpr Vec2i kCentre{20, 20};
/// Distance only: no ageing, so a value reads strength - distance_cost x route.
constexpr WaveParams kNoAge{.strength = 200, .distance_cost = 8, .age_cost = 0, .speed = 1};

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

void run(ScentWave& w, const Grid<bool>& walls, int updates) {
    for (int i = 0; i < updates; ++i) {
        w.update(walls);
    }
}

} // namespace

TEST_SUITE("scent_wave") {
    TEST_CASE("on an open grid a deposit reads strength less distance_cost per cell") {
        const Grid<bool> walls(kSide, kSide, false);
        ScentWave w(kSide, kSide, kNoAge);
        w.deposit(kCentre, kNoAge.strength);
        run(w, walls, kSide);
        for (int y = 0; y < kSide; ++y) {
            for (int x = 0; x < kSide; ++x) {
                const int d = chebyshev({x, y}, kCentre); // 8-connected route on open ground
                CAPTURE(x);
                CAPTURE(y);
                CHECK(w.sample({x, y}) == std::max(0, kNoAge.strength - kNoAge.distance_cost * d));
            }
        }
    }

    TEST_CASE("a wall line absorbs: nothing behind it without a route round") {
        Grid<bool> walls(kSide, kSide, false);
        constexpr int kWallX = 25;
        for (int y = 0; y < kSide; ++y) {
            walls.at(kWallX, y) = true;
        }
        ScentWave w(kSide, kSide, kNoAge);
        w.deposit(kCentre, kNoAge.strength);
        run(w, walls, kSide);
        for (int y = 0; y < kSide; ++y) {
            for (int x = kWallX; x < kSide; ++x) {
                CHECK(w.sample({x, y}) == 0);
            }
        }
        CHECK(w.sample({kWallX - 1, kCentre.y}) > 0);
    }

    TEST_CASE("a one-wide corridor loses exactly distance_cost per cell") {
        constexpr int kLength = 20;
        constexpr int kRow = 1;
        Grid<bool> walls(kLength, 3, true);
        for (int x = 0; x < kLength; ++x) {
            walls.at(x, kRow) = false;
        }
        ScentWave w(kLength, 3, kNoAge);
        w.deposit({0, kRow}, kNoAge.strength);
        run(w, walls, kLength);
        for (int x = 1; x < kLength; ++x) {
            CHECK(w.sample({x - 1, kRow}) - w.sample({x, kRow}) ==
                  std::min(kNoAge.distance_cost, w.sample({x - 1, kRow})));
        }
        CHECK(w.sample({kLength - 1, kRow}) ==
              std::max(0, kNoAge.strength - kNoAge.distance_cost * (kLength - 1)));
    }

    TEST_CASE("with no deposit the field ages by age_cost per update") {
        const WaveParams p{};
        const Grid<bool> walls(kSide, kSide, false);
        ScentWave w(kSide, kSide, p);
        w.deposit(kCentre, p.strength);
        constexpr int kSettle = 3;
        run(w, walls, kSettle);
        const Vec2i near = kCentre + Vec2i{1, 0};
        std::int32_t before = w.sample(near);
        REQUIRE(before > 0);
        for (int i = 0; i < kSettle; ++i) {
            w.update(walls);
            CHECK(w.sample(near) == before - p.age_cost);
            before = w.sample(near);
        }
    }

    TEST_CASE("speed 2 advances the front two cells per update") {
        const Grid<bool> walls(kSide, kSide, false);
        for (const int speed : {1, 2}) {
            ScentWave w(kSide, kSide, {.speed = speed});
            w.deposit(kCentre, w.params().strength);
            w.update(walls);
            CAPTURE(speed);
            CHECK(w.sample(kCentre + Vec2i{speed, 0}) > 0);
            CHECK(w.sample(kCentre + Vec2i{speed + 1, 0}) == 0);
        }
    }

    TEST_CASE("the wave never cuts a wall corner") {
        // (1,1) and (2,2) touch only diagonally between the walls at (2,1) and (1,2);
        // the only other way round is sealed, so (2,2) must stay at 0.
        Grid<bool> walls(4, 4, true);
        walls.at(1, 1) = false;
        walls.at(2, 2) = false;
        ScentWave w(4, 4, kNoAge);
        w.deposit({1, 1}, kNoAge.strength);
        run(w, walls, 4);
        CHECK(w.sample({2, 2}) == 0);
        CHECK(w.strongest_neighbour({2, 2}, &walls) == std::nullopt);
    }

    TEST_CASE("patch_deposit after update equals deposit then update") {
        // PEO-030's commit patch: the World speculates the update with no deposit
        // and patches the player's in. Random walls, a standing source and moving
        // deposits, 1-3 per update; the two paths must match bit for bit.
        constexpr int kW = 30;
        constexpr int kH = 20;
        constexpr int kUpdates = 300;
        constexpr int kWallOneIn = 6;
        Rng rng(7);
        Grid<bool> walls(kW, kH, false);
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                walls.at(x, y) = rng.range(1, kWallOneIn) == 1;
            }
        }
        const WaveParams p{};
        ScentWave plain(kW, kH, p);
        ScentWave patched(kW, kH, p);
        std::vector<Vec2i> tiles;
        for (int t = 0; t < kUpdates; ++t) {
            tiles.clear();
            const int count = rng.range(1, 3);
            for (int k = 0; k < count; ++k) {
                Vec2i c{};
                do {
                    c = {rng.range(0, kW - 1), rng.range(0, kH - 1)};
                } while (walls.at(c));
                tiles.push_back(c);
            }
            const std::int32_t strength = p.strength - p.age_cost * rng.range(0, 2);
            for (const Vec2i c : tiles) {
                plain.deposit(c, strength);
            }
            plain.update(walls);
            const ScentWave before = patched;
            patched.update(walls);
            for (const Vec2i c : tiles) {
                patched.patch_deposit(before, c, strength, walls);
            }
            if (plain.values() != patched.values()) {
                FAIL("diverged at update " << t);
            }
        }
        CHECK(plain.updates() == patched.updates());
    }
#ifdef NDEBUG
    TEST_CASE("nothing is read past strength / (distance_cost + age_cost / speed) cells") {
        // A standing source long past steady state. Strength set for a 20-cell reach
        // at speed 1 (26 at speed 2), so a small grid holds the whole disc. Release
        // only: ~110 ms under ASan, the suite's largest case (PEO-063 headroom).
        constexpr int kReach = 20;
        constexpr int kBig = 61;
        constexpr Vec2i kMid{30, 30};
        constexpr int kUpdates = 45; // the disc stops growing after ~reach updates
        for (const int speed : {1, 2}) {
            const WaveParams p{.strength = kReach * (kWaveDistanceCost + kWaveAgeCost), .speed = speed};
            const Grid<bool> walls(kBig, kBig, false);
            ScentWave w(kBig, kBig, p);
            for (int i = 0; i < kUpdates; ++i) {
                w.deposit(kMid, p.strength);
                w.update(walls);
            }
            const int reach = p.strength / (p.distance_cost + p.age_cost / p.speed);
            REQUIRE(reach < kMid.x);
            int farthest = 0;
            for (int y = 0; y < kBig; ++y) {
                for (int x = 0; x < kBig; ++x) {
                    if (w.sample({x, y}) > 0) {
                        farthest = std::max(farthest, chebyshev({x, y}, kMid));
                    }
                }
            }
            CAPTURE(speed);
            CHECK(farthest <= reach);
            CHECK(farthest >= reach - 1); // and it does reach about that far
        }
    }
#endif
}
