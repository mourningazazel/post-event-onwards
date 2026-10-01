#include "peo/core/dead.hpp"
#include "peo/core/scent_wave.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

/// The review's source: the open cell farthest by route from the top street's middle,
/// inside the middle offices (the probe's search box).
Vec2i deepest_office_cell(const Grid<bool>& b) {
    constexpr Vec2i kFarFrom{kTownWidth / 2, 1};
    constexpr int kMiddleX0 = 60;
    constexpr int kMiddleX1 = 140;
    constexpr int kMiddleY0 = 40;
    constexpr int kMiddleY1 = 80;
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
    return source;
}

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

/// One of the Dead alone on the town: it decides once per update (about its pace at
/// a 6 s step) and nothing else occupies or reserves a tile.
struct Follower {
    Dead unit;
    Grid<std::uint8_t> occupied{kTownWidth, kTownHeight, 0};
    Grid<bool> reserved{kTownWidth, kTownHeight, false};
    void decide(const ScentWave& f, const Grid<bool>& b) {
        if (const auto to = decide_move(unit, f, b, occupied, reserved)) {
            unit.pos = *to;
        }
    }
};

} // namespace

// Scenario tier (D-033): runs under the 5 s scenario budget, not the 1 s unit budget.
TEST_SUITE("scenario: town") {
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

    TEST_CASE("the town's buildings are indoors, its streets outdoors") {
        // PEO-048: every cell enclosed by a building's outer wall is indoors; the top
        // street and the left street are outdoors.
        const Grid<bool> b = town();
        const Grid<std::uint8_t> o = town_openness(true);
        CHECK(o.at(kTopStreet) == kOpennessOutdoors);
        CHECK(o.at(1, kTownHeight / 2) == kOpennessOutdoors);
        const Vec2i inside = deepest_office_cell(b);
        CHECK(o.at(inside) == kOpennessIndoors);
        CHECK(o.at(kStreetWidth, kStreetWidth) == kOpennessIndoors); // a building's corner wall
        const Grid<std::uint8_t> open = town_openness(false);
        CHECK(std::all_of(open.begin(), open.end(), [](std::uint8_t v) { return v == kOpennessOutdoors; }));
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

    // PEO-030 on the town. Cheap in every build: the wave only works the cells its
    // fronts reach, a few thousand here, not the map.
    TEST_CASE("the geodesic field on the town") {
        // A standing deposit in the deepest office room: every cell within reach reads
        // scent, and a climb from each of them arrives by a shortest route.
        const Grid<bool> b = town();
        const Vec2i source = deepest_office_cell(b);
        ScentWave f(kTownWidth, kTownHeight);
        for (int i = 0; i <= kWaveReachCells; ++i) {
            f.deposit(source, f.params().strength);
            f.update(b);
        }
        std::vector<int> dist = bfs_dist(b, source);
        int within = 0;
        int missing = 0;
        for (std::size_t i = 0; i < dist.size(); ++i) {
            if (dist[i] >= kWaveReachCells) {
                dist[i] = -1; // out of reach: not a starting cell for the climb
            } else if (dist[i] > 0) {
                ++within;
                const Vec2i c{static_cast<int>(i % kTownWidth), static_cast<int>(i / kTownWidth)};
                missing += f.sample(c) > 0 ? 0 : 1;
            }
        }
        const ClimbResult c = climb_all(b, dist, source, [&](Vec2i p) { return f.sample(p); });
        MESSAGE("geodesic field on the town: " << within << " cells within " << kWaveReachCells
                                               << " of the source, climb " << 100.0 * c.success << "%");
        CHECK(within > 0);
        CHECK(missing == 0);
        CHECK(c.success == 1.0);
        CHECK(c.stretch == 1.0);
        CHECK(c.frozen == 0);
    }

    TEST_CASE("a Dead follows a trail into the house") {
        // The player walks from the top street to the deepest office room, a tile per
        // update, then waits; a Dead starting on the path 5 updates behind follows the
        // trail in instead of parking at a crossing.
        constexpr int kBehind = 5;
        constexpr int kWait = 50;
        const Grid<bool> b = town();
        const Vec2i room = deepest_office_cell(b);
        const Vec2i street{room.x, 1};
        const std::vector<Vec2i> path = walk_path(b, street, room);
        REQUIRE(static_cast<int>(path.size()) > kBehind);
        ScentWave f(kTownWidth, kTownHeight);
        Vec2i player = street;
        Follower dead{.unit = {.pos = street}};
        for (std::size_t i = 0; i < path.size() + kWait; ++i) {
            player = i < path.size() ? path[i] : player;
            f.deposit(player, f.params().strength);
            f.update(b);
            if (i >= kBehind) {
                dead.decide(f, b);
            }
        }
        CAPTURE(dead.unit.pos.x);
        CAPTURE(dead.unit.pos.y);
        CHECK(chebyshev(dead.unit.pos, player) <= 1);
    }

    TEST_CASE("the doorway scenario") {
        // D-008's old acceptance: linger 30 updates just inside a house's front door,
        // walk 20 cells into the house, and one of the Dead starting outside on the
        // street reaches the door.
        constexpr int kLinger = 30;
        constexpr int kInto = 20;
        constexpr int kAfter = 40;
        constexpr int kStreetOffset = 6; // the Dead starts this far along the street
        const Grid<bool> b = town();
        // The first house: its front door is the one gap in its top wall.
        constexpr int kX0 = kStreetWidth;
        constexpr int kY0 = kStreetWidth;
        Vec2i door{};
        for (int x = kX0 + 1; x < kX0 + kBlockPitchX - kStreetWidth - 1; ++x) {
            if (!b.at(x, kY0)) {
                door = {x, kY0};
            }
        }
        REQUIRE(door.x > 0);
        const Vec2i inside = door + Vec2i{0, 1};
        // Twenty cells in: the first cell of a route that far from the door, inside.
        const std::vector<int> from_door = bfs_dist(b, inside);
        Vec2i deep = inside;
        for (int y = kY0 + 1; y < kY0 + kBlockPitchY - kStreetWidth; ++y) {
            for (int x = kX0 + 1; x < kX0 + kBlockPitchX - kStreetWidth; ++x) {
                if (from_door[cell_index(b, {x, y})] == kInto) {
                    deep = {x, y};
                }
            }
        }
        REQUIRE(deep != inside);
        const std::vector<Vec2i> walk = walk_path(b, inside, deep);
        ScentWave f(kTownWidth, kTownHeight);
        Follower dead{.unit = {.pos = Vec2i{door.x + kStreetOffset, 1}}};
        REQUIRE_FALSE(b.at(dead.unit.pos));
        Vec2i player = inside;
        bool reached = false;
        const std::size_t updates = kLinger + walk.size() + kAfter;
        for (std::size_t i = 0; i < updates; ++i) {
            if (i >= static_cast<std::size_t>(kLinger) && i - kLinger < walk.size()) {
                player = walk[i - kLinger];
            }
            f.deposit(player, f.params().strength);
            f.update(b);
            dead.decide(f, b);
            reached = reached || dead.unit.pos == door;
        }
        CHECK(reached);
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

#endif
}
