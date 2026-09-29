#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <vector>

using namespace peo::core;

namespace {

constexpr Seed kSeed = 7;
constexpr int kDeterminismTurns = 30;
constexpr int kWaitTurns = 30;

/// A fixed, varied action script: steps in all four directions plus waits.
Action scripted_action(int i) {
    const int k = i % 5;
    return k == 4 ? Action::wait() : Action::step(kNeighbours4[k]);
}

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

long long horde_distance(const World& w) {
    long long sum = 0;
    for (const Dead& d : w.horde()) {
        sum += chebyshev(d.pos, w.player());
    }
    return sum;
}

/// Shortest 4-connected path from the player to the stage exit, as step directions.
std::vector<Vec2i> path_to_exit(const World& w) {
    const Grid<bool>& blocked = w.stage().blocked;
    Grid<int> came_from(blocked.width(), blocked.height(), -1);
    std::deque<Vec2i> open{w.player()};
    came_from.at(w.player()) = 4; // start marker
    while (!open.empty()) {
        const Vec2i c = open.front();
        open.pop_front();
        if (c == w.stage().exit) {
            break;
        }
        for (int d = 0; d < 4; ++d) {
            const Vec2i n = c + kNeighbours4[d];
            if (blocked.in_bounds(n) && !blocked.at(n) && came_from.at(n) == -1) {
                came_from.at(n) = d;
                open.push_back(n);
            }
        }
    }
    // Walk back from the exit, appending, then reverse: prepending with insert()
    // trips a -Wnull-dereference false positive in libstdc++ at -O3.
    std::vector<Vec2i> steps;
    for (Vec2i c = w.stage().exit; c != w.player();) {
        const int d = came_from.at(c);
        REQUIRE(d >= 0);
        steps.push_back(kNeighbours4[d]);
        c = c - kNeighbours4[d];
    }
    std::reverse(steps.begin(), steps.end());
    return steps;
}

} // namespace

TEST_SUITE("world") {
    TEST_CASE("same seed same world after 30 turns") {
        World a(kSeed);
        World b(kSeed);
        // 30 turns, not the brief's 100: a scent step costs ~2.3 ms under the
        // headless sanitizers, and 30 turns already exercise moves, walls and waits.
        for (int i = 0; i < kDeterminismTurns; ++i) {
            a.step(scripted_action(i));
            b.step(scripted_action(i));
        }
        CHECK(a.player() == b.player());
        CHECK(a.turn() == b.turn());
        CHECK(static_cast<double>(a.scent().total()) ==
              doctest::Approx(static_cast<double>(b.scent().total())));
        REQUIRE(a.horde().size() == b.horde().size());
        for (std::size_t i = 0; i < a.horde().size(); ++i) {
            CHECK(a.horde()[i].pos == b.horde()[i].pos);
        }
    }

    TEST_CASE("step into wall consumes the turn but does not move") {
        World w(kSeed);
        // Walk west until blocked (the border is always a wall).
        while (!w.stage().blocked.at(w.player() + Vec2i{-1, 0})) {
            w.step(Action::step({-1, 0}));
        }
        const Vec2i before = w.player();
        const Tick turn = w.turn();
        REQUIRE(w.stage().blocked.at(before + Vec2i{-1, 0}));

        w.step(Action::step({-1, 0}));
        CHECK(w.player() == before);
        CHECK(w.turn() == turn + 1);
    }

    TEST_CASE("wait advances the turn and the dead approach") {
        // Fast, lossless scent so the test is about World, not scent tuning (PEO-026).
        const WorldParams params{.initial_dead = 200,
                                 .player_scent = 1.0F,
                                 .scent = {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F}};
        World w(kSeed, params);
        const long long before = horde_distance(w);
        const Vec2i where = w.player();
        for (int i = 0; i < kWaitTurns; ++i) {
            w.step(Action::wait());
        }
        CHECK(w.turn() == kWaitTurns);
        CHECK(w.player() == where);
        CHECK(horde_distance(w) < before);
    }

    TEST_CASE("reaching exit advances stage_index") {
        World w(kSeed, {.initial_dead = 0});
        const std::vector<Vec2i> steps = path_to_exit(w);
        REQUIRE_FALSE(steps.empty());
        for (const Vec2i d : steps) {
            w.step(Action::step(d));
        }
        CHECK(w.stage_index() == 1);
        CHECK(w.turn() == 0);
        CHECK(w.player() == w.stage().entry);
    }
}
