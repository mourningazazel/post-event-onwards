#include "peo/core/dead.hpp"

#include <doctest/doctest.h>

using namespace peo::core;

TEST_SUITE("dead") {
    TEST_CASE("the dead move toward scent and respect cooldown") {
        ScentField f(9, 9, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(9, 9, false);
        f.deposit({7, 4}, 1.0F);
        for (int i = 0; i < 6; ++i) {
            f.step();
        }
        std::vector<Dead> horde{{.pos = {1, 4}, .cooldown = 0, .speed = 1}};

        CHECK(step_horde(horde, f, walls) == 1);
        CHECK(horde[0].pos == Vec2i{2, 4});
        CHECK(step_horde(horde, f, walls) == 0); // cooling down
        CHECK(step_horde(horde, f, walls) == 1);
        CHECK(horde[0].pos == Vec2i{3, 4});
    }

    TEST_CASE("the dead never enter walls") {
        ScentField f(5, 5, {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F});
        Grid<bool> walls(5, 5, false);
        walls.at(2, 2) = true;
        f.deposit({2, 2}, 1.0F); // scent inside a wall cell: nothing should walk in
        std::vector<Dead> horde{{.pos = {1, 2}}};
        step_horde(horde, f, walls);
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
        const std::size_t moved = step_horde(horde, f, walls);
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
            step_horde(horde, f, walls);
        }
        CHECK(kStart.x - horde[0].pos.x >= kMinClosed);
    }
}
