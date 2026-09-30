#pragma once

// PEO-035: a cramped 200x120 town and three measuring helpers, ported from the
// pathfinding review's probe (docs/design/probes/probe.cpp) so PEO-030 and every
// later scent change are judged on houses, offices and a one-wide corridor, not
// only on the open stage. The draw order matches the probe, so Rng(11) gives the
// review's town. Unlike the probe, nothing here cuts a wall corner (PEO-044).

#include "peo/core/grid.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/types.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <vector>

namespace peo::test {

using core::Grid;
using core::Rng;
using core::Vec2i;

inline constexpr int kTownWidth = 200;
inline constexpr int kTownHeight = 120;
/// Streets every kBlockPitchX x kBlockPitchY cells, kStreetWidth wide; the rest of
/// each pitch is one building.
inline constexpr int kBlockPitchX = 24;
inline constexpr int kBlockPitchY = 20;
inline constexpr int kStreetWidth = 3;
/// Offices: a partition every kOfficeRoomPitch cells along the corridor, and each
/// room's door at most kOfficeDoorSpan cells past its first cell.
inline constexpr int kOfficeRoomPitch = 5;
inline constexpr int kOfficeDoorSpan = 2;
/// Moves a climb may take before it counts as lost (the probe's limit).
inline constexpr int kClimbLimit = 600;

/// The town: a border wall, and when `cramped` a grid of buildings between
/// streets. Buildings alternate: a house (four rooms off a cross partition, a
/// door in each arm, one front door) and an office (a one-wide central corridor,
/// rooms above and below it, each with a door onto it, one front door). true is
/// a wall. Without `cramped` it is the open map the review compared against.
inline Grid<bool> make_town(Rng& rng, bool cramped) {
    Grid<bool> b(kTownWidth, kTownHeight, false);
    for (int y = 0; y < kTownHeight; ++y) {
        for (int x = 0; x < kTownWidth; ++x) {
            b.at(x, y) = x == 0 || y == 0 || x == kTownWidth - 1 || y == kTownHeight - 1;
        }
    }
    if (!cramped) {
        return b;
    }
    int building = 0;
    for (int by = kStreetWidth; by + kBlockPitchY - kStreetWidth <= kTownHeight - 1; by += kBlockPitchY) {
        for (int bx = kStreetWidth; bx + kBlockPitchX - kStreetWidth <= kTownWidth - 1;
             bx += kBlockPitchX, ++building) {
            // Outer walls, inclusive corners.
            const int x0 = bx;
            const int y0 = by;
            const int x1 = bx + kBlockPitchX - kStreetWidth - 1;
            const int y1 = by + kBlockPitchY - kStreetWidth - 1;
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    b.at(x, y) = x == x0 || x == x1 || y == y0 || y == y1;
                }
            }
            // Front door on the top wall: drawn here to keep the probe's draw order,
            // carved after the partitions below.
            int front = x0 + 1 + rng.range(0, x1 - x0 - 2);
            const int my = (y0 + y1) / 2;
            if (building % 2 == 0) { // house: a cross partition, a door in each arm
                const int mx = (x0 + x1) / 2;
                front += front == mx ? 1 : 0; // see the note after the office branch
                for (int y = y0; y <= y1; ++y) {
                    b.at(mx, y) = true;
                }
                for (int x = x0; x <= x1; ++x) {
                    b.at(x, my) = true;
                }
                b.at(mx, y0 + 1 + rng.range(0, my - y0 - 2)) = false;
                b.at(mx, my + 1 + rng.range(0, y1 - my - 2)) = false;
                b.at(x0 + 1 + rng.range(0, mx - x0 - 2), my) = false;
                b.at(mx + 1 + rng.range(0, x1 - mx - 2), my) = false;
            } else { // office: corridor on row my, walled above and below
                for (int y = y0 + 1; y < y1; ++y) {
                    if (y == my) {
                        continue;
                    }
                    for (int x = x0 + 1; x < x1; ++x) {
                        b.at(x, y) = y == my - 1 || y == my + 1;
                    }
                }
                for (int x = x0 + kOfficeRoomPitch; x < x1; x += kOfficeRoomPitch) {
                    for (int y = y0 + 1; y < my - 1; ++y) {
                        b.at(x, y) = true;
                    }
                    for (int y = my + 2; y < y1; ++y) {
                        b.at(x, y) = true;
                    }
                }
                for (int x = x0 + 1; x < x1; x += kOfficeRoomPitch) { // a door into each room
                    const int dx = x + 1 + rng.range(0, std::min(kOfficeDoorSpan, x1 - x - 2));
                    b.at(dx, my - 1) = false;
                    b.at(dx, my + 1) = false;
                }
                front += (front - x0) % kOfficeRoomPitch == 0 ? 1 : 0;
            }
            // A front door drawn over a partition opened only diagonally past its end,
            // which the probe allowed and the game does not (PEO-044): it moves one cell
            // along, so every building has a way in. No extra draw.
            b.at(front, y0) = false;
        }
    }
    return b;
}

/// Whether one step from `from` by `d` is allowed: onto an open cell, and for a
/// diagonal only when both orthogonal cells beside it are open (PEO-044).
inline bool can_step(const Grid<bool>& b, Vec2i from, Vec2i d) {
    const auto open = [&](Vec2i c) { return b.in_bounds(c) && !b.at(c); };
    return open(from + d) &&
           (d.x == 0 || d.y == 0 || (open(from + Vec2i{d.x, 0}) && open(from + Vec2i{0, d.y})));
}

/// Row-major index of a cell in the helpers' distance vectors.
inline std::size_t cell_index(const Grid<bool>& b, Vec2i c) {
    return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(b.width()) +
           static_cast<std::size_t>(c.x);
}

/// Geodesic distance in moves from `source` to every cell (8-connected, no corner
/// cutting), row-major; -1 where unreachable.
inline std::vector<int> bfs_dist(const Grid<bool>& b, Vec2i source) {
    std::vector<int> d(static_cast<std::size_t>(b.width()) * static_cast<std::size_t>(b.height()), -1);
    std::deque<Vec2i> open{source};
    d[cell_index(b, source)] = 0;
    while (!open.empty()) {
        const Vec2i c = open.front();
        open.pop_front();
        for (const Vec2i step : core::kNeighbours8) {
            const Vec2i n = c + step;
            if (!can_step(b, c, step) || d[cell_index(b, n)] >= 0) {
                continue;
            }
            d[cell_index(b, n)] = d[cell_index(b, c)] + 1;
            open.push_back(n);
        }
    }
    return d;
}

/// How a field reads to strict strongest-neighbour climbers.
struct ClimbResult {
    /// Share of reachable cells from which the climb arrives at the goal.
    double success = 0.0;
    /// Mean moves taken over geodesic distance, over the climbs that arrived.
    double stretch = 0.0;
    /// Climbs that stopped on a cell with no stronger neighbour.
    int frozen = 0;
};

/// From every reachable open cell (dist > 0), walk to the strictly strongest
/// neighbour (no corner cutting) until the goal, a cell with none stronger, or
/// `limit` moves. `sample` is any callable Vec2i -> comparable value, so the float
/// field and PEO-030's integer field both fit.
template <typename Sample>
ClimbResult climb_all(const Grid<bool>& b, const std::vector<int>& dist, Vec2i goal, Sample sample,
                      int limit = kClimbLimit) {
    int reachable = 0;
    int arrived = 0;
    int frozen = 0;
    double stretch = 0.0;
    for (int y = 1; y < b.height() - 1; ++y) {
        for (int x = 1; x < b.width() - 1; ++x) {
            const Vec2i start{x, y};
            const int geodesic = dist[cell_index(b, start)];
            if (b.at(start) || geodesic <= 0) {
                continue;
            }
            ++reachable;
            Vec2i p = start;
            int moves = 0;
            bool stuck = false;
            while (p != goal && moves < limit) {
                auto best = sample(p);
                Vec2i next = p;
                for (const Vec2i step : core::kNeighbours8) {
                    if (!can_step(b, p, step)) {
                        continue;
                    }
                    const auto v = sample(p + step);
                    if (v > best) {
                        best = v;
                        next = p + step;
                    }
                }
                if (next == p) {
                    stuck = true;
                    break;
                }
                p = next;
                ++moves;
            }
            if (p == goal) {
                ++arrived;
                stretch += static_cast<double>(moves) / geodesic;
            } else if (stuck) {
                ++frozen;
            }
        }
    }
    return {.success = reachable > 0 ? static_cast<double>(arrived) / reachable : 0.0,
            .stretch = arrived > 0 ? stretch / arrived : 0.0,
            .frozen = frozen};
}

/// A shortest route from `from` to `to` by bfs_dist: the cells stepped onto, in
/// order, ending at `to`. Empty when `to` is unreachable or already reached.
inline std::vector<Vec2i> walk_path(const Grid<bool>& b, Vec2i from, Vec2i to) {
    const std::vector<int> dist = bfs_dist(b, to);
    std::vector<Vec2i> path;
    if (dist[cell_index(b, from)] < 0) {
        return path;
    }
    for (Vec2i p = from; p != to;) {
        for (const Vec2i step : core::kNeighbours8) {
            if (can_step(b, p, step) && dist[cell_index(b, p + step)] == dist[cell_index(b, p)] - 1) {
                p = p + step;
                break;
            }
        }
        path.push_back(p);
    }
    return path;
}

} // namespace peo::test
