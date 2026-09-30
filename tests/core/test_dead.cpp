#include "peo/core/dead.hpp"

#include <doctest/doctest.h>

#include <cstdint>
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
    bool step(Dead& unit, const ScentField& f, const Grid<bool>& walls) const {
        if (const auto to = decide_move(unit, f, walls, occupied, reserved)) {
            unit.pos = *to;
            return true;
        }
        return false;
    }
};

} // namespace

TEST_SUITE("dead") {
    TEST_CASE("the dead move toward scent") {
        ScentField f(9, 9, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(9, 9, false);
        f.deposit({7, 4}, 1.0F);
        for (int i = 0; i < 6; ++i) {
            f.step();
        }
        const Alone alone(walls);
        Dead unit{.pos = {1, 4}};
        CHECK(alone.step(unit, f, walls));
        CHECK(unit.pos == Vec2i{2, 4});
        CHECK(alone.step(unit, f, walls));
        CHECK(unit.pos == Vec2i{3, 4});
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
        ScentField f(kSide, kSide, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(kSide, kSide, false);
        for (int y = 0; y <= kArmEnd; ++y) {
            walls.at(kWallX, y) = true;
        }
        for (int i = 0; i < kWarmup; ++i) {
            f.deposit(kSource, 1.0F);
            f.step(&walls);
        }
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
        ScentField f(kSide, kSide);
        f.deposit(kBest, 2.0F);
        f.deposit(kWeaker, 1.0F);
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
        ScentField f(5, 5, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(5, 5, false);
        walls.at(2, 2) = true;
        f.deposit({2, 2}, 1.0F); // scent inside a wall cell: nothing should walk in
        const Alone alone(walls);
        Dead unit{.pos = {1, 2}};
        CHECK_FALSE(alone.step(unit, f, walls));
        CHECK(unit.pos == Vec2i{1, 2});
    }

    // The warm-up only has to lay a gradient the horde can climb: the Dead start
    // within 12 cells (Manhattan) of the source, and with no decay and no floor
    // nothing is flushed, so the front arrives well inside this many ticks. Keep it
    // short: this loop, not the Dead, dominated the whole headless suite (PEO-027).
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
        ScentField f(kSide, kSide, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(kSide, kSide, false);
        for (int i = 0; i < kHordeWarmupTicks; ++i) { // a player standing still
            f.deposit(kSource, 1.0F);
            f.step();
        }
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
        // The reach field of 'the scent front carries far' (34x11, source (4,5),
        // 40 warm-up steps), then frozen. Each call is one decision for a lone unit
        // (D-031); at least 10 of the 15 cells must close in 50. It must start inside
        // the front: on a flat zero field strongest_neighbour is empty and the unit
        // would never move.
        constexpr Vec2i kSource{4, 5};
        constexpr Vec2i kStart{19, 5};
        constexpr int kWarmupSteps = 40;
        constexpr int kCalls = 50;
        constexpr int kMinClosed = 10;
        ScentField f(34, 11);
        Grid<bool> walls(34, 11, false);
        for (int i = 0; i < kWarmupSteps; ++i) {
            f.deposit(kSource, kPlayerScent);
            f.step();
        }
        const Alone alone(walls);
        Dead unit{.pos = kStart};
        for (int i = 0; i < kCalls; ++i) {
            alone.step(unit, f, walls);
        }
        CHECK(kStart.x - unit.pos.x >= kMinClosed);
    }
}
