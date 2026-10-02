// PEO-088: the siege suite's buildings are what the brief describes. The only part of the
// suite in the regular tests; the runner (peo_siege) runs on demand.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <string>

#include "siege_layouts.hpp"

using namespace peo::siege;

namespace {
int count(const SiegeLayout& l, Region r) {
    int n = 0;
    for (const Region c : l.region) {
        n += c == r ? 1 : 0;
    }
    return n;
}
} // namespace

TEST_SUITE("siege layouts") {
    TEST_CASE("the three buildings, their setups and their player") {
        constexpr std::array<int, 3> kFloor{25, 32, 32};
        constexpr std::array<int, 3> kHallway{0, 4, 9};
        for (const Building b : kBuildings) {
            const auto bi = static_cast<std::size_t>(b);
            const std::string building = kBuildingNames[bi];
            CAPTURE(building);
            for (const Setup s : kSetups) {
                const std::string setup = kSetupNames[static_cast<std::size_t>(s)];
                CAPTURE(setup);
                const SiegeLayout l = build_layout(b, s);
                if (s == Setup::TwoOpenings) {
                    MESSAGE("building " << building << ", two openings:\n" << ascii(l));
                }
                CHECK(count(l, Region::WestRoom) + count(l, Region::EastRoom) == kFloor[bi]);
                CHECK(count(l, Region::Hallway) == kHallway[bi]);
                CHECK(count(l, Region::Opening) == static_cast<int>(s));
                // The player: the east room's (A: the room's) easternmost floor cell on row 2.
                const Region room = b == Building::A ? Region::WestRoom : Region::EastRoom;
                CHECK(l.region.at(l.player) == room);
                CHECK(l.region.at(l.player + Vec2i{1, 0}) != room);
                CHECK(l.region.at(l.player + Vec2i{0, -2}) == room);
                CHECK(l.region.at(l.player + Vec2i{0, -3}) != room);
                // Routes in: none enclosed; through each opening on its own otherwise.
                switch (s) {
                case Setup::Enclosed:
                    CHECK_FALSE(route_to_player(l));
                    break;
                case Setup::OneOpening:
                    CHECK(route_to_player(l));
                    CHECK_FALSE(route_to_player(l, l.openings));
                    break;
                case Setup::TwoOpenings:
                    CHECK(route_to_player(l, {l.openings[0]}));
                    CHECK(route_to_player(l, {l.openings[1]}));
                    CHECK_FALSE(route_to_player(l, l.openings));
                    break;
                }
                // Centred, inside the border, footprint indoors.
                // An even-sized building on the odd stage cannot sit exactly central: the
                // margins differ by at most one cell.
                CHECK(std::abs(l.x0 - (kStageSide - 1 - l.x1)) <= 1);
                CHECK(std::abs(l.y0 - (kStageSide - 1 - l.y1)) <= 1);
                CHECK(l.stage.openness.at(l.player) == peo::core::kOpennessIndoors);
                CHECK(l.stage.openness.at(1, 1) == peo::core::kOpennessOutdoors);
            }
        }
    }

    TEST_CASE("the densities place the Dead outside, one to a tile") {
        const SiegeLayout l = build_layout(Building::B, Setup::OneOpening);
        int ground = 0;
        for (int y = 1; y < kStageSide - 1; ++y) {
            for (int x = 1; x < kStageSide - 1; ++x) {
                ground += !l.stage.blocked.at(x, y) && !l.in_footprint({x, y}) ? 1 : 0;
            }
        }
        for (const Density d : kDensities) {
            const auto horde = place_horde(l, d, 7);
            CHECK(static_cast<int>(horde.size()) ==
                  ground * kDensityPercent[static_cast<std::size_t>(d)] / 100);
            Grid<std::uint8_t> on(kStageSide, kStageSide, 0);
            int clash = 0;
            for (const Dead& u : horde) {
                clash += (++on.at(u.pos) > 1 || l.in_footprint(u.pos) || l.stage.blocked.at(u.pos)) ? 1 : 0;
            }
            CHECK(clash == 0);
            CHECK(place_horde(l, d, 7)[0].pos == horde[0].pos); // from the seed
        }
    }
}
