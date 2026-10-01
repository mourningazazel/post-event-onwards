#include "peo/core/rng.hpp"
#include "peo/core/scent_wave.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
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

    TEST_CASE("the plain and AVX2 kernels give the same field and directions" *
              doctest::test_suite("scenario: scent_wave")) {
        // PEO-085: one source built twice. Two waves on the same walled stage take the
        // same deposits, one on each kernel; after every update their values must agree,
        // and every direction byte (read through flow_target) at the midpoint and the
        // end. The stage spans 3x4 tiles, so rows cross tile borders, and the run
        // outlasts the reach, so the front, the saturated field and the aged-out cells
        // all go through both copies; then again under a wind with gusts and some indoor
        // cells, for the windy kernel (PEO-048). Sized for the scenario budget (PEO-086).
        constexpr int kW = 2 * kWaveTileWidth + 9;
        constexpr int kH = 3 * kWaveTileHeight + 3;
        constexpr int kWallOneIn = 7;
        constexpr int kEmitters = 8;
        constexpr int kUpdates = kWaveReachCells + 10;
        const bool has_avx2 = ScentWave::kernel_available(WaveKernel::Avx2);
        if (!has_avx2) {
            MESSAGE("no AVX2 on this CPU: the AVX2 half is skipped, the plain half runs");
        }
        const Grid<bool> b = random_stage(kW, kH, kWallOneIn, true, 41);
        peo::test::Rng rng(41);
        std::vector<Vec2i> emitters;
        while (emitters.size() < static_cast<std::size_t>(kEmitters)) {
            const Vec2i c{rng.range(1, kW - 2), rng.range(1, kH - 2)};
            if (!b.at(c)) {
                emitters.push_back(c);
            }
        }
        Grid<std::uint8_t> openness(kW, kH, kOpennessOutdoors);
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW / 3; ++x) {
                openness.at(x, y) =
                    static_cast<std::uint8_t>(x % 2 == 0 ? kOpennessIndoors : kOpennessOutdoors / 3);
            }
        }
        const auto differ = [&](const auto& same) {
            int n = 0;
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    n += same(Vec2i{x, y}) ? 0 : 1;
                }
            }
            return n;
        };
        for (const bool windy : {false, true}) {
            CAPTURE(windy);
            const WaveParams params{.gust = windy ? 2 : 0};
            ScentWave plain(kW, kH, params);
            ScentWave avx2(kW, kH, params);
            plain.use_kernel(WaveKernel::Plain);
            if (has_avx2) {
                avx2.use_kernel(WaveKernel::Avx2);
            }
            if (windy) {
                const Wind wind{.toward_degrees = 200, .intensity = kWindFull};
                plain.set_wind(wind);
                avx2.set_wind(wind);
            }
            for (int u = 0; u < kUpdates; ++u) {
                // Emitters fall silent one by one, so cells also age out under both kernels.
                for (std::size_t e = static_cast<std::size_t>(u) % emitters.size(); e < emitters.size();
                     ++e) {
                    plain.deposit(emitters[e], plain.params().strength);
                    avx2.deposit(emitters[e], avx2.params().strength);
                }
                plain.update(b, &openness);
                if (!has_avx2) {
                    continue;
                }
                avx2.update(b, &openness);
                CAPTURE(u);
                REQUIRE(plain.values() == avx2.values());
                if (u == kUpdates / 2 || u == kUpdates - 1) {
                    REQUIRE(differ([&](Vec2i c) { return plain.flow_target(c) == avx2.flow_target(c); }) ==
                            0);
                }
            }
            // The plain half against the oracle, once, on the aged field.
            CHECK(differ([&](Vec2i c) {
                      return b.at(c) || plain.flow_target(c) == plain.strongest_neighbour(c, &b);
                  }) == 0);
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

    // ---- PEO-048: wind -------------------------------------------------------------

    TEST_CASE("the wind table in integers") {
        CHECK(cos_degrees(0) == kTrigOne);
        CHECK(cos_degrees(90) == 0);
        CHECK(cos_degrees(180) == -kTrigOne);
        CHECK(cos_degrees(270) == 0);
        CHECK(cos_degrees(360) == kTrigOne);
        CHECK(cos_degrees(-90) == 0);
        CHECK(cos_degrees(60) == kTrigOne / 2);
        // A full wind toward east: a step with any eastward progress is fully downwind
        // (the review's probe), north and south untouched, westward fully upwind.
        const WindTable east = wind_table({.toward_degrees = 0, .intensity = kWindFull}, 0);
        constexpr std::array<std::int32_t, 8> kEast{0, kWindFull,  kWindFull,  kWindFull,
                                                    0, -kWindFull, -kWindFull, -kWindFull};
        CHECK(east.step == kEast);
        CHECK(east.downwind == 0b0000'1110); // NE, E, SE
        // Toward south-east (45 degrees, y down): diagonal progress is sqrt 2.
        const WindTable se = wind_table({.toward_degrees = 45, .intensity = kWindFull}, 0);
        CHECK(se.step[3] == 11); // SE: round(8 x 1.414)
        CHECK(se.step[2] == 6);  // E: round(8 x 0.707)
        CHECK(se.step[7] == -11);
        // The upward loss comes off every direction.
        const WindTable lossy = wind_table({.toward_degrees = 0, .intensity = kWindFull}, 1);
        CHECK(lossy.step[2] == 0);
        CHECK(lossy.step[0] == -kWindFull);
        CHECK(lossy.downwind == east.downwind);
        CHECK(wind_table({.toward_degrees = 123, .intensity = 0}, 3).calm());
    }

    TEST_CASE("a calm wind changes nothing") {
        const Grid<bool> b = random_stage(150, 30, 7, true, 51);
        ScentWave plain(150, 30);
        ScentWave calm(150, 30, {.gust = 3});
        calm.set_wind({.toward_degrees = 30, .intensity = 0});
        for (int i = 0; i < 40; ++i) {
            const Vec2i at{10 + i, 15};
            if (!b.at(at)) {
                plain.deposit(at, plain.params().strength);
                calm.deposit(at, calm.params().strength);
            }
            plain.update(b);
            calm.update(b);
        }
        CHECK(calm.values() == plain.values());
    }

    namespace {
    /// An open stage with a border wall.
    Grid<bool> open_stage(int w, int h) {
        Grid<bool> b(w, h, false);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                b.at(x, y) = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            }
        }
        return b;
    }

    struct Reach {
        int ahead = 0;
        int behind = 0;
    };
    /// The review's walker (probe 3), smaller (PEO-086): 120x40 open, the player walks
    /// east a cell an update for 40 updates from x = 10 on row 20, a full wind toward
    /// east; gust 2 is past 20 cells ahead well inside that. Deposit,
    /// update, then step, so "ahead" counts from a cell the walker has not yet stood
    /// on. `indoors` puts the path inside a building: a corridor five cells wide along
    /// the whole stage, walled both sides, openness 0 within.
    Reach walk_east(int gust, bool indoors) {
        constexpr int kW = 120;
        constexpr int kH = 40;
        constexpr int kRow = 20;
        constexpr int kStartX = 10;
        constexpr int kStopX = 110;
        constexpr int kUpdates = 40;
        constexpr int kHalfCorridor = 2;
        Grid<bool> b = open_stage(kW, kH);
        Grid<std::uint8_t> openness(kW, kH, kOpennessOutdoors);
        for (int x = 1; indoors && x < kW - 1; ++x) {
            b.at(x, kRow - kHalfCorridor - 1) = true;
            b.at(x, kRow + kHalfCorridor + 1) = true;
            for (int y = kRow - kHalfCorridor; y <= kRow + kHalfCorridor; ++y) {
                openness.at(x, y) = kOpennessIndoors;
            }
        }
        ScentWave w(kW, kH, {.gust = gust});
        w.set_wind({.toward_degrees = 0, .intensity = kWindFull});
        int px = kStartX;
        for (int t = 0; t < kUpdates; ++t) {
            w.deposit({px, kRow}, w.params().strength);
            w.update(b, &openness);
            px += px < kStopX ? 1 : 0;
        }
        Reach r;
        while (px + r.ahead + 1 < kW && w.sample({px + r.ahead + 1, kRow}) > 0) {
            ++r.ahead;
        }
        while (px - r.behind - 1 >= 0 && w.sample({px - r.behind - 1, kRow}) > 0) {
            ++r.behind;
        }
        return r;
    }
    } // namespace

    TEST_CASE("wind carries scent ahead of a walker only with gusts and only outdoors" *
              doctest::test_suite("scenario: scent_wave")) {
        // D-020 and the review's section 8.1: without gust rounds the front keeps pace
        // with the walker; with two it runs well ahead; inside a building (walls, and
        // openness 0) the wind does nothing. Behind, the trail is the walker's own
        // deposits, so it reaches the same distance in all three. (An indoor band with no
        // walls is not a building: scent leaves it sideways, rides the wind outside and
        // comes back in ahead; the probe's push lost those offers.)
        constexpr int kGust = 2;
        constexpr int kAheadWithGust = 20; // the brief's floor; the probe measured 59
        const Reach still = walk_east(0, false);
        const Reach gusty = walk_east(kGust, false);
        const Reach indoors = walk_east(kGust, true);
        MESSAGE("ahead: no gust " << still.ahead << ", gust 2 " << gusty.ahead << ", indoors "
                                  << indoors.ahead << "; behind " << still.behind << " / " << gusty.behind
                                  << " / " << indoors.behind);
        CHECK(still.ahead == 0);
        CHECK(gusty.ahead >= kAheadWithGust);
        CHECK(indoors.ahead == 0);
        CHECK(gusty.behind == still.behind);
        CHECK(indoors.behind == still.behind);
    }

    TEST_CASE("upwind reaches fewer cells than downwind" * doctest::test_suite("scenario: scent_wave")) {
        // Upwind settles near 40 cells; downwind passes that within 60 updates (PEO-086).
        constexpr int kW = 140;
        constexpr int kH = 20;
        constexpr Vec2i kSource{70, 10};
        constexpr int kUpdates = 60;
        const Grid<bool> b = open_stage(kW, kH);
        for (const int gust : {0, 2}) {
            ScentWave w(kW, kH, {.gust = gust});
            w.set_wind({.toward_degrees = 0, .intensity = kWindFull});
            for (int t = 0; t < kUpdates; ++t) {
                w.deposit(kSource, w.params().strength);
                w.update(b);
            }
            int down = 0;
            int up = 0;
            while (w.sample({kSource.x + down + 1, kSource.y}) > 0) {
                ++down;
            }
            while (w.sample({kSource.x - up - 1, kSource.y}) > 0) {
                ++up;
            }
            CAPTURE(gust);
            MESSAGE("gust " << gust << ": downwind " << down << ", upwind " << up);
            CHECK(up < down);
        }
    }

    TEST_CASE("a windy field is still a climb to its source" * doctest::test_suite("scenario: scent_wave")) {
        // Every reached cell got its value from a stronger neighbour (a step costs at
        // least 1), so a strict strongest-neighbour walk from anywhere ends on the
        // source: wind, gusts and indoor patches included.
        constexpr int kW = 160;
        constexpr int kH = 64;
        constexpr int kWallOneIn = 6;
        constexpr int kUpdates = 70;
        constexpr int kClimbLimit = 400;
        constexpr int kRoomSide = 12;
        const Grid<bool> b = random_stage(kW, kH, kWallOneIn, true, 61);
        Grid<std::uint8_t> openness(kW, kH, kOpennessOutdoors);
        for (int y = 10; y < 10 + kRoomSide; ++y) { // an indoor patch and a half-open one
            for (int x = 30; x < 30 + kRoomSide; ++x) {
                openness.at(x, y) = kOpennessIndoors;
                openness.at(x + 60, y + 30) = kOpennessOutdoors / 2;
            }
        }
        Vec2i source{kW / 2, kH / 2};
        while (b.at(source)) {
            ++source.x;
        }
        ScentWave w(kW, kH, {.gust = 2, .wind_loss = 1});
        w.set_wind({.toward_degrees = 30, .intensity = kWindFull});
        for (int t = 0; t < kUpdates; ++t) {
            w.deposit(source, w.params().strength);
            w.update(b, &openness);
        }
        int reached = 0;
        int lost = 0;
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                if (b.at(x, y) || w.sample({x, y}) == 0) {
                    continue;
                }
                ++reached;
                Vec2i p{x, y};
                for (int s = 0; s < kClimbLimit; ++s) {
                    const auto next = w.strongest_neighbour(p, &b);
                    if (!next) {
                        break;
                    }
                    p = *next;
                }
                lost += p == source ? 0 : 1;
            }
        }
        MESSAGE("reached " << reached << " cells");
        CHECK(reached > 0);
        CHECK(lost == 0);
    }

    TEST_CASE("patch_deposit is exact under wind and gusts" * doctest::test_suite("scenario: scent_wave")) {
        // PEO-048's oracle: on random walled stages with random openness, wind and gust
        // 1-3, a deposit patched in after update() equals deposit() then update(), value
        // for value and byte for byte, and both waves stay equal through more updates
        // (so the patch also leaves the right tiles active).
        constexpr int kW = kWaveTileWidth + 20;
        constexpr int kH = 2 * kWaveTileHeight + 5;
        constexpr int kWallOneIn = 7;
        constexpr int kTrials = 6; // gust 1-3 twice, the upward loss in trial 3 (PEO-086)
        constexpr int kWarm = 8;
        constexpr int kAfter = 4;
        Rng rng(71);
        const auto open_cell = [&](const Grid<bool>& b) {
            Vec2i c{};
            do {
                c = {rng.range(0, kW - 1), rng.range(0, kH - 1)};
            } while (b.at(c));
            return c;
        };
        int windy = 0;
        for (int trial = 0; trial < kTrials; ++trial) {
            const Grid<bool> b =
                random_stage(kW, kH, kWallOneIn, trial % 2 == 0, static_cast<Seed>(80 + trial));
            Grid<std::uint8_t> openness(kW, kH, kOpennessOutdoors);
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    const int pick = rng.range(0, 9);
                    openness.at(x, y) = pick == 0 ? kOpennessIndoors
                                        : pick == 1
                                            ? static_cast<std::uint8_t>(rng.range(1, kOpennessOutdoors - 1))
                                            : kOpennessOutdoors;
                }
            }
            const WaveParams p{.gust = 1 + trial % 3, .wind_loss = trial % 4 == 3 ? 1 : 0};
            const Wind wind{.toward_degrees = rng.range(0, kDegreesPerTurn - 1),
                            .intensity = rng.range(1, kWindFull + 4)};
            ScentWave truth(kW, kH, p);
            truth.set_wind(wind);
            windy += truth.wind().calm() ? 0 : 1;
            for (int i = 0; i < kWarm; ++i) {
                truth.deposit(open_cell(b), p.strength);
                truth.update(b, &openness);
            }
            const ScentWave before = truth;
            const Vec2i at = open_cell(b);
            truth.deposit(at, p.strength);
            truth.update(b, &openness);
            ScentWave patched = before;
            patched.update(b, &openness);
            patched.patch_deposit(before, at, p.strength, b, &openness);
            CAPTURE(trial);
            REQUIRE(patched.values() == truth.values());
            for (int i = 0; i < kAfter; ++i) {
                const Vec2i next = open_cell(b);
                truth.deposit(next, p.strength);
                patched.deposit(next, p.strength);
                truth.update(b, &openness);
                patched.update(b, &openness);
                CAPTURE(i);
                REQUIRE(patched.values() == truth.values());
            }
            int differ = 0;
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    differ += patched.flow_target({x, y}) == truth.flow_target({x, y}) ? 0 : 1;
                }
            }
            CHECK(differ == 0);
        }
        CHECK(windy == kTrials);
    }
}
