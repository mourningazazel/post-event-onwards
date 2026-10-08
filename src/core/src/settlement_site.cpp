#include "peo/core/settlement_site.hpp"

#include "peo/core/rng.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

namespace peo::core {

namespace {
// Every roll is a hash of the world seed, a named salt and the site cell.
constexpr std::uint64_t kSiteSalt = 0x517EULL;
constexpr std::uint64_t kChanceSalt = 0x517E0CULL;
constexpr std::uint64_t kSizeSalt = 0x517E512ULL;
constexpr std::uint64_t kCandidateSalt = 0x517EC4ULL;
constexpr std::uint64_t kJitterSalt = 0x517E117ULL;
constexpr std::uint64_t kQ16Mask = 0xFFFFU;
constexpr int kHalfWord = 32;

std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

std::uint64_t cell_hash(std::int64_t site_x, std::int64_t site_y) noexcept {
    return hash_u64(static_cast<std::uint64_t>(site_x), static_cast<std::uint64_t>(site_y), 0);
}

std::uint64_t roll(Seed world, std::uint64_t salt, std::int64_t site_x, std::int64_t site_y,
                   std::uint64_t index = 0) noexcept {
    return hash_u64(hash_u64(world, salt, index), cell_hash(site_x, site_y), 0);
}

/// What a disc holds: its cells (inside the site cell), land, sea and lake counts, the
/// land's lowest and highest elevation, and the first lake body by index.
struct Disc {
    std::int64_t cells = 0;
    std::int64_t land = 0;
    std::int64_t sea = 0;
    std::int64_t lake = 0;
    std::int16_t low = std::numeric_limits<std::int16_t>::max();
    std::int16_t high = std::numeric_limits<std::int16_t>::min();
    std::uint16_t first_lake = 0;
};

/// The region's cell at world (x, y), if it holds it.
bool holds(const GeographyRegion& g, std::int64_t x, std::int64_t y) noexcept {
    return x >= g.rect.x && y >= g.rect.y && x < g.rect.x + g.rect.width && y < g.rect.y + g.rect.height;
}
Vec2i local(const GeographyRegion& g, std::int64_t x, std::int64_t y) noexcept {
    return {static_cast<int>(x - g.rect.x), static_cast<int>(y - g.rect.y)};
}

bool inside(const CellRect& r, std::int64_t x, std::int64_t y) noexcept {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

Disc disc_of(const GeographyRegion& g, const CellRect& site, std::int64_t cx, std::int64_t cy,
             std::int32_t r) {
    Disc d;
    const std::int64_t r2 = std::int64_t{r} * r;
    for (std::int64_t dy = -r; dy <= r; ++dy) {
        for (std::int64_t dx = -r; dx <= r; ++dx) {
            const std::int64_t x = cx + dx;
            const std::int64_t y = cy + dy;
            if (dx * dx + dy * dy > r2 || !inside(site, x, y) || !holds(g, x, y)) {
                continue; // only the site cell is ever read
            }
            const Vec2i l = local(g, x, y);
            const std::uint8_t c = g.cover.at(l);
            ++d.cells;
            if ((c & kCoverSea) != 0) {
                ++d.sea;
            } else if ((c & kCoverLake) != 0) {
                ++d.lake;
                const std::uint16_t id = g.body.at(l);
                d.first_lake = d.first_lake == 0 ? id : std::min(d.first_lake, id);
            } else {
                ++d.land;
                d.low = std::min(d.low, g.elevation_m.at(l));
                d.high = std::max(d.high, g.elevation_m.at(l));
            }
        }
    }
    return d;
}

/// The highest land cell of the site cell: the mountains in view.
std::int16_t highest_land(const GeographyRegion& g, const CellRect& site) {
    std::int16_t high = std::numeric_limits<std::int16_t>::min();
    for (std::int64_t y = site.y; y < site.y + site.height; ++y) {
        for (std::int64_t x = site.x; x < site.x + site.width; ++x) {
            if (holds(g, x, y)) {
                const Vec2i l = local(g, x, y);
                if ((g.cover.at(l) & (kCoverSea | kCoverLake)) == 0) {
                    high = std::max(high, g.elevation_m.at(l));
                }
            }
        }
    }
    return high;
}

struct Scored {
    std::int64_t score = 0; // unclamped: the jitter is added before the one clamp
    Disc disc;
    bool mountains = false;
};

/// score_site with the site cell's highest land already found (it is the same for every
/// candidate of the cell).
std::optional<Scored> score_with(const SiteParams& p, const GeographyRegion& g, const CellRect& site,
                                 std::int16_t site_high, std::int64_t cx, std::int64_t cy, std::int32_t r) {
    if (!inside(site, cx, cy) || !holds(g, cx, cy)) {
        return std::nullopt;
    }
    const Vec2i centre = local(g, cx, cy);
    const std::int16_t e = g.elevation_m.at(centre);
    if ((g.cover.at(centre) & (kCoverSea | kCoverLake)) != 0 || e > p.max_site_elevation_m) {
        return std::nullopt;
    }
    Scored s;
    s.disc = disc_of(g, site, cx, cy, r);
    if (s.disc.cells == 0 || (s.disc.land << 16) < std::int64_t{p.min_land_q16} * s.disc.cells) {
        return std::nullopt;
    }
    s.mountains = site_high >= std::int64_t{e} + p.mountain_rise_m;
    std::int64_t score = std::int64_t{s.disc.sea > 0 ? p.sea_bonus : 0} +
                         std::int64_t{s.disc.lake > 0 ? p.lake_bonus : 0} +
                         std::int64_t{s.mountains ? p.mountain_bonus : 0};
    score -= std::int64_t{s.disc.high - s.disc.low} * p.relief_points_per_m;
    score -= std::int64_t{std::max<std::int16_t>(e, 0)} * p.altitude_points_per_m;
    s.score = score;
    return s;
}

/// A score as SettlementSite holds it: clamped well inside int32.
std::int32_t clamp_score(std::int64_t score) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        score, std::numeric_limits<std::int32_t>::min() / 2, std::numeric_limits<std::int32_t>::max() / 2));
}

/// Candidate `i` of a site cell: a hash of a named salt, the cell and the index. No
/// allocation, so place_sites allocates only its result.
CellCorner candidate(Seed world, const SiteParams& p, std::int64_t site_x, std::int64_t site_y,
                     std::int32_t i) noexcept {
    const std::int64_t side = site_cell_side(p);
    const auto span = static_cast<std::uint64_t>(side - 2 * p.margin_cells); // valid() keeps it >= 1
    const std::uint64_t h = roll(world, kCandidateSalt, site_x, site_y, static_cast<std::uint64_t>(i));
    return {site_x * side + p.margin_cells + static_cast<std::int64_t>((h & 0xFFFFFFFFULL) % span),
            site_y * side + p.margin_cells + static_cast<std::int64_t>((h >> kHalfWord) % span)};
}
} // namespace

CellRect site_cell_rect(const SiteParams& p, std::int64_t site_x, std::int64_t site_y) noexcept {
    const std::int32_t side = site_cell_side(p);
    return {site_x * side, site_y * side, side, side};
}

std::vector<CellCorner> site_candidates(Seed world, const SiteParams& p, std::int64_t site_x,
                                        std::int64_t site_y) {
    assert(valid(p));
    std::vector<CellCorner> out;
    out.reserve(static_cast<std::size_t>(p.candidates_per_cell));
    for (std::int32_t i = 0; i < p.candidates_per_cell; ++i) {
        out.push_back(candidate(world, p, site_x, site_y, i));
    }
    return out;
}

std::optional<std::int32_t> score_site(const SiteParams& p, const GeographyRegion& g, std::int64_t x,
                                       std::int64_t y, std::int32_t radius) noexcept {
    const std::int32_t side = site_cell_side(p);
    const CellRect site = site_cell_rect(p, floor_div(x, side), floor_div(y, side));
    // The mountains term reads the whole site cell, so the region must hold it all.
    assert(holds(g, site.x, site.y) && holds(g, site.x + site.width - 1, site.y + site.height - 1));
    const auto s = score_with(p, g, site, highest_land(g, site), x, y, radius);
    return s ? std::optional<std::int32_t>(clamp_score(s->score)) : std::nullopt;
}

std::vector<SettlementSite> place_sites(Seed world, const SiteParams& p, const GeographyRegion& g) {
    assert(valid(p));
    std::vector<SettlementSite> sites;
    const std::int64_t side = site_cell_side(p);
    const std::int64_t sx0 = -floor_div(-g.rect.x, side); // ceiling: the first cell wholly inside
    const std::int64_t sy0 = -floor_div(-g.rect.y, side);
    const std::int64_t sx1 = floor_div(g.rect.x + g.rect.width, side); // one past the last
    const std::int64_t sy1 = floor_div(g.rect.y + g.rect.height, side);
    std::int64_t total_weight = 0;
    for (const std::int32_t w : p.weight) {
        total_weight += w;
    }
    for (std::int64_t sy = sy0; sy < sy1; ++sy) {
        for (std::int64_t sx = sx0; sx < sx1; ++sx) {
            if (static_cast<std::int64_t>(roll(world, kChanceSalt, sx, sy) & kQ16Mask) >= p.site_chance_q16) {
                continue;
            }
            // The size: a hashed draw over the weights.
            std::int64_t pick = static_cast<std::int64_t>(roll(world, kSizeSalt, sx, sy) %
                                                          static_cast<std::uint64_t>(total_weight));
            std::size_t size = 0;
            while (pick >= p.weight[size]) {
                pick -= p.weight[size];
                ++size;
            }
            const std::int32_t radius = p.radius_cells[size];
            const CellRect site = site_cell_rect(p, sx, sy);
            const std::int16_t high = highest_land(g, site);
            std::optional<Scored> best;
            CellCorner at{};
            for (std::int32_t i = 0; i < p.candidates_per_cell; ++i) {
                const CellCorner c = candidate(world, p, sx, sy, i);
                std::optional<Scored> s = score_with(p, g, site, high, c.x, c.y, radius);
                if (!s) {
                    continue;
                }
                s->score += static_cast<std::int64_t>(
                    roll(world, kJitterSalt, sx, sy, static_cast<std::uint64_t>(i)) %
                    static_cast<std::uint64_t>(p.jitter_points + 1));
                if (!best || s->score > best->score) { // ties keep the lower index
                    best = s;
                    at = c;
                }
            }
            if (!best || clamp_score(best->score) < p.min_score) {
                continue;
            }
            SettlementSite out;
            out.key = hash_u64(world, kSiteSalt, cell_hash(sx, sy));
            out.site_x = sx;
            out.site_y = sy;
            out.cell_x = at.x;
            out.cell_y = at.y;
            out.size = static_cast<SiteSize>(size);
            out.radius_cells = radius;
            out.elevation_m = g.elevation_m.at(local(g, at.x, at.y));
            out.water = static_cast<std::uint8_t>((best->disc.sea > 0 ? kSiteSea : 0) |
                                                  (best->disc.lake > 0 ? kSiteLake : 0));
            out.lake_key = best->disc.first_lake != 0 ? g.bodies[best->disc.first_lake - 1U].key : 0;
            out.score = clamp_score(best->score);
            // The largest bonus it took; ties in the order Sea, Lake, Mountains.
            std::int32_t taken = 0;
            const auto consider = [&](bool took, std::int32_t bonus, Landmark l) {
                if (took && bonus > 0 && (out.landmark == Landmark::None || bonus > taken)) {
                    taken = bonus;
                    out.landmark = l;
                }
            };
            consider(best->disc.sea > 0, p.sea_bonus, Landmark::Sea);
            consider(best->disc.lake > 0, p.lake_bonus, Landmark::Lake);
            consider(best->mountains, p.mountain_bonus, Landmark::Mountains);
            sites.push_back(out);
        }
    }
    return sites;
}

} // namespace peo::core
