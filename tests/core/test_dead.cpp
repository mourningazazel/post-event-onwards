#include "peo/core/dead.hpp"

#include <doctest/doctest.h>

#include <cstdint>

using namespace peo::core;

TEST_SUITE("dead") {
    TEST_CASE("the dead move toward scent and respect cooldown") {
        ScentField f(9, 9, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(9, 9, false);
        f.deposit({7, 4}, 1.0F);
        for (int i = 0; i < 6; ++i) {
            f.step();
        }
        // One step per update: after moving it waits out one update, then moves again.
        std::vector<Dead> horde{{.pos = {1, 4}, .cooldown_s = 0, .step_seconds = kUpdatePeriodSeconds}};

        CHECK(step_horde(horde, f, walls, kUpdatePeriodSeconds) == 1);
        CHECK(horde[0].pos == Vec2i{2, 4});
        CHECK(step_horde(horde, f, walls, kUpdatePeriodSeconds) == 0); // cooling down
        CHECK(step_horde(horde, f, walls, kUpdatePeriodSeconds) == 1);
        CHECK(horde[0].pos == Vec2i{3, 4});
    }

    TEST_CASE("the dead go round a wall corner, never through it") {
        // PEO-044: a wall arm down x = 3 from the top edge to (3,2); the unit at
        // (2,2) sits inside the corner, the source round it at (5,3). The diagonal
        // (3,3) is open but passes the wall at (3,2), so the unit must go via (2,3).
        constexpr int kSide = 7;
        constexpr int kArmEnd = 2;
        constexpr int kWallX = 3;
        constexpr int kWarmup = 30;
        constexpr int kUpdates = 20;
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
        const Vec2i start{2, 2};
        REQUIRE(f.sample({3, 3}) > f.sample({2, 3})); // the corner cut would pull harder
        std::vector<Dead> horde{{.pos = start}};
        for (int i = 0; i < kUpdates; ++i) {
            const Vec2i before = horde[0].pos;
            step_horde(horde, f, walls, kUpdatePeriodSeconds);
            const Vec2i d = horde[0].pos - before;
            if (d.x != 0 && d.y != 0) { // a diagonal: both cells beside it are open
                CHECK_FALSE(walls.at(before + Vec2i{d.x, 0}));
                CHECK_FALSE(walls.at(before + Vec2i{0, d.y}));
            }
            if (i == 0) {
                CHECK(horde[0].pos == Vec2i{2, 3});
            }
        }
        CHECK(horde[0].pos.x > kWallX); // it did come round
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
        std::vector<Dead> horde{{.pos = {1, 2}}};
        step_horde(horde, f, walls, kUpdatePeriodSeconds);
        CHECK(horde[0].pos == Vec2i{1, 2});
    }

    // The warm-up only has to lay a gradient the horde can climb: the Dead start
    // 12 cells (Manhattan) from the source, and with no decay and no floor nothing
    // is flushed, so the front arrives well inside this many ticks. Keep it short —
    // this loop, not step_horde, dominated the whole headless suite (PEO-027).
    constexpr int kHordeWarmupTicks = 20;

    TEST_CASE("a thousand dead step without touching each other") {
        ScentField f(64, 64, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(64, 64, false);
        for (int i = 0; i < kHordeWarmupTicks; ++i) { // a player standing still
            f.deposit({32, 32}, 1.0F);
            f.step();
        }
        std::vector<Dead> horde(1000, Dead{.pos = {26, 26}});
        const std::size_t moved = step_horde(horde, f, walls, kUpdatePeriodSeconds);
        CHECK(moved == 1000);
    }

    TEST_CASE("distant Dead close in") {
        // The reach field of 'the scent front carries far' (34x11, source (4,5),
        // 40 warm-up steps), then frozen. Measured: the unit at (19,5) closes all 15
        // cells by call 29. It must start inside the front: on a flat zero field
        // strongest_neighbour is empty and the unit would never move.
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
        std::vector<Dead> horde{{.pos = kStart}};
        for (int i = 0; i < kCalls; ++i) {
            step_horde(horde, f, walls, kUpdatePeriodSeconds);
        }
        CHECK(kStart.x - horde[0].pos.x >= kMinClosed);
    }
}
