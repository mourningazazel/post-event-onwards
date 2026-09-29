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
}
