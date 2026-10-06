#pragma once

// PEO-088 (N022): the siege suite's buildings, opening setups and densities, as data built
// from the brief's rules. A building sits centred on a 141 x 141 stage with a border wall;
// every other cell is open ground, outdoors. Walls are one cell thick round every floor and
// hallway cell (the cells 8-adjacent to them), so no step cuts a corner into the building.
//
//   A: one room, 5 x 5 floor.
//   B: two 4 x 4 rooms, west and east; a hallway 1 wide and 4 long on floor row 2 between
//      doorways in the west room's east wall and the east room's west wall.
//   C: as B, the hallway 9 cells: 3 east, 3 south, 3 east, so the east room is 3 rows lower.
//
// Setups: enclosed (no gap in the outer walls), one opening (a gap in the west room's west
// wall on its floor row 2), two openings (and the mirror gap in the east room's east wall).
// The player stands on the east room's easternmost floor cell on row 2 in every setup.

#include "peo/core/dead.hpp"
#include "peo/core/grid.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/stage.hpp"
#include "peo/core/types.hpp"
#include "peo/core/wind.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace peo::siege {

using core::Dead;
using core::Grid;
using core::Rng;
using core::Seed;
using core::Stage;
using core::Vec2i;

/// 141, not 161: ADR-0016 doubled the updates per game hour, and PEO-088's budget ladder
/// (10 min on the x86 CI run) takes the stage down a step (PEO-090).
inline constexpr int kStageSide = 141;

enum class Building : std::uint8_t { A, B, C };
enum class Setup : std::uint8_t { Enclosed, OneOpening, TwoOpenings };
enum class Density : std::uint8_t { Sparse, Heavy };
inline constexpr std::array<Building, 3> kBuildings{Building::A, Building::B, Building::C};
inline constexpr std::array<Setup, 3> kSetups{Setup::Enclosed, Setup::OneOpening, Setup::TwoOpenings};
inline constexpr std::array<Density, 2> kDensities{Density::Sparse, Density::Heavy};
inline constexpr std::array<const char*, 3> kBuildingNames{"A", "B", "C"};
inline constexpr std::array<const char*, 3> kBuildingTitles{"A, one room", "B, two rooms and a hallway",
                                                            "C, two rooms and a bent hallway"};
inline constexpr std::array<const char*, 3> kSetupNames{"enclosed", "one-opening", "two-openings"};
inline constexpr std::array<const char*, 2> kDensityNames{"sparse", "heavy"};
/// The share of the outdoor open cells the Dead start on, in percent.
inline constexpr std::array<int, 2> kDensityPercent{5, 40};

/// What a cell of the stage is.
enum class Region : std::uint8_t { Ground, WestRoom, EastRoom, Hallway, Doorway, Opening, Wall };

struct SiegeLayout {
    Building building = Building::A;
    Setup setup = Setup::Enclosed;
    Stage stage;
    Grid<Region> region;
    Vec2i player{};
    /// The outer-wall gaps, west first.
    std::vector<Vec2i> openings;
    /// The footprint's box, walls included: [x0, x1] x [y0, y1].
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    [[nodiscard]] bool in_footprint(Vec2i c) const noexcept {
        return c.x >= x0 && c.x <= x1 && c.y >= y0 && c.y <= y1 && region.at(c) != Region::Ground;
    }
    /// Chebyshev distance from the footprint's box (0 inside it).
    [[nodiscard]] int distance_out(Vec2i c) const noexcept {
        const int dx = c.x < x0 ? x0 - c.x : (c.x > x1 ? c.x - x1 : 0);
        const int dy = c.y < y0 ? y0 - c.y : (c.y > y1 ? c.y - y1 : 0);
        return std::max(dx, dy);
    }
};

namespace detail {
struct Plan {
    std::vector<Vec2i> west_floor;
    std::vector<Vec2i> east_floor; // empty for A
    std::vector<Vec2i> hallway;
    std::vector<Vec2i> doorways;
    Vec2i west_gap;
    Vec2i east_gap;
    Vec2i player;
};

inline void room(std::vector<Vec2i>& out, int x0, int y0, int side) {
    for (int y = y0; y < y0 + side; ++y) {
        for (int x = x0; x < x0 + side; ++x) {
            out.push_back({x, y});
        }
    }
}

/// The building in its own coordinates: the west room's floor starts at (0, 0).
inline Plan plan(Building b) {
    constexpr int kRoomA = 5;
    constexpr int kRoomBC = 4;
    constexpr int kRow = 2; // the floor row the doors and gaps sit on
    Plan p;
    switch (b) {
    case Building::A:
        room(p.west_floor, 0, 0, kRoomA);
        p.west_gap = {-1, kRow};
        p.east_gap = {kRoomA, kRow};
        p.player = {kRoomA - 1, kRow};
        break;
    case Building::B:
        room(p.west_floor, 0, 0, kRoomBC);
        p.doorways = {{4, kRow}, {9, kRow}};
        for (int x = 5; x <= 8; ++x) {
            p.hallway.push_back({x, kRow});
        }
        room(p.east_floor, 10, 0, kRoomBC);
        p.west_gap = {-1, kRow};
        p.east_gap = {14, kRow};
        p.player = {13, kRow};
        break;
    case Building::C:
        room(p.west_floor, 0, 0, kRoomBC);
        p.doorways = {{4, kRow}, {11, kRow + 3}};
        p.hallway = {{5, 2}, {6, 2}, {7, 2}, {7, 3}, {7, 4}, {7, 5}, {8, 5}, {9, 5}, {10, 5}};
        room(p.east_floor, 12, 3, kRoomBC);
        p.west_gap = {-1, kRow};
        p.east_gap = {16, kRow + 3};
        p.player = {15, kRow + 3};
        break;
    }
    return p;
}
} // namespace detail

/// The layout for `building` and `setup`, centred on the stage.
inline SiegeLayout build_layout(Building building, Setup setup) {
    const detail::Plan p = detail::plan(building);
    SiegeLayout l;
    l.building = building;
    l.setup = setup;
    // The box of floor and hallway, then one cell of wall round it.
    int bx0 = 0;
    int by0 = 0;
    int bx1 = 0;
    int by1 = 0;
    for (const auto* cells : {&p.west_floor, &p.east_floor, &p.hallway}) {
        for (const Vec2i c : *cells) {
            bx0 = std::min(bx0, c.x);
            by0 = std::min(by0, c.y);
            bx1 = std::max(bx1, c.x);
            by1 = std::max(by1, c.y);
        }
    }
    --bx0;
    --by0;
    ++bx1;
    ++by1;
    const Vec2i off{(kStageSide - (bx1 - bx0 + 1)) / 2 - bx0, (kStageSide - (by1 - by0 + 1)) / 2 - by0};
    l.x0 = bx0 + off.x;
    l.y0 = by0 + off.y;
    l.x1 = bx1 + off.x;
    l.y1 = by1 + off.y;

    l.region = Grid<Region>(kStageSide, kStageSide, Region::Ground);
    const auto mark = [&](const std::vector<Vec2i>& cells, Region r) {
        for (const Vec2i c : cells) {
            l.region.at(c + off) = r;
        }
    };
    mark(p.west_floor, Region::WestRoom);
    mark(p.east_floor, Region::EastRoom);
    mark(p.hallway, Region::Hallway);
    mark(p.doorways, Region::Doorway);
    // Walls: every ground cell 8-adjacent to floor or hallway.
    for (int y = l.y0; y <= l.y1; ++y) {
        for (int x = l.x0; x <= l.x1; ++x) {
            if (l.region.at(x, y) != Region::Ground) {
                continue;
            }
            for (const Vec2i d : core::kNeighbours8) {
                const Region n = l.region.at(Vec2i{x, y} + d);
                if (n == Region::WestRoom || n == Region::EastRoom || n == Region::Hallway) {
                    l.region.at(x, y) = Region::Wall;
                    break;
                }
            }
        }
    }
    if (setup != Setup::Enclosed) {
        l.openings.push_back(p.west_gap + off);
    }
    if (setup == Setup::TwoOpenings) {
        l.openings.push_back(p.east_gap + off);
    }
    for (const Vec2i o : l.openings) {
        l.region.at(o) = Region::Opening;
    }
    l.player = p.player + off;

    l.stage.blocked = Grid<bool>(kStageSide, kStageSide, false);
    l.stage.openness = Grid<std::uint8_t>(kStageSide, kStageSide, core::kOpennessOutdoors);
    for (int y = 0; y < kStageSide; ++y) {
        for (int x = 0; x < kStageSide; ++x) {
            const bool border = x == 0 || y == 0 || x == kStageSide - 1 || y == kStageSide - 1;
            l.stage.blocked.at(x, y) = border || l.region.at(x, y) == Region::Wall;
            if (l.in_footprint({x, y})) {
                l.stage.openness.at(x, y) = core::kOpennessIndoors;
            }
        }
    }
    l.stage.entry = l.player;
    l.stage.exit = core::kNoExit;
    return l;
}

/// The horde for `density`: that share of the outdoor open cells, drawn uniformly from
/// `seed`, never in the footprint; each a speed as World::load_stage draws one.
inline std::vector<Dead> place_horde(const SiegeLayout& l, Density density, Seed seed,
                                     core::Substeps update_period = core::kUpdatePeriodSubsteps) {
    constexpr int kPercent = 100;
    constexpr int kMinSpeed = 1; // updates per step, as load_stage's kMinDeadSpeed..kMaxDeadSpeed
    constexpr int kMaxSpeed = 3;
    std::vector<Vec2i> ground;
    for (int y = 1; y < kStageSide - 1; ++y) {
        for (int x = 1; x < kStageSide - 1; ++x) {
            if (!l.stage.blocked.at(x, y) && !l.in_footprint({x, y})) {
                ground.push_back({x, y});
            }
        }
    }
    const std::size_t count = ground.size() *
                              static_cast<std::size_t>(kDensityPercent[static_cast<std::size_t>(density)]) /
                              kPercent;
    Rng rng(seed);
    // A partial Fisher-Yates shuffle: the first `count` cells, each drawn uniformly.
    for (std::size_t i = 0; i < count; ++i) {
        const auto j = i + static_cast<std::size_t>(rng.range(0, static_cast<int>(ground.size() - i) - 1));
        std::swap(ground[i], ground[j]);
    }
    std::vector<Dead> horde;
    horde.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        horde.push_back({.pos = ground[i],
                         .step_substeps = static_cast<std::uint16_t>(
                             static_cast<core::Substeps>(rng.range(kMinSpeed, kMaxSpeed)) * update_period)});
    }
    return horde;
}

/// The building's box and one cell round it as text: # wall, . floor or ground, @ player.
inline std::string ascii(const SiegeLayout& l) {
    std::string out;
    for (int y = l.y0 - 1; y <= l.y1 + 1; ++y) {
        for (int x = l.x0 - 1; x <= l.x1 + 1; ++x) {
            out += Vec2i{x, y} == l.player ? '@' : (l.stage.blocked.at(x, y) ? '#' : '.');
        }
        out += '\n';
    }
    return out;
}

/// Whether a walk (8-connected, no corner cutting, PEO-044) leads from the stage's open
/// ground at (1, 1) to the player, with the cells in `closed` treated as walls.
inline bool route_to_player(const SiegeLayout& l, const std::vector<Vec2i>& closed = {}) {
    Grid<bool> blocked = l.stage.blocked;
    for (const Vec2i c : closed) {
        blocked.at(c) = true;
    }
    Grid<bool> seen(kStageSide, kStageSide, false);
    std::deque<Vec2i> todo{{1, 1}};
    seen.at(1, 1) = true;
    while (!todo.empty()) {
        const Vec2i c = todo.front();
        todo.pop_front();
        if (c == l.player) {
            return true;
        }
        for (const Vec2i d : core::kNeighbours8) {
            const Vec2i n = c + d;
            const bool diagonal = d.x != 0 && d.y != 0;
            if (blocked.at(n) || seen.at(n) ||
                (diagonal && (blocked.at(c.x + d.x, c.y) || blocked.at(c.x, c.y + d.y)))) {
                continue;
            }
            seen.at(n) = true;
            todo.push_back(n);
        }
    }
    return false;
}

} // namespace peo::siege
