#include "peo/core/scent.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <vector>

#include "town_fixture.hpp"

using namespace peo::core;
using namespace peo::test;

namespace {

/// The review's town seed (docs/design/probes/probe.cpp, Rng(11)).
constexpr Seed kTownSeed = 11;
/// A cell on the top street: row 1 is open all the way across (the border is row 0).
constexpr Vec2i kTopStreet{1, 1};
constexpr int kMinCorridor = 8;

Grid<bool> town() {
    Rng rng(kTownSeed);
    return make_town(rng, true);
}

bool open_at(const Grid<bool>& b, Vec2i c) {
    return b.in_bounds(c) && !b.at(c);
}

/// A one-cell gap in a horizontal wall: open, with walls to its left and right.
bool doorway(const Grid<bool>& b, Vec2i c) {
    return open_at(b, c) && !open_at(b, c + Vec2i{-1, 0}) && !open_at(b, c + Vec2i{1, 0});
}

/// Longest horizontal one-wide corridor: a run of open cells in one row, closed
/// by walls at both ends, walled above and below except at doorways, with at
/// least one doorway opening into a room (an open cell beyond it).
int longest_corridor(const Grid<bool>& b) {
    int longest = 0;
    for (int y = 1; y < b.height() - 1; ++y) {
        int run = 0;
        bool walled = true;
        bool into_room = false;
        for (int x = 1; x < b.width(); ++x) {
            const Vec2i c{x, y};
            if (!open_at(b, c)) {
                if (run > 0 && walled && into_room && !open_at(b, c - Vec2i{run + 1, 0})) {
                    longest = std::max(longest, run);
                }
                run = 0;
                walled = true;
                into_room = false;
                continue;
            }
            ++run;
            for (const int dy : {-1, 1}) {
                const Vec2i side = c + Vec2i{0, dy};
                if (!open_at(b, side)) {
                    continue;
                }
                walled = walled && doorway(b, side);
                into_room = into_room || open_at(b, side + Vec2i{0, dy});
            }
        }
    }
    return longest;
}

} // namespace

TEST_SUITE("town") {
    TEST_CASE("the town fixture is deterministic") {
        const Grid<bool> a = town();
        const Grid<bool> b = town();
        REQUIRE(a.width() == kTownWidth);
        REQUIRE(a.height() == kTownHeight);
        CHECK(std::equal(a.begin(), a.end(), b.begin()));
    }

    TEST_CASE("every open cell is reachable from the top street") {
        const Grid<bool> b = town();
        const std::vector<int> dist = bfs_dist(b, kTopStreet);
        int open = 0;
        int unreachable = 0;
        for (int y = 0; y < b.height(); ++y) {
            for (int x = 0; x < b.width(); ++x) {
                if (!b.at(x, y)) {
                    ++open;
                    unreachable += dist[cell_index(b, {x, y})] < 0 ? 1 : 0;
                }
            }
        }
        CHECK(open > 0);
        CHECK(unreachable == 0);
    }

    TEST_CASE("the cramped town has a one-wide corridor at least 8 cells long ending in a room") {
        CHECK(longest_corridor(town()) >= kMinCorridor);
        Rng rng(kTownSeed);
        CHECK(longest_corridor(make_town(rng, false)) == 0); // the open map has none
    }

    TEST_CASE("walk_path follows a shortest route") {
        const Grid<bool> b = town();
        const Vec2i to = kTopStreet;
        const std::vector<int> dist = bfs_dist(b, to);
        const Vec2i from{kTownWidth / 2, kTownHeight / 2};
        REQUIRE(dist[cell_index(b, from)] > 0);
        const std::vector<Vec2i> path = walk_path(b, from, to);
        CHECK(static_cast<int>(path.size()) == dist[cell_index(b, from)]);
        CHECK(path.back() == to);
        Vec2i prev = from;
        for (const Vec2i p : path) {
            CHECK(can_step(b, prev, p - prev));
            prev = p;
        }
    }

#ifdef NDEBUG
    TEST_CASE("climb_all on an exact geodesic sample climbs from every cell") {
        // Proves the helper before any field depends on it: on -dist every cell has a
        // neighbour one move nearer, so every climb arrives by a shortest route.
        // Release only: ~12M neighbour checks cost ~5 s under ASan, tens of ms at -O3;
        // CI's release job and verify.py --full run it.
        const Grid<bool> b = town();
        const Vec2i goal = kTopStreet;
        const std::vector<int> dist = bfs_dist(b, goal);
        const ClimbResult r = climb_all(b, dist, goal, [&](Vec2i p) { return -dist[cell_index(b, p)]; });
        CHECK(r.success == 1.0);
        CHECK(r.stretch == 1.0);
        CHECK(r.frozen == 0);
    }

    TEST_CASE("baseline: the diffusion field on the town") {
        // The before-picture PEO-030 improves on: the review's standing source in the
        // deepest office room, 300 updates of today's field. No assertion on the two
        // shares; the review measured about 15-18% on this town.
        constexpr int kUpdates = 300;
        constexpr Vec2i kFarFrom{kTownWidth / 2, 1};
        constexpr int kMiddleX0 = 60; // the probe's search box: the middle offices
        constexpr int kMiddleX1 = 140;
        constexpr int kMiddleY0 = 40;
        constexpr int kMiddleY1 = 80;
        const Grid<bool> b = town();
        const std::vector<int> far = bfs_dist(b, kFarFrom);
        Vec2i source{};
        int deepest = -1;
        for (int y = kMiddleY0 + 1; y < kMiddleY1; ++y) {
            for (int x = kMiddleX0 + 1; x < kMiddleX1; ++x) {
                const int d = far[cell_index(b, {x, y})];
                if (!b.at(x, y) && d > deepest) {
                    deepest = d;
                    source = {x, y};
                }
            }
        }
        REQUIRE(deepest > 0);
        const std::vector<int> dist = bfs_dist(b, source);
        ScentField f(kTownWidth, kTownHeight);
        for (int i = 0; i < kUpdates; ++i) {
            f.deposit(source, kPlayerScent);
            f.step(&b);
        }
        int reachable = 0;
        int reached = 0;
        for (int y = 0; y < b.height(); ++y) {
            for (int x = 0; x < b.width(); ++x) {
                if (!b.at(x, y) && dist[cell_index(b, {x, y})] > 0) {
                    ++reachable;
                    reached += f.sample({x, y}) > 0.0F ? 1 : 0;
                }
            }
        }
        const ClimbResult c = climb_all(b, dist, source, [&](Vec2i p) { return f.sample(p); });
        MESSAGE("town baseline: source (" << source.x << "," << source.y << "), " << kUpdates
                                          << " updates: reached " << 100.0 * reached / reachable
                                          << "%, climb " << 100.0 * c.success << "%, stretch " << c.stretch
                                          << ", frozen " << c.frozen);
        CHECK(reachable > 0);
    }
#endif
}
