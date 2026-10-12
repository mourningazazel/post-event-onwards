// PEO-120: the Look cursor and the look report (peo/core/look.hpp).
#include "peo/core/look.hpp"

#include <doctest/doctest.h>

#include <span>
#include <vector>

using namespace peo::core;

namespace {

constexpr Vec2i kEast{1, 0};
constexpr Vec2i kWest{-1, 0};

/// An open `size` x `size` stage with no exit, built by hand.
Stage open_stage(int size) {
    Stage stage;
    stage.blocked = Grid<bool>(size, size, false);
    stage.exit = kNoExit;
    return stage;
}

/// Sight from `origin` over `stage` at `range`.
SightField sight_from(const Stage& stage, Vec2i origin, int range) {
    SightField sight;
    sight.compute(stage.blocked, origin, {range});
    return sight;
}

} // namespace

TEST_SUITE("look") {
    TEST_CASE("begin puts the cursor on the player; after end it never moves") {
        const Stage stage = open_stage(16);
        const Vec2i player{8, 8};
        const SightField sight = sight_from(stage, player, 5);
        LookCursor look;
        CHECK_FALSE(look.active());
        look.begin(player);
        CHECK(look.active());
        CHECK(look.cell() == player);
        look.end();
        CHECK_FALSE(look.active());
        CHECK_FALSE(look.step(kEast, 1, sight, sight.window()));
        CHECK(look.cell() == player);
    }

    TEST_CASE("a step onto a seen open cell moves one cell") {
        const Stage stage = open_stage(16);
        const Vec2i player{8, 8};
        const SightField sight = sight_from(stage, player, 5);
        LookCursor look;
        look.begin(player);
        CHECK(look.step(kEast, 1, sight, sight.window()));
        CHECK(look.cell() == Vec2i{9, 8});
    }

    TEST_CASE("the cursor rests on a seen wall but not on the unseen cell behind it") {
        Stage stage = open_stage(16);
        stage.blocked.at(9, 8) = true;
        const Vec2i player{8, 8};
        const SightField sight = sight_from(stage, player, 5);
        REQUIRE(sight.seen({9, 8}));
        REQUIRE_FALSE(sight.seen({10, 8}));
        LookCursor look;
        look.begin(player);
        CHECK(look.step(kEast, 1, sight, sight.window()));
        CHECK(look.cell() == Vec2i{9, 8});
        CHECK_FALSE(look.step(kEast, 1, sight, sight.window()));
        CHECK(look.cell() == Vec2i{9, 8});
        CHECK_FALSE(look.step(kEast, kLookJumpCells, sight, sight.window()));
        CHECK(look.cell() == Vec2i{9, 8});
    }

    TEST_CASE("a jump stops on the last seen cell, and on the last cell inside smaller bounds") {
        const Stage stage = open_stage(32);
        const Vec2i player{10, 10};
        const int range = 5;
        const SightField sight = sight_from(stage, player, range);
        LookCursor look;
        look.begin(player);
        CHECK(look.step(kEast, kLookJumpCells, sight, sight.window()));
        CHECK(look.cell() == Vec2i{player.x + range, player.y});

        look.begin(player);
        const CellRect bounds{{8, 8}, {12, 12}};
        CHECK(look.step(kEast, kLookJumpCells, sight, bounds));
        CHECK(look.cell() == Vec2i{12, 10});
        CHECK_FALSE(look.step(kEast, 1, sight, bounds));
    }

    TEST_CASE("a step off the grid does not move") {
        const Stage stage = open_stage(9);
        const Vec2i player{0, 4};
        const SightField sight = sight_from(stage, player, 5);
        LookCursor look;
        look.begin(player);
        CHECK_FALSE(look.step(kWest, 1, sight, sight.window()));
        CHECK_FALSE(look.step(kWest, 1, sight, {{-5, -5}, {20, 20}})); // wider bounds: still off grid
        CHECK(look.cell() == player);
    }

    TEST_CASE("a diagonal step moves both axes") {
        const Stage stage = open_stage(16);
        const Vec2i player{8, 8};
        const SightField sight = sight_from(stage, player, 5);
        LookCursor look;
        look.begin(player);
        CHECK(look.step({1, 1}, 1, sight, sight.window()));
        CHECK(look.cell() == Vec2i{9, 9});
        CHECK(look.step({-1, -1}, 2, sight, sight.window()));
        CHECK(look.cell() == Vec2i{7, 7});
    }

    TEST_CASE("look_at reports only seen cells: the player, a wall, the exit and the Dead") {
        Stage stage = open_stage(16);
        stage.blocked.at(12, 8) = true;
        stage.exit = {4, 8};
        const Vec2i player{8, 8};
        const SightField sight = sight_from(stage, player, 6);
        const std::vector<Dead> horde{{{8, 12}}, {{8, 12}}, {{9, 11}}};

        CHECK_FALSE(look_at(sight, stage, horde, player, {13, 8}).has_value());  // behind the wall
        CHECK_FALSE(look_at(sight, stage, horde, player, {-1, 0}).has_value());  // off the grid
        CHECK_FALSE(look_at(sight, stage, horde, player, {15, 15}).has_value()); // out of range

        const auto self = look_at(sight, stage, horde, player, player);
        REQUIRE(self.has_value());
        CHECK(self->cell == player);
        CHECK(self->player);
        CHECK(self->distance_sq == 0);
        CHECK_FALSE(self->opaque);
        CHECK_FALSE(self->exit);
        CHECK(self->dead == 0);

        const auto wall = look_at(sight, stage, horde, player, {12, 8});
        REQUIRE(wall.has_value());
        CHECK(wall->opaque);
        CHECK_FALSE(wall->player);
        CHECK(wall->distance_sq == 16);

        const auto exit = look_at(sight, stage, horde, player, {4, 8});
        REQUIRE(exit.has_value());
        CHECK(exit->exit);
        CHECK_FALSE(exit->opaque);
        CHECK(exit->distance_sq == 16);

        const auto crowd = look_at(sight, stage, horde, player, {8, 12});
        REQUIRE(crowd.has_value());
        CHECK(crowd->dead == 2);
        CHECK(crowd->distance_sq == 16);

        const auto one = look_at(sight, stage, horde, player, {9, 11});
        REQUIRE(one.has_value());
        CHECK(one->dead == 1);
        CHECK(one->distance_sq == 1 * 1 + 3 * 3);

        const auto empty = look_at(sight, stage, std::span<const Dead>{}, player, {8, 12});
        REQUIRE(empty.has_value());
        CHECK(empty->dead == 0);
    }

    TEST_CASE("look_at marks cells within 6 cells legible, by the sight disc test") {
        const Stage stage = open_stage(24);
        const Vec2i player{12, 12};
        // Sight reaches well past the legible range, so only the 20 ft rule decides.
        const SightField sight = sight_from(stage, player, 10);

        const auto legible_at = [&](Vec2i offset) {
            const auto report = look_at(sight, stage, std::span<const Dead>{}, player, player + offset);
            REQUIRE(report.has_value());
            return report->legible;
        };

        CHECK(legible_at({0, 0}));
        CHECK(legible_at({6, 0}));
        CHECK(legible_at({0, 6}));
        CHECK(legible_at({4, 4})); // distance_sq 32
        CHECK_FALSE(legible_at({7, 0}));
        CHECK_FALSE(legible_at({5, 5})); // distance_sq 50
        CHECK_FALSE(legible_at({6, 1})); // distance_sq 37
    }
}
