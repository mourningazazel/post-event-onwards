// PEO-146: where a cell is on the one map (peo/core/chunk.hpp) and its placeholder terrain
// (peo/core/map_terrain.hpp).
#include "peo/core/chunk.hpp"
#include "peo/core/map_terrain.hpp"
#include "peo/core/wind.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>

using namespace peo::core;

namespace {

constexpr Seed kSeed = 46;
constexpr std::int64_t kFar = std::int64_t{1} << 40;

struct Fill {
    Grid<bool> blocked;
    Grid<std::uint8_t> openness;
};

Fill filled(int w, int h, WorldCell origin) {
    Fill f{Grid<bool>(w, h, false), Grid<std::uint8_t>(w, h, 0)};
    fill_terrain(kSeed, TerrainParams{}, origin, f.blocked, f.openness);
    return f;
}

} // namespace

TEST_SUITE("chunk") {
    TEST_CASE("chunk_of floors toward minus infinity") {
        CHECK(chunk_of({0, 0}) == ChunkCoord{0, 0});
        CHECK(chunk_of({31, 31}) == ChunkCoord{0, 0});
        CHECK(chunk_of({32, 0}) == ChunkCoord{1, 0});
        CHECK(chunk_of({-1, 0}) == ChunkCoord{-1, 0});
        CHECK(chunk_of({-32, 0}) == ChunkCoord{-1, 0});
        CHECK(chunk_of({-33, 0}) == ChunkCoord{-2, 0});
        CHECK(chunk_of({kFar, -kFar}) == ChunkCoord{kFar / kChunkSide, -kFar / kChunkSide});
        CHECK(chunk_of({kFar + 1, -kFar - 1}) == ChunkCoord{kFar / kChunkSide, -kFar / kChunkSide - 1});
    }

    TEST_CASE("chunk_origin and chunk_of round-trip") {
        for (const ChunkCoord c : {ChunkCoord{0, 0}, ChunkCoord{-1, 3}, ChunkCoord{kFar, -kFar}}) {
            const WorldCell o = chunk_origin(c);
            CHECK(chunk_of(o) == c);
            CHECK(chunk_of(o + Vec2i{static_cast<int>(kChunkSide - 1), static_cast<int>(kChunkSide - 1)}) ==
                  c);
            CHECK(chunk_of(o + Vec2i{-1, 0}) == ChunkCoord{c.x - 1, c.y});
        }
    }

    TEST_CASE("chunk_seed is stable and differs between neighbours") {
        CHECK(chunk_seed(kSeed, {0, 0}) == 14140079917046627962ULL);
        CHECK(chunk_seed(kSeed, {1, 0}) == 6865315434579281713ULL);
        CHECK(chunk_seed(kSeed, {0, 1}) != chunk_seed(kSeed, {1, 0}));
        CHECK(chunk_seed(kSeed + 1, {0, 0}) != chunk_seed(kSeed, {0, 0}));
    }

    TEST_CASE("a filled rect equals wall_at cell for cell") {
        // 70x50 from (-40, -20): crosses chunk lines and zero on both axes.
        const WorldCell origin{-40, -20};
        const Fill f = filled(70, 50, origin);
        for (int y = 0; y < 50; ++y) {
            for (int x = 0; x < 70; ++x) {
                REQUIRE(f.blocked.at(x, y) == wall_at(kSeed, TerrainParams{}, origin + Vec2i{x, y}));
            }
        }
        // As four quarters, in another order: the same grid.
        Fill q{Grid<bool>(70, 50, false), Grid<std::uint8_t>(70, 50, 0)};
        const TerrainParams p;
        fill_terrain(kSeed, p, origin, q.blocked, q.openness, 35, 25, 70, 50);
        fill_terrain(kSeed, p, origin, q.blocked, q.openness, 0, 0, 35, 25);
        fill_terrain(kSeed, p, origin, q.blocked, q.openness, 0, 25, 35, 50);
        fill_terrain(kSeed, p, origin, q.blocked, q.openness, 35, 0, 70, 25);
        CHECK(std::equal(q.blocked.begin(), q.blocked.end(), f.blocked.begin()));
        CHECK(std::equal(q.openness.begin(), q.openness.end(), f.openness.begin()));
    }

    TEST_CASE("overlapping fills agree") {
        // Two rects of the map sharing a strip: (0..40) and (30..70) along x.
        const Fill left = filled(40, 20, {-10, 5});
        const Fill right = filled(40, 20, {20, 5});
        for (int y = 0; y < 20; ++y) {
            for (int x = 30; x < 40; ++x) {
                REQUIRE(left.blocked.at(x, y) == right.blocked.at(x - 30, y));
            }
        }
    }

    TEST_CASE("walls are about a tenth of the map") {
        const Fill f = filled(256, 256, {-128, 4096});
        int walls = 0;
        for (const std::uint8_t cell : f.blocked) {
            walls += cell != 0 ? 1 : 0;
        }
        const double share = static_cast<double>(walls) / (256.0 * 256.0);
        CHECK(share > 0.095);
        CHECK(share < 0.105);
    }

    TEST_CASE("the placeholder generator is pinned") {
        // ADR-0002 freeze: change these only on purpose, with a note in the commit.
        const Fill f = filled(64, 64, {1000000, -1000000});
        int walls = 0;
        for (const std::uint8_t cell : f.blocked) {
            walls += cell != 0 ? 1 : 0;
        }
        CHECK(walls == 419);
        const TerrainParams p;
        CHECK_FALSE(wall_at(kSeed, p, {0, 0}));
        CHECK(wall_at(kSeed, p, {-1, -1}));
        CHECK(wall_at(kSeed, p, {10, 7}));
        CHECK_FALSE(wall_at(kSeed, p, {123456789, -987654321}));
    }

    TEST_CASE("openness follows the walls") {
        const Fill f = filled(64, 64, {-32, -32});
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                REQUIRE(f.openness.at(x, y) == (f.blocked.at(x, y) ? kOpennessIndoors : kOpennessOutdoors));
            }
        }
    }
}
