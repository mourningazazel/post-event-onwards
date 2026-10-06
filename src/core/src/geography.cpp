#include "peo/core/geography.hpp"

#include "peo/core/rng.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <optional>
#include <utility>

namespace peo::core {

namespace {
// Each layer draws from its own field: a hash of the world seed and a named salt.
constexpr std::uint64_t kContinentSalt = 0xC0A57ULL;
constexpr std::uint64_t kRangeSalt = 0x4A46EULL;
constexpr std::uint64_t kRangeMaskSalt = 0x4A46EAULL;
constexpr std::uint64_t kHillsSalt = 0x41115ULL;
constexpr std::uint64_t kForestSalt = 0xF0435ULL;
constexpr std::uint64_t kLakeSalt = 0x1A4EULL;
constexpr int kQ16 = 16;
constexpr std::uint64_t kQ16Mask = 0xFFFFU;

Seed layer(Seed world, std::uint64_t salt) noexcept {
    return hash_u64(world, salt, 0);
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

/// The one arithmetic of a cell's ground, over any evaluator of its layers (a point scan
/// or a row): `n` gives continent(), mask(), ridged(), hills() and forest() for the cell.
/// Range and forest are read only where they can matter.
template <typename Layers> TerrainSample terrain_from(const GeographyParams& p, Layers& n) noexcept {
    std::int64_t elevation = p.continent_bias_m + (std::int64_t{p.continent_m} * n.continent()) / kNoiseOne;
    // Ranges only where the mask is up, so mountains come in bands with lowland between.
    const std::int32_t mask = n.mask();
    if (mask > p.range_mask_none) {
        const std::int64_t m =
            std::min<std::int64_t>(kNoiseOne, (std::int64_t{mask - p.range_mask_none} << kQ16) /
                                                  (p.range_mask_full - p.range_mask_none));
        const std::int64_t ridge = (std::int64_t{n.ridged()} + kNoiseOne) / 2;
        const std::int64_t sq = (ridge * ridge) >> kQ16;
        const std::int64_t crest = (sq * sq) >> kQ16; // to the fourth: sharp crests, broad valleys
        elevation += (std::int64_t{p.range_m} * ((m * crest) >> kQ16)) >> kQ16;
    }
    elevation += (std::int64_t{p.hills_m} * n.hills()) / kNoiseOne;
    const auto e = static_cast<std::int16_t>(std::clamp<std::int64_t>(
        elevation, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
    const bool land = e >= p.sea_level_m;
    const bool forest = land && e < p.treeline_m && n.forest() > p.forest_threshold;
    return {.elevation_m = e, .forest = forest};
}

/// Layer seeds, derived once per scan.
struct LayerSeeds {
    Seed continent;
    Seed mask;
    Seed range;
    Seed hills;
    Seed forest;
    explicit LayerSeeds(Seed world) noexcept
        : continent(layer(world, kContinentSalt)), mask(layer(world, kRangeMaskSalt)),
          range(layer(world, kRangeSalt)), hills(layer(world, kHillsSalt)),
          forest(layer(world, kForestSalt)) {}
};

/// Cells one at a time, in any order, each layer's last lattice square kept: terrain_at,
/// and the lake discs.
class TerrainScan {
public:
    TerrainScan(Seed world, const GeographyParams& p) noexcept : p_(p), seeds_(world) {}

    TerrainSample sample(std::int64_t x, std::int64_t y) noexcept {
        x_ = x;
        y_ = y;
        return terrain_from(p_, *this);
    }
    std::int32_t continent() noexcept { return continent_.fbm(seeds_.continent, x_, y_, p_.continent); }
    std::int32_t mask() noexcept { return mask_.fbm(seeds_.mask, x_, y_, p_.range_mask); }
    std::int32_t ridged() noexcept { return range_.ridged(seeds_.range, x_, y_, p_.range); }
    std::int32_t hills() noexcept { return hills_.fbm(seeds_.hills, x_, y_, p_.hills); }
    std::int32_t forest() noexcept { return forest_.fbm(seeds_.forest, x_, y_, p_.forest); }

private:
    const GeographyParams& p_;
    LayerSeeds seeds_;
    std::int64_t x_ = 0;
    std::int64_t y_ = 0;
    FbmScan continent_;
    FbmScan mask_;
    FbmScan range_;
    FbmScan hills_;
    FbmScan forest_;
};

/// A rect's cells row by row (NoiseRow): each cell costs one vertical blend per octave.
/// The same terrain_from on the same layer values, so the same cells as terrain_at.
class TerrainRows {
public:
    TerrainRows(Seed world, const GeographyParams& p, std::int64_t x0, int width) : p_(p) {
        const LayerSeeds s(world);
        continent_.reset(s.continent, p.continent, x0, width);
        mask_.reset(s.mask, p.range_mask, x0, width);
        range_.reset(s.range, p.range, x0, width);
        hills_.reset(s.hills, p.hills, x0, width);
        forest_.reset(s.forest, p.forest, x0, width);
    }

    TerrainSample sample(int i, std::int64_t y) noexcept {
        i_ = i;
        y_ = y;
        return terrain_from(p_, *this);
    }
    std::int32_t continent() noexcept { return continent_.fbm(i_, y_); }
    std::int32_t mask() noexcept { return mask_.fbm(i_, y_); }
    std::int32_t ridged() noexcept { return range_.ridged(i_, y_); }
    std::int32_t hills() noexcept { return hills_.fbm(i_, y_); }
    std::int32_t forest() noexcept { return forest_.fbm(i_, y_); }

private:
    const GeographyParams& p_;
    int i_ = 0;
    std::int64_t y_ = 0;
    FbmRow continent_;
    FbmRow mask_;
    FbmRow range_;
    FbmRow hills_;
    FbmRow forest_;
};

/// A lake as its site leaves it: whole, from terrain_at alone, so every region that
/// touches it sees the same key, level and cells.
struct Lake {
    std::uint64_t key = 0;
    std::int16_t level = 0;
    std::vector<CellCorner> cells; // in scan order (y, then x)
    CellRect bounds{};
};

/// The site's lake, if it rolls one (PEO-095): the disc round its jittered centre, the
/// lowest cell near the centre, a hashed level above it, and the 4-connected cells below
/// that level; the level comes down a metre at a time (by halving) until the water
/// neither reaches the disc's rim nor touches the sea.
std::optional<Lake> lake_at_site(Seed world, const GeographyParams& p, std::int64_t gx, std::int64_t gy) {
    const std::uint64_t h =
        hash_u64(layer(world, kLakeSalt), static_cast<std::uint64_t>(gx), static_cast<std::uint64_t>(gy));
    if (static_cast<std::int64_t>(h & kQ16Mask) >= p.lake_chance_q16) {
        return std::nullopt;
    }
    const std::int64_t spacing = p.lake_site_spacing_cells;
    const int r = p.lake_max_radius_cells;
    const std::int64_t span = spacing - 2 * std::int64_t{r}; // valid() keeps it >= 1
    const std::int64_t cx =
        gx * spacing + r + static_cast<std::int64_t>(hash_u64(h, 1, 0) % static_cast<std::uint64_t>(span));
    const std::int64_t cy =
        gy * spacing + r + static_cast<std::int64_t>(hash_u64(h, 2, 0) % static_cast<std::uint64_t>(span));

    // The disc, its ground read once: index (dy + r) * side + (dx + r).
    const int side = 2 * r + 1;
    const auto r2 = std::int64_t{r} * r;
    const auto in_disc = [&](int dx, int dy) {
        return dx >= -r && dx <= r && dy >= -r && dy <= r &&
               std::int64_t{dx} * dx + std::int64_t{dy} * dy <= r2;
    };
    const auto at = [&](int dx, int dy) { return static_cast<std::size_t>((dy + r) * side + (dx + r)); };
    // The disc's ground, read only where the search and the floods look (most sites have
    // no basin, and most of a disc is never reached).
    std::vector<std::int16_t> ground_cache(static_cast<std::size_t>(side) * static_cast<std::size_t>(side),
                                           0);
    std::vector<std::uint8_t> known(ground_cache.size(), 0);
    TerrainScan scan(world, p);
    const auto ground_at = [&](int dx, int dy) {
        const std::size_t i = at(dx, dy);
        if (known[i] == 0) {
            ground_cache[i] = scan.sample(cx + dx, cy + dy).elevation_m;
            known[i] = 1;
        }
        return ground_cache[i];
    };
    const auto rim = [&](int dx, int dy) {
        return !in_disc(dx + 1, dy) || !in_disc(dx - 1, dy) || !in_disc(dx, dy + 1) || !in_disc(dx, dy - 1);
    };

    // The lowest cell within the seed radius; ties to the first in y, then x.
    const int seed_r = p.lake_seed_radius_cells;
    int lx = 0;
    int ly = 0;
    std::int16_t lowest = std::numeric_limits<std::int16_t>::max();
    for (int dy = -seed_r; dy <= seed_r; ++dy) {
        for (int dx = -seed_r; dx <= seed_r; ++dx) {
            if (std::int64_t{dx} * dx + std::int64_t{dy} * dy <= std::int64_t{seed_r} * seed_r &&
                ground_at(dx, dy) < lowest) {
                lowest = ground_at(dx, dy);
                lx = dx;
                ly = dy;
            }
        }
    }
    if (lowest <= p.sea_level_m) {
        return std::nullopt;
    }

    // The water below `level`, 4-connected from the lowest cell, inside the disc; empty
    // when it reaches the rim or a cell at or below sea level.
    std::vector<std::uint8_t> seen(ground_cache.size(), 0);
    std::vector<std::pair<int, int>> stack;
    std::vector<std::pair<int, int>> found;
    const auto flood = [&](std::int32_t level) {
        std::fill(seen.begin(), seen.end(), 0);
        found.clear();
        stack.assign(1, {lx, ly});
        seen[at(lx, ly)] = 1;
        while (!stack.empty()) {
            const auto [x, y] = stack.back();
            stack.pop_back();
            if (rim(x, y) || ground_at(x, y) <= p.sea_level_m) {
                found.clear();
                return false;
            }
            found.emplace_back(x, y);
            for (const Vec2i d : kNeighbours4) {
                const int nx = x + d.x;
                const int ny = y + d.y;
                if (in_disc(nx, ny) && seen[at(nx, ny)] == 0 && ground_at(nx, ny) < level) {
                    seen[at(nx, ny)] = 1;
                    stack.emplace_back(nx, ny);
                }
            }
        }
        return true;
    };
    const std::int32_t depth_span = p.lake_max_depth_m - p.lake_min_depth_m + 1;
    std::int32_t level = std::min<std::int32_t>(
        lowest + p.lake_min_depth_m +
            static_cast<std::int32_t>(hash_u64(h, 3, 0) % static_cast<std::uint64_t>(depth_span)),
        std::numeric_limits<std::int16_t>::max());
    if (!flood(level)) {
        // The highest whole-metre level in (lowest, level) whose water stays put: the water
        // only grows with the level, so its validity falls monotonically.
        std::int32_t lo = lowest + 1; // the lowest cell alone
        std::int32_t hi = level - 1;
        if (!flood(lo)) {
            return std::nullopt;
        }
        while (lo < hi) {
            const std::int32_t mid = lo + (hi - lo + 1) / 2;
            if (flood(mid)) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        level = lo;
        flood(level);
    }

    Lake lake;
    lake.key = h;
    lake.level = static_cast<std::int16_t>(level);
    std::sort(found.begin(), found.end(),
              [](auto a, auto b) { return a.second != b.second ? a.second < b.second : a.first < b.first; });
    std::int64_t x0 = std::numeric_limits<std::int64_t>::max();
    std::int64_t y0 = x0;
    std::int64_t x1 = std::numeric_limits<std::int64_t>::min();
    std::int64_t y1 = x1;
    for (const auto& [dx, dy] : found) {
        const CellCorner c{cx + dx, cy + dy};
        lake.cells.push_back(c);
        x0 = std::min(x0, c.x);
        y0 = std::min(y0, c.y);
        x1 = std::max(x1, c.x);
        y1 = std::max(y1, c.y);
    }
    lake.bounds = {x0, y0, static_cast<std::int32_t>(x1 - x0 + 1), static_cast<std::int32_t>(y1 - y0 + 1)};
    return lake;
}

/// The outline of a set of cells as closed rings of corners, the set on the left of
/// every step: outer rings come out with positive shoelace area, holes negative.
/// `cells` lists the set in scan order; `in(x, y)` says whether a cell belongs.
template <typename In>
std::vector<std::vector<CellCorner>> rings_of(const std::vector<CellCorner>& cells, In in) {
    struct Edge {
        CellCorner from;
        CellCorner to;
    };
    std::vector<Edge> edges;
    for (const CellCorner c : cells) {
        const std::int64_t x = c.x;
        const std::int64_t y = c.y;
        if (!in(x, y - 1)) {
            edges.push_back({{x, y}, {x + 1, y}}); // top, left to right
        }
        if (!in(x + 1, y)) {
            edges.push_back({{x + 1, y}, {x + 1, y + 1}}); // right, downward
        }
        if (!in(x, y + 1)) {
            edges.push_back({{x + 1, y + 1}, {x, y + 1}}); // bottom, right to left
        }
        if (!in(x - 1, y)) {
            edges.push_back({{x, y + 1}, {x, y}}); // left, upward
        }
    }
    // Edges by start corner, for the walk; ties keep generation order.
    std::vector<std::uint32_t> by_start(edges.size());
    for (std::uint32_t i = 0; i < by_start.size(); ++i) {
        by_start[i] = i;
    }
    const auto before = [&](CellCorner a, CellCorner b) { return a.y != b.y ? a.y < b.y : a.x < b.x; };
    std::stable_sort(by_start.begin(), by_start.end(),
                     [&](std::uint32_t a, std::uint32_t b) { return before(edges[a].from, edges[b].from); });
    std::vector<std::uint8_t> used(edges.size(), 0);
    std::vector<std::vector<CellCorner>> rings;
    for (std::uint32_t start = 0; start < edges.size(); ++start) {
        if (used[start] != 0) {
            continue;
        }
        std::vector<CellCorner> ring;
        std::uint32_t e = start;
        while (used[e] == 0) {
            used[e] = 1;
            ring.push_back(edges[e].from);
            // The next edge from where this one ends. At a pinch (two cells touching only
            // at a corner) two leave the corner: prefer the right turn while both are free.
            // A ring may still touch itself at a pinch; every edge is used once, so each
            // ring closes and the areas still sum to the cells.
            const CellCorner end = edges[e].to;
            auto it =
                std::lower_bound(by_start.begin(), by_start.end(), end,
                                 [&](std::uint32_t i, CellCorner c) { return before(edges[i].from, c); });
            std::optional<std::uint32_t> next;
            const std::int64_t ix = edges[e].to.x - edges[e].from.x;
            const std::int64_t iy = edges[e].to.y - edges[e].from.y;
            for (; it != by_start.end() && edges[*it].from == end; ++it) {
                if (used[*it] != 0) {
                    continue;
                }
                const std::int64_t ox = edges[*it].to.x - edges[*it].from.x;
                const std::int64_t oy = edges[*it].to.y - edges[*it].from.y;
                const bool right_turn = ix * oy - iy * ox > 0; // y down: positive cross is a right turn
                if (!next || right_turn) {
                    next = *it;
                }
            }
            if (!next) {
                break; // closed: back at the ring's start
            }
            e = *next;
        }
        rings.push_back(std::move(ring));
    }
    return rings;
}
} // namespace

SquareCoord square_of(std::int64_t cell_x, std::int64_t cell_y) noexcept {
    return {static_cast<std::int32_t>(floor_div(cell_x, kSquareCells)),
            static_cast<std::int32_t>(floor_div(cell_y, kSquareCells))};
}

CellRect square_rect(SquareCoord square) noexcept {
    return {std::int64_t{square.x} * kSquareCells, std::int64_t{square.y} * kSquareCells, kSquareCells,
            kSquareCells};
}

TerrainSample terrain_at(Seed world, const GeographyParams& p, std::int64_t x, std::int64_t y) noexcept {
    // One cell: the free noise functions, nothing kept, through the same terrain_from.
    struct Point {
        const GeographyParams& p;
        Seed world;
        std::int64_t x;
        std::int64_t y;
        std::int32_t continent() const noexcept {
            return fbm(layer(world, kContinentSalt), x, y, p.continent);
        }
        std::int32_t mask() const noexcept { return fbm(layer(world, kRangeMaskSalt), x, y, p.range_mask); }
        std::int32_t ridged() const noexcept {
            return peo::core::ridged(layer(world, kRangeSalt), x, y, p.range);
        }
        std::int32_t hills() const noexcept { return fbm(layer(world, kHillsSalt), x, y, p.hills); }
        std::int32_t forest() const noexcept { return fbm(layer(world, kForestSalt), x, y, p.forest); }
    };
    Point point{p, world, x, y};
    return terrain_from(p, point);
}

std::size_t GeographyRegion::bytes() const noexcept {
    std::size_t n = sizeof(*this) + elevation_m.size() * sizeof(std::int16_t) + cover.size() +
                    body.size() * sizeof(std::uint16_t);
    for (const WaterBody& b : bodies) {
        n += sizeof(WaterBody);
        for (const auto& ring : b.rings) {
            n += sizeof(ring) + ring.size() * sizeof(CellCorner);
        }
    }
    return n;
}

GeographyRegion generate_geography(Seed world, const GeographyParams& p, CellRect rect) {
    assert(valid(p) && rect.width >= 0 && rect.height >= 0);
    assert(std::abs(rect.x) < kMaxCellCoordinate && std::abs(rect.y) < kMaxCellCoordinate);
    GeographyRegion g;
    g.rect = rect;
    g.elevation_m = Grid<std::int16_t>(rect.width, rect.height, 0);
    g.cover = Grid<std::uint8_t>(rect.width, rect.height, 0);
    g.body = Grid<std::uint16_t>(rect.width, rect.height, 0);
    Grid<bool> forest(rect.width, rect.height, false);
    TerrainRows rows(world, p, rect.x, rect.width);
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const TerrainSample t = rows.sample(x, rect.y + y);
            g.elevation_m.at(x, y) = t.elevation_m;
            g.cover.at(x, y) = t.elevation_m < p.sea_level_m ? kCoverSea : 0;
            forest.at(x, y) = t.forest;
        }
    }
    const auto local = [&](std::int64_t wx, std::int64_t wy) -> std::optional<Vec2i> {
        const std::int64_t lx = wx - rect.x;
        const std::int64_t ly = wy - rect.y;
        if (lx < 0 || ly < 0 || lx >= rect.width || ly >= rect.height) {
            return std::nullopt;
        }
        return Vec2i{static_cast<int>(lx), static_cast<int>(ly)};
    };
    [[maybe_unused]] constexpr std::size_t kMaxBodies = std::numeric_limits<std::uint16_t>::max();

    // Lakes: every site whose grid square meets the rect (its disc lies inside it), rows
    // then columns, kept when one of its cells is in the rect.
    const std::int64_t spacing = p.lake_site_spacing_cells;
    for (std::int64_t gy = floor_div(rect.y, spacing); gy <= floor_div(rect.y + rect.height - 1, spacing);
         ++gy) {
        for (std::int64_t gx = floor_div(rect.x, spacing); gx <= floor_div(rect.x + rect.width - 1, spacing);
             ++gx) {
            std::optional<Lake> lake = lake_at_site(world, p, gx, gy);
            if (!lake) {
                continue;
            }
            const bool inside = std::any_of(lake->cells.begin(), lake->cells.end(),
                                            [&](CellCorner c) { return local(c.x, c.y).has_value(); });
            if (!inside) {
                continue;
            }
            assert(g.bodies.size() < kMaxBodies); // ids are uint16; a square holds tens
            const auto id = static_cast<std::uint16_t>(g.bodies.size() + 1);
            const auto& cells = lake->cells;
            const auto in = [&](std::int64_t x, std::int64_t y) {
                return std::binary_search(
                    cells.begin(), cells.end(), CellCorner{x, y},
                    [](CellCorner a, CellCorner b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
            };
            for (const CellCorner c : cells) {
                if (const auto l = local(c.x, c.y)) {
                    g.cover.at(*l) = kCoverLake;
                    g.body.at(*l) = id;
                }
            }
            g.bodies.push_back({.kind = WaterBody::Kind::Lake,
                                .key = lake->key,
                                .level_m = lake->level,
                                .cells = static_cast<std::uint32_t>(cells.size()),
                                .bounds = lake->bounds,
                                .rings = rings_of(cells, in)});
        }
    }

    // Seas: 4-connected sea components inside the rect, in scan order.
    std::vector<CellCorner> component;
    std::vector<Vec2i> stack;
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            if ((g.cover.at(x, y) & kCoverSea) == 0 || g.body.at(x, y) != 0) {
                continue;
            }
            assert(g.bodies.size() < kMaxBodies); // ids are uint16; a square holds tens
            const auto id = static_cast<std::uint16_t>(g.bodies.size() + 1);
            component.clear();
            stack.assign(1, {x, y});
            g.body.at(x, y) = id;
            std::int64_t x0 = x;
            std::int64_t y0 = y;
            std::int64_t x1 = x;
            std::int64_t y1 = y;
            while (!stack.empty()) {
                const Vec2i c = stack.back();
                stack.pop_back();
                component.push_back({rect.x + c.x, rect.y + c.y});
                x0 = std::min<std::int64_t>(x0, c.x);
                y0 = std::min<std::int64_t>(y0, c.y);
                x1 = std::max<std::int64_t>(x1, c.x);
                y1 = std::max<std::int64_t>(y1, c.y);
                for (const Vec2i d : kNeighbours4) {
                    const Vec2i n = c + d;
                    if (g.cover.in_bounds(n) && (g.cover.at(n) & kCoverSea) != 0 && g.body.at(n) == 0) {
                        g.body.at(n) = id;
                        stack.push_back(n);
                    }
                }
            }
            std::sort(component.begin(), component.end(),
                      [](CellCorner a, CellCorner b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
            const auto in = [&](std::int64_t wx, std::int64_t wy) {
                const auto l = local(wx, wy);
                return l && g.body.at(*l) == id;
            };
            g.bodies.push_back({.kind = WaterBody::Kind::Sea,
                                .key = 0,
                                .level_m = p.sea_level_m,
                                .cells = static_cast<std::uint32_t>(component.size()),
                                .bounds = {rect.x + x0, rect.y + y0, static_cast<std::int32_t>(x1 - x0 + 1),
                                           static_cast<std::int32_t>(y1 - y0 + 1)},
                                .rings = rings_of(component, in)});
        }
    }

    // Forest last: never under a lake or the sea.
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            if (forest.at(x, y) && g.cover.at(x, y) == 0) {
                g.cover.at(x, y) = kCoverForest;
            }
        }
    }
    return g;
}

GeographyRegion generate_square(Seed world, const GeographyParams& p, SquareCoord square) {
    return generate_geography(world, p, square_rect(square));
}

} // namespace peo::core
