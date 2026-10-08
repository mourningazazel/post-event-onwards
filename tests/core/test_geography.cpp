#include "peo/core/geography.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "geo_fixture.hpp"

using namespace peo::core;

namespace {
constexpr Seed kSeed = 1;

/// The cell at world (x, y) of a region that holds it.
template <typename T> T at(const Grid<T>& g, const CellRect& r, std::int64_t x, std::int64_t y) {
    return g.at(static_cast<int>(x - r.x), static_cast<int>(y - r.y));
}

/// Twice a ring's signed area (the shoelace sum).
std::int64_t twice_area(const std::vector<CellCorner>& ring) {
    std::int64_t a = 0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const CellCorner p = ring[i];
        const CellCorner q = ring[(i + 1) % ring.size()];
        a += p.x * q.y - q.x * p.y;
    }
    return a;
}

/// Lakes with their key, level and cell count, by key.
std::map<std::uint64_t, std::pair<std::int16_t, std::uint32_t>> lakes_of(const GeographyRegion& g) {
    std::map<std::uint64_t, std::pair<std::int16_t, std::uint32_t>> out;
    for (const WaterBody& b : g.bodies) {
        if (b.kind == WaterBody::Kind::Lake) {
            out[b.key] = {b.level_m, b.cells};
        }
    }
    return out;
}

/// Two regions agree on every cell both hold: elevation, cover, and for lake cells the
/// lake's key and level.
void check_agree(const GeographyRegion& a, const GeographyRegion& b) {
    const std::int64_t x0 = std::max(a.rect.x, b.rect.x);
    const std::int64_t y0 = std::max(a.rect.y, b.rect.y);
    const std::int64_t x1 = std::min(a.rect.x + a.rect.width, b.rect.x + b.rect.width);
    const std::int64_t y1 = std::min(a.rect.y + a.rect.height, b.rect.y + b.rect.height);
    REQUIRE(x0 < x1);
    REQUIRE(y0 < y1);
    for (std::int64_t y = y0; y < y1; ++y) {
        for (std::int64_t x = x0; x < x1; ++x) {
            const bool same = at(a.elevation_m, a.rect, x, y) == at(b.elevation_m, b.rect, x, y) &&
                              at(a.cover, a.rect, x, y) == at(b.cover, b.rect, x, y);
            if (!same) {
                FAIL("regions differ at " << x << "," << y);
            }
            if ((at(a.cover, a.rect, x, y) & kCoverLake) != 0) {
                const WaterBody& la = a.bodies[at(a.body, a.rect, x, y) - 1U];
                const WaterBody& lb = b.bodies[at(b.body, b.rect, x, y) - 1U];
                if (la.key != lb.key || la.level_m != lb.level_m || la.cells != lb.cells) {
                    FAIL("a lake differs at " << x << "," << y);
                }
            }
        }
    }
}

/// Rings closed, one axis step at a time, areas summing to the body's cells; and the
/// body grid holding each body exactly where its cover says.
void check_bodies(const GeographyRegion& g) {
    std::vector<std::uint32_t> in_grid(g.bodies.size() + 1, 0);
    for (int y = 0; y < g.rect.height; ++y) {
        for (int x = 0; x < g.rect.width; ++x) {
            const std::uint16_t id = g.body.at(x, y);
            const std::uint8_t c = g.cover.at(x, y);
            if (id == 0) {
                REQUIRE((c & (kCoverSea | kCoverLake)) == 0);
                continue;
            }
            REQUIRE(id <= g.bodies.size());
            const bool lake = g.bodies[id - 1U].kind == WaterBody::Kind::Lake;
            REQUIRE((c & (lake ? kCoverLake : kCoverSea)) != 0);
            ++in_grid[id];
        }
    }
    for (std::size_t i = 0; i < g.bodies.size(); ++i) {
        const WaterBody& b = g.bodies[i];
        CAPTURE(i);
        std::int64_t area2 = 0;
        for (const auto& ring : b.rings) {
            REQUIRE(ring.size() >= 4);
            for (std::size_t k = 0; k < ring.size(); ++k) {
                const CellCorner p = ring[k];
                const CellCorner q = ring[(k + 1) % ring.size()];
                REQUIRE(std::abs(p.x - q.x) + std::abs(p.y - q.y) == 1);
            }
            area2 += twice_area(ring);
        }
        CHECK(area2 == 2 * std::int64_t{b.cells});
        // A sea is wholly in the region; a lake may reach past it.
        if (b.kind == WaterBody::Kind::Sea) {
            CHECK(in_grid[i + 1] == b.cells);
        } else {
            CHECK(in_grid[i + 1] <= b.cells);
        }
    }
}
} // namespace

TEST_SUITE("geography") {
    TEST_CASE("cells and squares round-trip, with floor division below zero") {
        for (const std::int64_t c : {0LL, 999LL, 1000LL, -1LL, -1000LL, -1001LL}) {
            CAPTURE(c);
            const SquareCoord s = square_of(c, -c);
            const CellRect r = square_rect(s);
            CHECK(r.x <= c);
            CHECK(c < r.x + r.width);
            CHECK(r.y <= -c);
            CHECK(-c < r.y + r.height);
            CHECK(r.width == kSquareCells);
        }
        CHECK(square_of(-1, 0) == SquareCoord{-1, 0});
        CHECK(square_of(999, 1000) == SquareCoord{0, 1});
        CHECK(square_of(-1000, -1001) == SquareCoord{-1, -2});
    }

    TEST_CASE("every cell of a region is terrain_at") {
        const GeographyParams p;
        const CellRect r{-20, 300, 64, 64};
        const GeographyRegion g = generate_geography(kSeed, p, r);
        for (int y = 0; y < r.height; ++y) {
            for (int x = 0; x < r.width; ++x) {
                const TerrainSample t = terrain_at(kSeed, p, r.x + x, r.y + y);
                const std::uint8_t c = g.cover.at(x, y);
                REQUIRE(g.elevation_m.at(x, y) == t.elevation_m);
                REQUIRE(((c & kCoverSea) != 0) == (t.elevation_m < p.sea_level_m));
                REQUIRE(((c & kCoverForest) != 0) == (t.forest && (c & kCoverLake) == 0));
            }
        }
    }

    TEST_CASE("regions agree wherever they meet: across a square edge and across cell 0" *
              doctest::test_suite("scenario: geography")) {
        const GeographyParams p;
        constexpr int kSide = 48;
        for (const CellRect left :
             {CellRect{kSquareCells - kSide, 470, kSide, kSide}, CellRect{-kSide, -24, kSide, kSide}}) {
            const CellRect right{left.x + kSide, left.y, kSide, kSide};
            const CellRect both{left.x, left.y, 2 * kSide, kSide};
            const GeographyRegion whole = generate_geography(kSeed, p, both);
            check_agree(whole, generate_geography(kSeed, p, left));
            check_agree(whole, generate_geography(kSeed, p, right));
        }
    }

    TEST_CASE("lakes sit in their own grid square, below their level and above the sea" *
              doctest::test_suite("scenario: geography")) {
        GeographyParams p;
        p.lake_chance_q16 = kNoiseOne; // every site rolls
        REQUIRE(valid(p));
        const CellRect r{-96, -96, 192, 192}; // nine lake grid squares, across cell 0
        const GeographyRegion g = generate_geography(kSeed, p, r);
        int lakes = 0;
        for (const WaterBody& b : g.bodies) {
            if (b.kind != WaterBody::Kind::Lake) {
                continue;
            }
            ++lakes;
            // Inside the max-radius disc and off its rim: narrower than the disc.
            CHECK(b.bounds.width < 2 * p.lake_max_radius_cells);
            CHECK(b.bounds.height < 2 * p.lake_max_radius_cells);
            // The disc lies inside the site's grid square, so lakes never overlap.
            const auto grid = [&](std::int64_t v) {
                return v >= 0 ? v / p.lake_site_spacing_cells : (v + 1) / p.lake_site_spacing_cells - 1;
            };
            CHECK(grid(b.bounds.x) == grid(b.bounds.x + b.bounds.width - 1));
            CHECK(grid(b.bounds.y) == grid(b.bounds.y + b.bounds.height - 1));
            // Every cell below the level, above the sea, and 4-connected.
            std::set<std::pair<std::int64_t, std::int64_t>> cells;
            for (std::int64_t y = b.bounds.y; y < b.bounds.y + b.bounds.height; ++y) {
                for (std::int64_t x = b.bounds.x; x < b.bounds.x + b.bounds.width; ++x) {
                    const bool mine = x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height &&
                                      at(g.body, r, x, y) != 0 &&
                                      g.bodies[at(g.body, r, x, y) - 1U].key == b.key;
                    if (mine) {
                        const std::int16_t e = terrain_at(kSeed, p, x, y).elevation_m;
                        CHECK(e < b.level_m);
                        CHECK(e > p.sea_level_m);
                        cells.insert({x, y});
                    }
                }
            }
            if (cells.size() == b.cells) { // wholly inside the region: check it is one piece
                std::set<std::pair<std::int64_t, std::int64_t>> reached{*cells.begin()};
                std::deque<std::pair<std::int64_t, std::int64_t>> q{*cells.begin()};
                while (!q.empty()) {
                    const auto [x, y] = q.front();
                    q.pop_front();
                    for (const Vec2i d : kNeighbours4) {
                        const std::pair<std::int64_t, std::int64_t> n{x + d.x, y + d.y};
                        if (cells.contains(n) && reached.insert(n).second) {
                            q.push_back(n);
                        }
                    }
                }
                CHECK(reached.size() == cells.size());
            }
        }
        CHECK(lakes > 0);
        GeographyParams tight = p;
        tight.lake_site_spacing_cells = 2 * p.lake_max_radius_cells;
        CHECK_FALSE(valid(tight));
    }

    TEST_CASE("a lake across a square edge is the same lake from either side") {
        // The edge between squares (3,0) and (4,0), x = 4000, falls mid-way through its
        // lake grid square's jitter range, so lakes straddle it. A lake is found whole from
        // terrain_at, so one column each side sees every lake that crosses.
        GeographyParams p;
        p.lake_chance_q16 = kNoiseOne;
        constexpr std::int64_t kEdge = 4 * std::int64_t{kSquareCells};
        constexpr int kRows = 512;
        const GeographyRegion a = generate_geography(kSeed, p, CellRect{kEdge - 1, 0, 1, kRows});
        const GeographyRegion b = generate_geography(kSeed, p, CellRect{kEdge, 0, 1, kRows});
        const auto la = lakes_of(a);
        const auto lb = lakes_of(b);
        int straddling = 0;
        for (const WaterBody& w : a.bodies) {
            if (w.kind == WaterBody::Kind::Lake && w.bounds.x < kEdge &&
                w.bounds.x + w.bounds.width > kEdge) {
                ++straddling;
                REQUIRE(lb.contains(w.key));
                CHECK(lb.at(w.key) == la.at(w.key));
            }
        }
        CHECK(straddling > 0);
        check_agree(a, generate_geography(kSeed, p, CellRect{kEdge - 1, 0, 2, kRows}));
    }

    TEST_CASE("rings close, step along an axis and enclose exactly their bodies" *
              doctest::test_suite("scenario: geography")) {
        GeographyParams p;
        p.lake_chance_q16 = kNoiseOne;
        check_bodies(generate_geography(kSeed, p, CellRect{-200, 900, 192, 192}));
    }

    TEST_CASE("forest grows only on land below the treeline") {
        const GeographyParams p;
        const CellRect r{400, 400, 128, 128};
        const GeographyRegion g = generate_geography(kSeed, p, r);
        for (int y = 0; y < r.height; ++y) {
            for (int x = 0; x < r.width; ++x) {
                if ((g.cover.at(x, y) & kCoverForest) != 0) {
                    REQUIRE((g.cover.at(x, y) & (kCoverSea | kCoverLake)) == 0);
                    REQUIRE(g.elevation_m.at(x, y) < p.treeline_m);
                }
            }
        }
    }

    TEST_CASE("a pinned region: any platform or compiler that moves one cell fails") {
        // 128 x 128 of seed 1 at (-64, 936): it straddles x = 0 and the edge between
        // squares (0,0) and (0,1). Pinned after looking at peo_geo's image of it.
        constexpr std::uint64_t kPinned = 0xA4216A37C9F84008ULL;
        const GeographyRegion g = generate_geography(kSeed, GeographyParams{}, CellRect{-64, 936, 128, 128});
        std::uint64_t h = 0xCBF29CE484222325ULL; // FNV-1a
        const auto add = [&](std::uint64_t v) {
            for (int i = 0; i < 8; ++i) {
                h = (h ^ ((v >> (8 * i)) & 0xFFU)) * 0x100000001B3ULL;
            }
        };
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                add(static_cast<std::uint16_t>(g.elevation_m.at(x, y)));
                add(g.cover.at(x, y));
                add(g.body.at(x, y));
            }
        }
        MESSAGE("golden " << h);
        CHECK(h == kPinned);
    }
}

#ifdef NDEBUG
TEST_SUITE("scenario: geography") {
    // Release only (Ryan, 2026-10-08): a whole 100 km square and 48 sampled squares cost
    // about 2.5 s of Debug+ASan time on every push. The release job and verify --full
    // run them in full.
    TEST_CASE("a whole square fits in 8 MB and its bodies match its cover") {
        constexpr std::size_t kMaxBytes = 8U << 20U;
        const GeographyRegion& g = peo::test::seed1_square00(); // shared with the site golden
        MESSAGE("square (0,0): " << g.bytes() << " bytes, " << g.bodies.size() << " bodies");
        CHECK(g.bytes() <= kMaxBytes);
        check_bodies(g);
    }

    TEST_CASE("sea, forest and mountain shares sit in their bands") {
        constexpr int kStride = 10;
        constexpr int kBlock = 4;
        constexpr int kHighM = 1000;
        const GeographyParams p;
        std::int64_t samples = 0;
        std::int64_t sea = 0;
        std::int64_t land = 0;
        std::int64_t forest = 0;
        std::int64_t high = 0;
        int dry_squares = 0;
        int coastal_squares = 0;
        for (const Seed seed : {1ULL, 2ULL, 3ULL}) {
            for (int qy = -kBlock / 2; qy < kBlock / 2; ++qy) {
                for (int qx = -kBlock / 2; qx < kBlock / 2; ++qx) {
                    const CellRect r = square_rect({qx, qy});
                    std::int64_t square_sea = 0;
                    std::int64_t square_samples = 0;
                    for (std::int64_t y = r.y; y < r.y + r.height; y += kStride) {
                        for (std::int64_t x = r.x; x < r.x + r.width; x += kStride) {
                            const TerrainSample t = terrain_at(seed, p, x, y);
                            ++samples;
                            ++square_samples;
                            if (t.elevation_m < p.sea_level_m) {
                                ++sea;
                                ++square_sea;
                                continue;
                            }
                            ++land;
                            forest += t.forest ? 1 : 0;
                            high += t.elevation_m > kHighM ? 1 : 0;
                        }
                    }
                    dry_squares += square_sea == 0 ? 1 : 0;
                    coastal_squares += square_sea > 0 && square_sea < square_samples ? 1 : 0;
                }
            }
        }
        const double sea_share = static_cast<double>(sea) / static_cast<double>(samples);
        const double forest_share = static_cast<double>(forest) / static_cast<double>(land);
        const double high_share = static_cast<double>(high) / static_cast<double>(land);
        MESSAGE("sea " << sea_share << ", forest of land " << forest_share << ", above 1000 m " << high_share
                       << "; dry squares " << dry_squares << ", coastal " << coastal_squares);
        // Bands chosen from the peo_geo review of these seeds (sea 13.6%, forest 49.7% of
        // land, 14.3% of land above 1000 m when pinned), wide enough for a tuning nudge.
        CHECK(sea_share >= 0.08);
        CHECK(sea_share <= 0.20);
        CHECK(forest_share >= 0.40);
        CHECK(forest_share <= 0.60);
        CHECK(high_share >= 0.08);
        CHECK(high_share <= 0.22);
        CHECK(dry_squares > 0);
        CHECK(coastal_squares > 0);
    }
}
#endif
