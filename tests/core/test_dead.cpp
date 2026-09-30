#include "peo/core/dead.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace peo::core;

namespace {

/// A lone unit on an otherwise empty map: nothing occupies or reserves its tiles.
struct Alone {
    Grid<std::uint8_t> occupied;
    Grid<bool> reserved;
    explicit Alone(const Grid<bool>& walls)
        : occupied(walls.width(), walls.height(), 0), reserved(walls.width(), walls.height(), false) {}

    /// One decision; moves the unit if it decided. Returns whether it moved.
    bool step(Dead& unit, const ScentWave& f, const Grid<bool>& walls) const {
        if (const auto to = decide_move(unit, f, walls, occupied, reserved)) {
            unit.pos = *to;
            return true;
        }
        return false;
    }
};

/// A standing source: `updates` deposits of full strength, each followed by an update.
ScentWave standing(int width, int height, Vec2i source, const Grid<bool>& walls, int updates) {
    ScentWave f(width, height);
    for (int i = 0; i < updates; ++i) {
        f.deposit(source, f.params().strength);
        f.update(walls);
    }
    return f;
}

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

} // namespace

TEST_SUITE("dead") {
    TEST_CASE("the dead move toward scent") {
        // On the geodesic field (PEO-030) a route is 8-connected, so each decision
        // brings the unit one cell nearer the source.
        constexpr Vec2i kSource{7, 4};
        const Grid<bool> walls(9, 9, false);
        const ScentWave f = standing(9, 9, kSource, walls, 6);
        const Alone alone(walls);
        Dead unit{.pos = {1, 4}};
        for (int i = 0; i < 2; ++i) {
            const int before = chebyshev(unit.pos, kSource);
            CHECK(alone.step(unit, f, walls));
            CHECK(chebyshev(unit.pos, kSource) == before - 1);
        }
    }

    TEST_CASE("the dead go round a wall corner, never through it") {
        // PEO-044: a wall arm down x = 3 from the top edge to (3,2); the unit at
        // (2,2) sits inside the corner, the source round it at (5,3). The diagonal
        // (3,3) is open but passes the wall at (3,2), so the unit must go via (2,3).
        constexpr int kSide = 7;
        constexpr int kArmEnd = 2;
        constexpr int kWallX = 3;
        constexpr int kWarmup = 30;
        constexpr int kDecisions = 20;
        constexpr Vec2i kSource{5, 3};
        Grid<bool> walls(kSide, kSide, false);
        for (int y = 0; y <= kArmEnd; ++y) {
            walls.at(kWallX, y) = true;
        }
        const ScentWave f = standing(kSide, kSide, kSource, walls, kWarmup);
        REQUIRE(f.sample({3, 3}) > f.sample({2, 3})); // the corner cut would pull harder
        const Alone alone(walls);
        Dead unit{.pos = {2, 2}};
        for (int i = 0; i < kDecisions; ++i) {
            const Vec2i before = unit.pos;
            alone.step(unit, f, walls);
            const Vec2i d = unit.pos - before;
            if (d.x != 0 && d.y != 0) { // a diagonal: both cells beside it are open
                CHECK_FALSE(walls.at(before + Vec2i{d.x, 0}));
                CHECK_FALSE(walls.at(before + Vec2i{0, d.y}));
            }
            if (i == 0) {
                CHECK(unit.pos == Vec2i{2, 3});
            }
        }
        CHECK(unit.pos.x > kWallX); // it did come round
    }

    TEST_CASE("decide_move takes the best tile or stays put") {
        // D-031: the strongest neighbour, (4,3), holds more than the weaker free one,
        // (2,3). Taken or reserved, the unit stays; it never falls back to (2,3).
        constexpr int kSide = 7;
        constexpr Vec2i kFrom{3, 3};
        constexpr Vec2i kBest{4, 3};
        constexpr Vec2i kWeaker{2, 3};
        constexpr std::int32_t kStrong = 20;
        constexpr std::int32_t kWeak = 10;
        ScentWave f(kSide, kSide);
        f.deposit(kBest, kStrong);
        f.deposit(kWeaker, kWeak);
        const Grid<bool> walls(kSide, kSide, false);
        Grid<std::uint8_t> occupied(kSide, kSide, 0);
        Grid<bool> reserved(kSide, kSide, false);
        const Dead unit{.pos = kFrom};

        CHECK(decide_move(unit, f, walls, occupied, reserved) == kBest);
        occupied.at(kBest) = 1;
        CHECK_FALSE(decide_move(unit, f, walls, occupied, reserved).has_value());
        occupied.at(kBest) = 0;
        reserved.at(kBest) = true;
        CHECK_FALSE(decide_move(unit, f, walls, occupied, reserved).has_value());
    }

    TEST_CASE("the dead never enter walls") {
        ScentWave f(5, 5);
        Grid<bool> walls(5, 5, false);
        walls.at(2, 2) = true;
        f.deposit({2, 2}, f.params().strength); // scent inside a wall cell: nothing should walk in
        const Alone alone(walls);
        Dead unit{.pos = {1, 2}};
        CHECK_FALSE(alone.step(unit, f, walls));
        CHECK(unit.pos == Vec2i{1, 2});
    }

    // The warm-up only has to lay a gradient the horde can climb: the Dead start
    // within 12 cells of the source, and the front moves a cell per update, so it
    // arrives well inside this many updates.
    constexpr int kHordeWarmupTicks = 20;

    TEST_CASE("a thousand dead decide without touching each other") {
        // Scale, and D-031's rule at scale: 1000 of the Dead on distinct tiles all
        // decide in one second; no decision lands on another's tile or on a tile
        // someone already reserved.
        constexpr int kSide = 64;
        constexpr int kBlock = 25; // a 25x40 block of the Dead just east of the source
        constexpr int kRows = 40;
        constexpr Vec2i kSource{32, 32};
        constexpr Vec2i kCorner{36, 12};
        const Grid<bool> walls(kSide, kSide, false);
        const ScentWave f = standing(kSide, kSide, kSource, walls, kHordeWarmupTicks);
        std::vector<Dead> horde;
        Grid<std::uint8_t> occupied(kSide, kSide, 0);
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kBlock; ++x) {
                horde.push_back({.pos = kCorner + Vec2i{x, y}});
                occupied.at(horde.back().pos) = 1;
            }
        }
        REQUIRE(horde.size() == 1000);
        Grid<bool> reserved(kSide, kSide, false);
        std::size_t decided = 0;
        for (const Dead& unit : horde) {
            if (const auto to = decide_move(unit, f, walls, occupied, reserved)) {
                CHECK(occupied.at(*to) == 0);
                CHECK_FALSE(reserved.at(*to));
                reserved.at(*to) = true;
                ++decided;
            }
        }
        // The column facing the source has free tiles ahead; the packed rest cannot move.
        CHECK(decided > 0);
        CHECK(decided < horde.size());
    }

    TEST_CASE("distant Dead close in") {
        // A standing source at (4,5) on a 34x11 map, 40 updates, then frozen: the
        // unit at (19,5) is inside the reach and each call is one decision, so it
        // closes at least 10 of the 15 cells in 50 calls.
        constexpr Vec2i kSource{4, 5};
        constexpr Vec2i kStart{19, 5};
        constexpr int kWarmupSteps = 40;
        constexpr int kCalls = 50;
        constexpr int kMinClosed = 10;
        const Grid<bool> walls(34, 11, false);
        const ScentWave f = standing(34, 11, kSource, walls, kWarmupSteps);
        const Alone alone(walls);
        Dead unit{.pos = kStart};
        for (int i = 0; i < kCalls; ++i) {
            alone.step(unit, f, walls);
        }
        CHECK(kStart.x - unit.pos.x >= kMinClosed);
    }
}
