#include "peo/core/rng.hpp"
#include "peo/core/scent_wave.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "reference_wave.hpp"
#include "town_fixture.hpp"

using namespace peo::core;

namespace {

constexpr int kSide = 41;
constexpr Vec2i kCentre{20, 20};
/// Distance only: no ageing, so a value reads strength - distance_cost x route.
constexpr WaveParams kNoAge{.strength = 200, .distance_cost = 8, .age_cost = 0, .speed = 1};

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

void run(ScentWave& w, const Grid<bool>& walls, int updates) {
    for (int i = 0; i < updates; ++i) {
        w.update(walls);
    }
}

/// A stage with one wall cell in `wall_one_in`, and a border wall when `bordered`.
Grid<bool> random_stage(int w, int h, int wall_one_in, bool bordered, Seed seed) {
    Rng rng(seed);
    Grid<bool> b(w, h, false);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const bool edge = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            b.at(x, y) = (bordered && edge) || rng.range(1, wall_one_in) == 1;
        }
    }
    return b;
}

/// PEO-078's oracle: the push (ReferenceWave) and ScentWave side by side on `b`, with
/// `emitters_per_1000` standing sources and one walker wandering the open cells, every
/// update compared value for value. Returns the first update that differs, or -1.
int first_mismatch(const Grid<bool>& b, WaveParams p, int emitters_per_1000, int updates, Seed seed) {
    constexpr int kPerMille = 1000;
    Rng rng(seed);
    const auto open_cell = [&] {
        Vec2i c{};
        do {
            c = {rng.range(0, b.width() - 1), rng.range(0, b.height() - 1)};
        } while (b.at(c));
        return c;
    };
    std::vector<Vec2i> emitters(
        static_cast<std::size_t>(b.width() * b.height() * emitters_per_1000 / kPerMille));
    for (Vec2i& e : emitters) {
        e = open_cell();
    }
    Vec2i walker = open_cell();
    peo::test::ReferenceWave ref(b.width(), b.height(), p);
    ScentWave wave(b.width(), b.height(), p);
    for (int u = 0; u < updates; ++u) {
        const Vec2i step = kNeighbours4[static_cast<std::size_t>(rng.range(0, 3))];
        if (b.in_bounds(walker + step) && !b.at(walker + step)) {
            walker = walker + step;
        }
        for (const Vec2i e : emitters) {
            ref.deposit(e, p.strength);
            wave.deposit(e, p.strength);
        }
        ref.deposit(walker, p.strength);
        wave.deposit(walker, p.strength);
        ref.update(b);
        wave.update(b);
        if (ref.values() != wave.values() || ref.updates() != wave.updates()) {
            return u;
        }
    }
    return -1;
}

} // namespace

TEST_SUITE("scent_wave") {
    TEST_CASE("on an open grid a deposit reads strength less distance_cost per cell") {
        const Grid<bool> walls(kSide, kSide, false);
        ScentWave w(kSide, kSide, kNoAge);
        w.deposit(kCentre, kNoAge.strength);
        run(w, walls, kSide);
        for (int y = 0; y < kSide; ++y) {
            for (int x = 0; x < kSide; ++x) {
                const int d = chebyshev({x, y}, kCentre); // 8-connected route on open ground
                CAPTURE(x);
                CAPTURE(y);
                CHECK(w.sample({x, y}) == std::max(0, kNoAge.strength - kNoAge.distance_cost * d));
            }
        }
    }

    TEST_CASE("a wall line absorbs: nothing behind it without a route round") {
        Grid<bool> walls(kSide, kSide, false);
        constexpr int kWallX = 25;
        for (int y = 0; y < kSide; ++y) {
            walls.at(kWallX, y) = true;
        }
        ScentWave w(kSide, kSide, kNoAge);
        w.deposit(kCentre, kNoAge.strength);
        run(w, walls, kSide);
        for (int y = 0; y < kSide; ++y) {
            for (int x = kWallX; x < kSide; ++x) {
                CHECK(w.sample({x, y}) == 0);
            }
        }
        CHECK(w.sample({kWallX - 1, kCentre.y}) > 0);
    }

    TEST_CASE("a one-wide corridor loses exactly distance_cost per cell") {
        constexpr int kLength = 20;
        constexpr int kRow = 1;
        Grid<bool> walls(kLength, 3, true);
        for (int x = 0; x < kLength; ++x) {
            walls.at(x, kRow) = false;
        }
        ScentWave w(kLength, 3, kNoAge);
        w.deposit({0, kRow}, kNoAge.strength);
        run(w, walls, kLength);
        for (int x = 1; x < kLength; ++x) {
            CHECK(w.sample({x - 1, kRow}) - w.sample({x, kRow}) ==
                  std::min(kNoAge.distance_cost, w.sample({x - 1, kRow})));
        }
        CHECK(w.sample({kLength - 1, kRow}) ==
              std::max(0, kNoAge.strength - kNoAge.distance_cost * (kLength - 1)));
    }

    TEST_CASE("with no deposit the field ages by age_cost per update") {
        const WaveParams p{};
        const Grid<bool> walls(kSide, kSide, false);
        ScentWave w(kSide, kSide, p);
        w.deposit(kCentre, p.strength);
        constexpr int kSettle = 3;
        run(w, walls, kSettle);
        const Vec2i near = kCentre + Vec2i{1, 0};
        std::int32_t before = w.sample(near);
        REQUIRE(before > 0);
        for (int i = 0; i < kSettle; ++i) {
            w.update(walls);
            CHECK(w.sample(near) == before - p.age_cost);
            before = w.sample(near);
        }
    }

    TEST_CASE("speed 2 advances the front two cells per update") {
        const Grid<bool> walls(kSide, kSide, false);
        for (const int speed : {1, 2}) {
            ScentWave w(kSide, kSide, {.speed = speed});
            w.deposit(kCentre, w.params().strength);
            w.update(walls);
            CAPTURE(speed);
            CHECK(w.sample(kCentre + Vec2i{speed, 0}) > 0);
            CHECK(w.sample(kCentre + Vec2i{speed + 1, 0}) == 0);
        }
    }

    TEST_CASE("the wave never cuts a wall corner") {
        // (1,1) and (2,2) touch only diagonally between the walls at (2,1) and (1,2);
        // the only other way round is sealed, so (2,2) must stay at 0.
        Grid<bool> walls(4, 4, true);
        walls.at(1, 1) = false;
        walls.at(2, 2) = false;
        ScentWave w(4, 4, kNoAge);
        w.deposit({1, 1}, kNoAge.strength);
        run(w, walls, 4);
        CHECK(w.sample({2, 2}) == 0);
        CHECK(w.strongest_neighbour({2, 2}, &walls) == std::nullopt);
    }

    TEST_CASE("patch_deposit after update equals deposit then update") {
        // PEO-030's commit patch: the World speculates the update with no deposit
        // and patches the player's in. Random walls, a standing source and moving
        // deposits, 1-3 per update; the two paths must match bit for bit.
        constexpr int kW = 30;
        constexpr int kH = 20;
        constexpr int kUpdates = 300;
        constexpr int kWallOneIn = 6;
        Rng rng(7);
        Grid<bool> walls(kW, kH, false);
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                walls.at(x, y) = rng.range(1, kWallOneIn) == 1;
            }
        }
        const WaveParams p{};
        ScentWave plain(kW, kH, p);
        ScentWave patched(kW, kH, p);
        std::vector<Vec2i> tiles;
        for (int t = 0; t < kUpdates; ++t) {
            tiles.clear();
            const int count = rng.range(1, 3);
            for (int k = 0; k < count; ++k) {
                Vec2i c{};
                do {
                    c = {rng.range(0, kW - 1), rng.range(0, kH - 1)};
                } while (walls.at(c));
                tiles.push_back(c);
            }
            const std::int32_t strength = p.strength - p.age_cost * rng.range(0, 2);
            for (const Vec2i c : tiles) {
                plain.deposit(c, strength);
            }
            plain.update(walls);
            const ScentWave before = patched;
            patched.update(walls);
            for (const Vec2i c : tiles) {
                patched.patch_deposit(before, c, strength, walls);
            }
            if (plain.values() != patched.values()) {
                FAIL("diverged at update " << t);
            }
        }
        CHECK(plain.updates() == patched.updates());
    }
#ifdef NDEBUG
    TEST_CASE("nothing is read past strength / (distance_cost + age_cost / speed) cells") {
        // A standing source long past steady state. Strength set for a 20-cell reach
        // at speed 1 (26 at speed 2), so a small grid holds the whole disc. Release
        // only: ~110 ms under ASan, the suite's largest case (PEO-063 headroom).
        constexpr int kReach = 20;
        constexpr int kBig = 61;
        constexpr Vec2i kMid{30, 30};
        constexpr int kUpdates = 45; // the disc stops growing after ~reach updates
        for (const int speed : {1, 2}) {
            const WaveParams p{.strength = kReach * (kWaveDistanceCost + kWaveAgeCost), .speed = speed};
            const Grid<bool> walls(kBig, kBig, false);
            ScentWave w(kBig, kBig, p);
            for (int i = 0; i < kUpdates; ++i) {
                w.deposit(kMid, p.strength);
                w.update(walls);
            }
            const int reach = p.strength / (p.distance_cost + p.age_cost / p.speed);
            REQUIRE(reach < kMid.x);
            int farthest = 0;
            for (int y = 0; y < kBig; ++y) {
                for (int x = 0; x < kBig; ++x) {
                    if (w.sample({x, y}) > 0) {
                        farthest = std::max(farthest, chebyshev({x, y}, kMid));
                    }
                }
            }
            CAPTURE(speed);
            CHECK(farthest <= reach);
            CHECK(farthest >= reach - 1); // and it does reach about that far
        }
    }
#endif

    TEST_CASE("the field equals the reference push on every update" *
              doctest::test_suite("scenario: scent_wave")) {
        // PEO-078: small stages in every build; the town and larger stages below in
        // release. Open and walled borders, speed 1 and 2, emitters and a walker.
        constexpr int kW = 48;
        constexpr int kH = 32;
        constexpr int kWallOneIn = 7;
        constexpr int kEmitters = 3; // per 1000 tiles
        constexpr int kUpdates = 60; // the reach fills in 60 updates
        // Two cases cover both borders and both speeds (suite budget).
        for (const auto& [bordered, speed] : {std::pair{false, 1}, std::pair{true, 2}}) {
            const Grid<bool> b = random_stage(kW, kH, kWallOneIn, bordered, 11);
            CAPTURE(bordered);
            CAPTURE(speed);
            CHECK(first_mismatch(b, {.speed = speed}, kEmitters, kUpdates, 5) == -1);
        }
    }
    TEST_CASE("a partner sync copies only written tiles and equals a full copy" *
              doctest::test_suite("scenario: scent_wave")) {
        // PEO-078: the World's wave and its Speculation's are partners after a copy; a
        // later sync copies just the tiles either wrote, and the waves stay equal.
        constexpr int kBig = 512;
        constexpr Vec2i kSource{40, 40};
        constexpr int kUpdates = 10;
        const Grid<bool> walls(kBig, kBig, false);
        ScentWave world(kBig, kBig);
        world.set_token(1);
        world.deposit(kSource, world.params().strength);
        world.update(walls);
        ScentWave spec(1, 1);
        const std::size_t cold = spec.sync_from(world); // not partners yet: everything
        CHECK(spec.values() == world.values());
        for (int i = 0; i < kUpdates; ++i) {
            world.deposit(kSource, world.params().strength);
            world.update(walls);
            spec.update(walls); // the partner moves on too: its writes are recorded
            const std::size_t warm = spec.sync_from(world);
            CHECK(warm < cold);
            CHECK(spec.values() == world.values());
            CHECK(spec.updates() == world.updates());
            ScentWave a = world;
            ScentWave b = spec;
            a.update(walls);
            b.update(walls);
            CHECK(a.values() == b.values()); // and they update alike
        }
    }

    TEST_CASE("flow_target equals strongest_neighbour on every open cell" *
              doctest::test_suite("scenario: scent_wave")) {
        // PEO-079's contract. The stage spans 3x3 pull tiles, sources sit on tile
        // borders, and the check runs after updates, a patched deposit and both kinds
        // of sync, so a direction byte left stale anywhere shows up here.
        constexpr int kW = 2 * kWaveTileWidth + 16;
        constexpr int kH = 2 * kWaveTileHeight + 5;
        constexpr int kWallOneIn = 6;
        constexpr int kUpdates = 9;
        const Grid<bool> b = random_stage(kW, kH, kWallOneIn, true, 31);
        const auto open_near = [&](Vec2i c) {
            while (b.at(c)) {
                c.x = c.x % (kW - 2) + 1;
            }
            return c;
        };
        const Vec2i sources[] = {open_near({kWaveTileWidth - 1, kWaveTileHeight}),
                                 open_near({kWaveTileWidth, 2 * kWaveTileHeight - 1}),
                                 open_near({2 * kWaveTileWidth + 5, 2 * kWaveTileHeight})};
        const auto agrees = [&](const ScentWave& w) {
            int wrong = 0;
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    if (!b.at(x, y) && w.flow_target({x, y}) != w.strongest_neighbour({x, y}, &b)) {
                        ++wrong;
                    }
                }
            }
            return wrong;
        };
        ScentWave world(kW, kH);
        world.set_token(1);
        CHECK_FALSE(world.flow_target(sources[0]).has_value()); // no update yet
        ScentWave spec(1, 1);
        for (int i = 0; i < kUpdates; ++i) {
            const Vec2i at = sources[static_cast<std::size_t>(i) % std::size(sources)];
            if (i % 3 == 2) { // as the World commits a speculation: update, then patch
                const ScentWave before = world;
                world.update(b);
                world.patch_deposit(before, at, world.params().strength, b);
            } else {
                world.deposit(at, world.params().strength);
                world.update(b);
            }
            CAPTURE(i);
            CHECK(agrees(world) == 0);
            if (i > 0) {
                spec.update(b); // the partner's own writes, then a warm sync
            }
            spec.sync_from(world); // the first is cold: a full copy
            CHECK(agrees(spec) == 0);
        }
        // A deposit between updates (World::deposit) is read at once, as the field is.
        world.deposit(open_near({kWaveTileWidth / 2, kH / 2}), world.params().strength);
        CHECK(agrees(world) == 0);
    }

#ifdef NDEBUG
    TEST_CASE("the field equals the reference push on the town and a large open stage") {
        constexpr int kEmitters = 6;  // per 1000 tiles
        constexpr int kUpdates = 120; // past the 60-cell reach's saturation, twice over
        constexpr int kW = 200;
        constexpr int kH = 120;
        constexpr int kWallOneIn = 9;
        peo::test::Rng town_rng(11);
        const Grid<bool> town = peo::test::make_town(town_rng, true);
        const Grid<bool> open = random_stage(kW, kH, kWallOneIn, true, 23);
        for (const int speed : {1, 2}) {
            CAPTURE(speed);
            CHECK(first_mismatch(town, {.speed = speed}, kEmitters, kUpdates, 7) == -1);
            CHECK(first_mismatch(open, {.speed = speed}, kEmitters, kUpdates, 9) == -1);
        }
    }
#endif
}
