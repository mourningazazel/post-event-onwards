// PEO-124: what a character remembers seeing of the stage (peo/core/memory.hpp).
#include "peo/core/memory.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/sight.hpp"
#include "peo/core/stage.hpp"
#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

using namespace peo::core;

namespace {

constexpr Seed kSeed = 124;

/// Does `memory` remember exactly what `sight` sees, on every cell of a `w` x `h` grid?
bool remembers_exactly(const MapMemory& memory, const SightField& sight, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (memory.remembered({x, y}) != sight.seen({x, y})) {
                return false;
            }
        }
    }
    return true;
}

std::size_t seen_count(const SightField& sight, int w, int h) {
    std::size_t n = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            n += sight.seen({x, y}) ? 1U : 0U;
        }
    }
    return n;
}

WorldParams small_params() {
    return {.initial_dead = 20, .stage_width = 32, .stage_height = 20};
}

} // namespace

TEST_SUITE("memory") {
    TEST_CASE("a new or reset memory remembers nothing, on or off the grid") {
        const MapMemory none;
        CHECK_FALSE(none.remembered({0, 0}));
        CHECK_FALSE(none.remembered({-1, -1}));
        CHECK(none.count() == 0);
        MapMemory memory;
        memory.reset(10, 8);
        CHECK(memory.width() == 10);
        CHECK(memory.height() == 8);
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 10; ++x) {
                CHECK_FALSE(memory.remembered({x, y}));
            }
        }
        CHECK_FALSE(memory.remembered({-1, -1}));
        CHECK_FALSE(memory.remembered({10, 8}));
        CHECK(memory.count() == 0);
        memory.mark({10, 8}); // off the grid: ignored
        CHECK(memory.count() == 0);
    }

    TEST_CASE("one fold remembers exactly what sight sees") {
        Grid<bool> grid(21, 21, false);
        grid.at(12, 10) = true;
        SightField sight;
        sight.compute(grid, {10, 10}, {5});
        MapMemory memory;
        memory.reset(21, 21);
        const std::size_t added = memory.remember(sight);
        CHECK(remembers_exactly(memory, sight, 21, 21));
        CHECK(added == memory.count());
        CHECK(added == seen_count(sight, 21, 21));
    }

    TEST_CASE("folds add up: a union, counting only the new cells") {
        Grid<bool> grid(21, 21, false);
        grid.at(12, 10) = true;
        SightField first;
        first.compute(grid, {6, 10}, {5});
        SightField second;
        second.compute(grid, {14, 10}, {5});
        MapMemory memory;
        memory.reset(21, 21);
        const std::size_t a = memory.remember(first);
        const std::size_t b = memory.remember(second);
        std::size_t both = 0;
        for (int y = 0; y < 21; ++y) {
            for (int x = 0; x < 21; ++x) {
                const bool either = first.seen({x, y}) || second.seen({x, y});
                CHECK(memory.remembered({x, y}) == either);
                both += first.seen({x, y}) && second.seen({x, y}) ? 1U : 0U;
            }
        }
        CHECK(b == seen_count(second, 21, 21) - both);
        CHECK(memory.count() == a + b);
        CHECK(memory.remember(second) == 0);
    }

    TEST_CASE("folds allocate nothing; a same-size reset keeps the buffer and clears it") {
        const Grid<bool> grid(32, 32, false);
        MapMemory memory;
        memory.reset(32, 32);
        const auto* data = memory.cells().data();
        SightField sight;
        for (int i = 0; i < 5; ++i) {
            sight.compute(grid, {4 + 5 * i, 16}, {6});
            (void)memory.remember(sight);
        }
        CHECK(memory.cells().data() == data);
        REQUIRE(memory.count() > 0);
        memory.reset(32, 32);
        CHECK(memory.cells().data() == data);
        CHECK(memory.count() == 0);
        CHECK(std::none_of(memory.cells().begin(), memory.cells().end(),
                           [](std::uint8_t c) { return c != 0; }));
    }

    TEST_CASE("sight over a larger grid folds only the cells inside the memory") {
        const Grid<bool> big(64, 64, false);
        SightField sight;
        sight.compute(big, {20, 20}, {12});
        MapMemory memory;
        memory.reset(24, 24);
        (void)memory.remember(sight);
        CHECK(remembers_exactly(memory, sight, 24, 24));
        CHECK_FALSE(memory.remembered({30, 20}));
    }

    TEST_CASE("World: every load starts the memory empty, and only fresh sight folds") {
        World world(kSeed, small_params());
        const Stage& stage = world.stage();
        CHECK(world.memory().width() == stage.spec.width);
        CHECK(world.memory().height() == stage.spec.height);
        CHECK(world.memory().count() == 0);

        SightField sight;
        sight.compute(stage.blocked, world.player(), {6});
        CHECK(world.remember(sight) > 0);
        CHECK(world.memory().remembered(world.player()));

        // A field from a cell beside the player is stale: nothing folds.
        Vec2i beside = world.player();
        for (const Vec2i d : kNeighbours8) {
            if (!stage.blocked.at(world.player() + d)) {
                beside = world.player() + d;
                break;
            }
        }
        REQUIRE(beside != world.player());
        const MapMemory before = world.memory();
        SightField stale;
        stale.compute(stage.blocked, beside, {6});
        CHECK(world.remember(stale) == 0);
        CHECK(world.memory() == before);

        world.load_stage(world.stage_index() + 1);
        CHECK(world.memory().count() == 0);
        CHECK(world.memory().width() == world.stage().spec.width);

        sight.compute(world.stage().blocked, world.player(), {6});
        REQUIRE(world.remember(sight) > 0);
        Stage layout;
        layout.blocked = Grid<bool>(12, 9, false);
        layout.exit = kNoExit;
        world.load_layout(std::move(layout), {3, 3}, {});
        CHECK(world.memory().count() == 0);
        CHECK(world.memory().width() == 12);
        CHECK(world.memory().height() == 9);
    }

    TEST_CASE("World: step and commit(speculate()) leave the memory as it was") {
        World a(kSeed, small_params());
        SightField sight;
        sight.compute(a.stage().blocked, a.player(), {6});
        REQUIRE(a.remember(sight) > 0);
        const MapMemory remembered = a.memory();
        World b = a;
        constexpr int kActions = 4; // a wait and steps; the unit tier has little headroom
        Rng pick(kSeed);
        for (int i = 0; i < kActions; ++i) {
            const Action action = i % 3 == 0 ? Action::wait() : Action::step(kNeighbours4[pick.range(0, 3)]);
            Speculation spec = b.speculate();
            a.step(action);
            b.commit(spec, action);
            REQUIRE(World::equivalent(a, b));
        }
        CHECK(a.memory() == remembered);
        CHECK(b.memory() == remembered);
    }

    TEST_CASE("perf: memory fold, range 152" * doctest::test_suite("scenario: perf")) {
        constexpr int kSide = 512;
        constexpr int kSamples = 50;
        using Clock = std::chrono::steady_clock;
        const Grid<bool> open(kSide, kSide, false);
        Rng rng(kSeed);
        SightField sight;
        MapMemory memory;
        std::vector<double> us;
        std::size_t added = 0;
        for (int i = 0; i < kSamples; ++i) {
            const Vec2i origin{rng.range(0, kSide - 1), rng.range(0, kSide - 1)};
            sight.compute(open, origin);
            memory.reset(kSide, kSide); // every fold a first sight: the most cells written
            const auto t0 = Clock::now();
            added += memory.remember(sight);
            us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
        }
        std::sort(us.begin(), us.end());
        MESSAGE("perf memory " << kSide << "x" << kSide << " open range=" << kOpticalRangeCells
                               << " fold_median=" << us[us.size() / 2] << " us");
        CHECK(added > 0);
    }
}
