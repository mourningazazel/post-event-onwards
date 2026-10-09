// PEO-119: the player's sight (peo/core/sight.hpp).
#include "peo/core/rng.hpp"
#include "peo/core/sight.hpp"
#include "peo/core/stage.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using namespace peo::core;

namespace {

/// Is `cell` within the disc of `range` round `origin`?
bool in_disc(Vec2i origin, Vec2i cell, int range) {
    const int dx = cell.x - origin.x;
    const int dy = cell.y - origin.y;
    return dx * dx + dy * dy <= range * range;
}

/// On an open grid, sight is exactly the disc (clipped to the grid).
bool sees_exactly_the_disc(const SightField& sight, const Grid<bool>& grid, Vec2i origin, int range) {
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            if (sight.seen({x, y}) != in_disc(origin, {x, y}, range)) {
                return false;
            }
        }
    }
    return true;
}

/// The open cells of `grid`, in row order.
std::vector<Vec2i> open_cells(const Grid<bool>& grid) {
    std::vector<Vec2i> out;
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            if (grid.at(x, y) == 0) {
                out.push_back({x, y});
            }
        }
    }
    return out;
}

} // namespace

TEST_SUITE("sight") {
    TEST_CASE("on an open grid sight is the disc of the range and nothing else") {
        const Grid<bool> open(21, 21, false);
        const Vec2i origin{10, 10};
        SightField sight;
        CHECK_FALSE(sight.seen(origin)); // nothing before the first compute
        sight.compute(open, origin, {5});
        CHECK(sees_exactly_the_disc(sight, open, origin, 5));
        CHECK_FALSE(sight.seen({-1, -1}));
        CHECK_FALSE(sight.seen({100, 100}));
        CHECK(sight.origin() == origin);
        CHECK(sight.range() == 5);
    }

    TEST_CASE("a range of 0 or below sees only the origin") {
        const Grid<bool> open(9, 9, false);
        SightField sight;
        for (const int range : {0, -3}) {
            sight.compute(open, {4, 4}, {range});
            CHECK(sees_exactly_the_disc(sight, open, {4, 4}, 0));
            CHECK(sight.range() == 0);
        }
    }

    TEST_CASE("a wall is seen and shadows the cell behind it, not the cells beside the shadow") {
        Grid<bool> grid(21, 21, false);
        grid.at(12, 10) = true;
        SightField sight;
        sight.compute(grid, {10, 10}, {8});
        CHECK(sight.seen({12, 10}));
        CHECK_FALSE(sight.seen({13, 10}));
        CHECK_FALSE(sight.seen({16, 10}));
        CHECK(sight.seen({13, 9}));
        CHECK(sight.seen({13, 11}));
    }

    TEST_CASE("from inside a closed room its walls and floor are seen and nothing outside") {
        Grid<bool> grid(31, 31, false);
        for (int i = 13; i <= 17; ++i) {
            grid.at(i, 13) = grid.at(i, 17) = grid.at(13, i) = grid.at(17, i) = true;
        }
        SightField sight;
        sight.compute(grid, {15, 15}, {20});
        for (int y = 0; y < 31; ++y) {
            for (int x = 0; x < 31; ++x) {
                const bool in_room = x >= 13 && x <= 17 && y >= 13 && y <= 17;
                CHECK_MESSAGE(sight.seen({x, y}) == in_room, "cell " << x << "," << y);
            }
        }
    }

    TEST_CASE("round the corner of a one-wide corridor is not seen from its far end") {
        Grid<bool> grid(13, 13, true);
        for (int x = 2; x <= 10; ++x) {
            grid.at(x, 2) = false;
        }
        for (int y = 2; y <= 10; ++y) {
            grid.at(10, y) = false;
        }
        SightField sight;
        sight.compute(grid, {2, 2}, {20});
        CHECK(sight.seen({10, 2}));
        for (int y = 3; y <= 10; ++y) {
            CHECK_MESSAGE(!sight.seen({10, y}), "corridor cell 10," << y);
        }
    }

    TEST_CASE("between open cells sight is symmetric") {
        StageSpec spec;
        spec.world_seed = 119;
        spec.width = 64;
        spec.height = 36;
        spec.wall_density = 0.2F;
        const Stage stage = generate_stage(spec);
        const std::vector<Vec2i> open = open_cells(stage.blocked);
        REQUIRE(open.size() > 8);
        constexpr int kOrigins = 8;
        constexpr int kRange = 12;
        SightField from_a;
        SightField from_b;
        int pairs = 0;
        for (int i = 0; i < kOrigins; ++i) {
            const Vec2i a = open[open.size() * static_cast<std::size_t>(i) / kOrigins];
            from_a.compute(stage.blocked, a, {kRange});
            for (const Vec2i b : open) {
                if (b == a || !from_a.seen(b)) {
                    continue;
                }
                from_b.compute(stage.blocked, b, {kRange});
                ++pairs;
                if (!from_b.seen(a)) {
                    FAIL_CHECK("a=" << a.x << "," << a.y << " sees b=" << b.x << "," << b.y
                                    << " but not back");
                }
            }
        }
        CHECK(pairs > kOrigins); // the stage is open enough to test something
    }

    TEST_CASE("a reused field equals a fresh one, and its window is clipped to the grid") {
        StageSpec spec;
        spec.world_seed = 7;
        spec.width = 48;
        spec.height = 32;
        spec.wall_density = 0.2F;
        Stage stage = generate_stage(spec);
        stage.blocked.at(1, 1) = false; // an origin one cell from a corner
        const Vec2i a{24, 16};
        stage.blocked.at(a) = false;
        constexpr int kRange = 9;
        for (const Vec2i b : {Vec2i{1, 1}, Vec2i{30, 20}}) {
            stage.blocked.at(b) = false;
            SightField reused;
            reused.compute(stage.blocked, a, {kRange});
            reused.compute(stage.blocked, b, {kRange});
            SightField fresh;
            fresh.compute(stage.blocked, b, {kRange});
            bool same = true;
            for (int y = 0; y < spec.height; ++y) {
                for (int x = 0; x < spec.width; ++x) {
                    same = same && reused.seen({x, y}) == fresh.seen({x, y});
                }
            }
            CHECK(same);
            const CellRect expect{
                {std::max(b.x - kRange, 0), std::max(b.y - kRange, 0)},
                {std::min(b.x + kRange, spec.width - 1), std::min(b.y + kRange, spec.height - 1)}};
            CHECK(reused.window() == expect);
        }
    }

    TEST_CASE("the range is a parameter: a longer one grows the field, a shorter one sees less again") {
        const Grid<bool> open(64, 64, false);
        const Vec2i origin{32, 32};
        SightField sight;
        sight.compute(open, origin, {10});
        CHECK(sees_exactly_the_disc(sight, open, origin, 10));
        sight.compute(open, origin, {20});
        CHECK(sees_exactly_the_disc(sight, open, origin, 20));
        sight.compute(open, origin, {10});
        CHECK(sees_exactly_the_disc(sight, open, origin, 10));
    }

    TEST_CASE("the optical range: 152 cells on an open stage is the whole disc" *
              doctest::test_suite("scenario: sight")) {
        constexpr int kSide = 401;
        const Grid<bool> open(kSide, kSide, false);
        const Vec2i origin{kSide / 2, kSide / 2};
        SightField sight;
        sight.compute(open, origin);
        CHECK(sight.range() == kOpticalRangeCells);
        int expect = 0;
        int got = 0;
        for (int y = 0; y < kSide; ++y) {
            for (int x = 0; x < kSide; ++x) {
                expect += in_disc(origin, {x, y}, kOpticalRangeCells) ? 1 : 0;
                got += sight.seen({x, y}) ? 1 : 0;
            }
        }
        CHECK(got == expect);
        for (const Vec2i dir : {Vec2i{1, 0}, Vec2i{-1, 0}, Vec2i{0, 1}, Vec2i{0, -1}}) {
            CHECK(sight.seen({origin.x + dir.x * kOpticalRangeCells, origin.y + dir.y * kOpticalRangeCells}));
            CHECK_FALSE(sight.seen(
                {origin.x + dir.x * (kOpticalRangeCells + 1), origin.y + dir.y * (kOpticalRangeCells + 1)}));
        }
    }

    TEST_CASE("perf: sight, range 152" * doctest::test_suite("scenario: perf")) {
        constexpr int kSide = 512;
        constexpr int kSamples = 50;
        constexpr Seed kSeed = 119;
        using Clock = std::chrono::steady_clock;
        StageSpec spec = stage_spec(kSeed, 0);
        spec.width = kSide;
        spec.height = kSide;
        const Stage stage = generate_stage(spec);
        const Grid<bool> open(kSide, kSide, false);
        for (const bool generated : {false, true}) {
            const Grid<bool>& grid = generated ? stage.blocked : open;
            const std::vector<Vec2i> cells = open_cells(grid);
            Rng rng(kSeed);
            SightField sight;
            std::vector<double> us;
            std::size_t seen = 0;
            for (int i = 0; i < kSamples; ++i) {
                const Vec2i origin =
                    cells[static_cast<std::size_t>(rng.range(0, static_cast<int>(cells.size()) - 1))];
                const auto t0 = Clock::now();
                sight.compute(grid, origin);
                us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
                seen += sight.seen(origin) ? 1U : 0U;
            }
            std::sort(us.begin(), us.end());
            MESSAGE("perf sight " << kSide << "x" << kSide << std::string(generated ? " stage" : " open")
                                  << " range=" << kOpticalRangeCells
                                  << " compute_median=" << us[us.size() / 2] << " us");
            CHECK(seen == kSamples);
        }
    }
}
