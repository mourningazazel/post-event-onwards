#include "peo/core/rng.hpp"
#include "peo/core/settlement_site.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "geo_fixture.hpp"

using namespace peo::core;

namespace {
constexpr Seed kSeed = 1;
constexpr std::int16_t kFlatM = 50;

/// A hand-built region over `rect`: ground from `elevation`, cover bits from `cover`,
/// lake cells given body 1 (a lake keyed kLakeKey) and sea cells body 2 when present.
constexpr std::uint64_t kLakeKey = 0x1A4E0001ULL;
GeographyRegion hand_built(CellRect rect,
                           const std::function<std::int16_t(std::int64_t, std::int64_t)>& elevation,
                           const std::function<std::uint8_t(std::int64_t, std::int64_t)>& cover) {
    GeographyRegion g;
    g.rect = rect;
    g.elevation_m = Grid<std::int16_t>(rect.width, rect.height, 0);
    g.cover = Grid<std::uint8_t>(rect.width, rect.height, 0);
    g.body = Grid<std::uint16_t>(rect.width, rect.height, 0);
    WaterBody lake;
    lake.kind = WaterBody::Kind::Lake;
    lake.key = kLakeKey;
    g.bodies.push_back(lake);
    WaterBody sea;
    sea.kind = WaterBody::Kind::Sea;
    g.bodies.push_back(sea);
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const std::uint8_t c = cover(rect.x + x, rect.y + y);
            g.elevation_m.at(x, y) =
                (c & kCoverSea) != 0 ? std::int16_t{-20} : elevation(rect.x + x, rect.y + y);
            g.cover.at(x, y) = c;
            g.body.at(x, y) = (c & kCoverLake) != 0 ? 1 : (c & kCoverSea) != 0 ? 2 : 0;
        }
    }
    return g;
}

GeographyRegion flat_cell(const SiteParams& p, std::int64_t sx = 0, std::int64_t sy = 0) {
    return hand_built(
        site_cell_rect(p, sx, sy), [](auto, auto) { return kFlatM; },
        [](auto, auto) { return std::uint8_t{0}; });
}

SiteParams always(SiteParams p = {}) {
    p.site_chance_q16 = kNoiseOne;
    p.min_score = std::numeric_limits<std::int32_t>::min() / 4;
    return p;
}

SiteParams only(SiteSize s, SiteParams p = always()) {
    p.weight = {0, 0, 0, 0};
    p.weight[static_cast<std::size_t>(s)] = 1;
    return p;
}

/// Whether the disc of `s` holds a cell with `bit`, by a direct scan.
bool disc_has(const GeographyRegion& g, const SettlementSite& s, std::uint8_t bit) {
    const std::int64_t r = s.radius_cells;
    for (std::int64_t dy = -r; dy <= r; ++dy) {
        for (std::int64_t dx = -r; dx <= r; ++dx) {
            const std::int64_t x = s.cell_x + dx;
            const std::int64_t y = s.cell_y + dy;
            if (dx * dx + dy * dy <= r * r && x >= g.rect.x && y >= g.rect.y && x < g.rect.x + g.rect.width &&
                y < g.rect.y + g.rect.height &&
                (g.cover.at(static_cast<int>(x - g.rect.x), static_cast<int>(y - g.rect.y)) & bit) != 0) {
                return true;
            }
        }
    }
    return false;
}

bool same(const SettlementSite& a, const SettlementSite& b) {
    return a.key == b.key && a.site_x == b.site_x && a.site_y == b.site_y && a.cell_x == b.cell_x &&
           a.cell_y == b.cell_y && a.size == b.size && a.radius_cells == b.radius_cells &&
           a.elevation_m == b.elevation_m && a.landmark == b.landmark && a.water == b.water &&
           a.lake_key == b.lake_key && a.score == b.score;
}
} // namespace

TEST_SUITE("settlement sites") {
    TEST_CASE("valid() takes the defaults and refuses broken tilings, margins and counts") {
        const SiteParams d;
        CHECK(valid(d));
        SiteParams bad = d;
        bad.site_cells_per_side = 7; // 1000 / 7 is not whole
        CHECK_FALSE(valid(bad));
        bad = d;
        bad.margin_cells = d.radius_cells[3] - 1; // a city disc would leave its cell
        CHECK_FALSE(valid(bad));
        bad = d;
        bad.margin_cells = site_cell_side(d) / 2 + 1; // no room left to jitter
        CHECK_FALSE(valid(bad));
        bad = d;
        bad.candidates_per_cell = 0;
        CHECK_FALSE(valid(bad));
    }

    TEST_CASE("site cells tile the world from cell 0, each inside one square") {
        const SiteParams p;
        const std::int32_t side = site_cell_side(p);
        CHECK(side == 125);
        CHECK(site_cell_rect(p, 0, 0) == CellRect{0, 0, side, side});
        CHECK(site_cell_rect(p, 7, 7) == CellRect{875, 875, side, side});
        CHECK(site_cell_rect(p, 8, 0) == CellRect{1000, 0, side, side});
        CHECK(site_cell_rect(p, -1, 0) == CellRect{-125, 0, side, side});
        CHECK(site_cell_rect(p, 0, -1) == CellRect{0, -125, side, side});
        for (const std::int64_t s : {-9, -8, -1, 0, 7, 8, 15}) {
            const CellRect r = site_cell_rect(p, s, -s);
            CHECK(square_of(r.x, r.y) == square_of(r.x + r.width - 1, r.y + r.height - 1));
        }
    }

    TEST_CASE("candidates are a pure function of seed and cell, inside the margins") {
        const SiteParams p;
        const auto a = site_candidates(kSeed, p, 3, -2);
        CHECK(a.size() == static_cast<std::size_t>(p.candidates_per_cell));
        CHECK(std::equal(a.begin(), a.end(), site_candidates(kSeed, p, 3, -2).begin()));
        const auto b = site_candidates(kSeed + 1, p, 3, -2);
        CHECK_FALSE(std::equal(a.begin(), a.end(), b.begin()));
        const CellRect r = site_cell_rect(p, 3, -2);
        for (const CellCorner c : a) {
            CHECK(c.x >= r.x + p.margin_cells);
            CHECK(c.x < r.x + r.width - p.margin_cells);
            CHECK(c.y >= r.y + p.margin_cells);
            CHECK(c.y < r.y + r.height - p.margin_cells);
        }
    }

    TEST_CASE("flat land holds one plain site whose score is its best candidate's") {
        const SiteParams p = always();
        const GeographyRegion g = flat_cell(p);
        const auto sites = place_sites(kSeed, p, g);
        REQUIRE(sites.size() == 1);
        const SettlementSite& s = sites[0];
        CHECK(s.landmark == Landmark::None);
        CHECK(s.water == 0);
        CHECK(s.radius_cells == p.radius_cells[static_cast<std::size_t>(s.size)]);
        std::int32_t best = std::numeric_limits<std::int32_t>::min();
        for (const CellCorner c : site_candidates(kSeed, p, 0, 0)) {
            if (const auto v = score_site(p, g, c.x, c.y, s.radius_cells)) {
                best = std::max(best, *v);
            }
        }
        CHECK(s.score >= best);
        CHECK(s.score <= best + p.jitter_points);
        // The winner is one of the candidates, scored as score_site scores it plus its jitter.
        const auto candidates = site_candidates(kSeed, p, 0, 0);
        CHECK(std::any_of(candidates.begin(), candidates.end(),
                          [&](CellCorner c) { return c.x == s.cell_x && c.y == s.cell_y; }));
        const auto own = score_site(p, g, s.cell_x, s.cell_y, s.radius_cells);
        REQUIRE(own.has_value());
        CHECK(s.score - *own >= 0);
        CHECK(s.score - *own <= p.jitter_points);
    }

    TEST_CASE("the size draw follows the weights, and no chance means no site") {
        const SiteParams city = only(SiteSize::City);
        const GeographyRegion g = flat_cell(city);
        const auto sites = place_sites(kSeed, city, g);
        REQUIRE(sites.size() == 1);
        CHECK(sites[0].size == SiteSize::City);
        CHECK(sites[0].radius_cells == city.radius_cells[3]);
        SiteParams none = always();
        none.site_chance_q16 = 0;
        CHECK(place_sites(kSeed, none, g).empty());
    }

    TEST_CASE("an all-sea cell holds no site; a lake draws the site to its shore") {
        const SiteParams p = only(SiteSize::Town);
        const CellRect r = site_cell_rect(p, 0, 0);
        const GeographyRegion sea =
            hand_built(r, [](auto, auto) { return kFlatM; }, [](auto, auto) { return kCoverSea; });
        CHECK(place_sites(kSeed, p, sea).empty());
        // A 20 x 20 lake in the middle of flat land.
        const auto lake = [&](std::int64_t x, std::int64_t y) {
            return x >= 52 && x < 72 && y >= 52 && y < 72 ? kCoverLake : std::uint8_t{0};
        };
        const GeographyRegion g = hand_built(r, [](auto, auto) { return kFlatM; }, lake);
        const auto sites = place_sites(kSeed, p, g);
        REQUIRE(sites.size() == 1);
        const SettlementSite& s = sites[0];
        CHECK(lake(s.cell_x, s.cell_y) == 0);
        bool any_reaches = false;
        for (const CellCorner c : site_candidates(kSeed, p, 0, 0)) {
            SettlementSite probe = s;
            probe.cell_x = c.x;
            probe.cell_y = c.y;
            any_reaches = any_reaches || (lake(c.x, c.y) == 0 && disc_has(g, probe, kCoverLake));
        }
        REQUIRE(any_reaches);
        CHECK((s.water & kSiteLake) != 0);
        CHECK(s.landmark == Landmark::Lake);
        CHECK(s.lake_key == kLakeKey);
    }

    TEST_CASE("a sea edge draws the site to the coast; a slope loses to the flat") {
        const SiteParams p = only(SiteSize::City);
        const CellRect r = site_cell_rect(p, 0, 0);
        const GeographyRegion coast = hand_built(
            r, [](auto, auto) { return kFlatM; },
            [](auto x, auto) { return x < 12 ? kCoverSea : std::uint8_t{0}; });
        const auto at_sea = place_sites(kSeed, p, coast);
        REQUIRE(at_sea.size() == 1);
        CHECK((at_sea[0].water & kSiteSea) != 0);
        CHECK(at_sea[0].landmark == Landmark::Sea);

        const SiteParams hamlet = only(SiteSize::Hamlet);
        const std::int64_t half = r.x + r.width / 2;
        const GeographyRegion slope = hand_built(
            r, [&](auto x, auto) { return x < half ? static_cast<std::int16_t>(10 * (half - x)) : kFlatM; },
            [](auto, auto) { return std::uint8_t{0}; });
        const auto chosen = place_sites(kSeed, hamlet, slope);
        REQUIRE(chosen.size() == 1);
        CHECK(chosen[0].cell_x - chosen[0].radius_cells >= half);
    }

    TEST_CASE("score_site refuses water, height and a disc without enough land") {
        const SiteParams p;
        const CellRect r = site_cell_rect(p, 0, 0);
        const GeographyRegion g = hand_built(
            r, [](auto x, auto) { return x > 100 ? std::int16_t{3000} : kFlatM; },
            [](auto x, auto y) {
                return x < 30                                     ? kCoverSea
                       : (x >= 60 && x < 64 && y >= 60 && y < 64) ? kCoverLake
                                                                  : std::uint8_t{0};
            });
        CHECK_FALSE(score_site(p, g, 10, 60, 3).has_value());  // on the sea
        CHECK_FALSE(score_site(p, g, 61, 61, 3).has_value());  // on the lake
        CHECK_FALSE(score_site(p, g, 110, 60, 3).has_value()); // above max_site_elevation_m
        CHECK_FALSE(score_site(p, g, 31, 60, 15).has_value()); // near half its disc is sea
        CHECK(score_site(p, g, 50, 40, 3).has_value());
    }
}

TEST_SUITE("scenario: settlement sites") {
    TEST_CASE("a site is the same from any region that holds its cell") {
        const SiteParams p;
        const GeographyParams geo;
        for (const auto& [sx, sy] :
             {std::pair<std::int64_t, std::int64_t>{-1, -1}, std::pair<std::int64_t, std::int64_t>{7, 0}}) {
            const CellRect one = site_cell_rect(p, sx, sy);
            const CellRect block{one.x, one.y, 2 * one.width, 2 * one.height};
            const auto all = place_sites(kSeed, p, generate_geography(kSeed, geo, block));
            std::vector<SettlementSite> parts;
            for (std::int64_t dy = 0; dy < 2; ++dy) {
                for (std::int64_t dx = 0; dx < 2; ++dx) {
                    for (const SettlementSite& s : place_sites(
                             kSeed, p, generate_geography(kSeed, geo, site_cell_rect(p, sx + dx, sy + dy)))) {
                        parts.push_back(s);
                    }
                }
            }
            std::sort(parts.begin(), parts.end(), [](const SettlementSite& a, const SettlementSite& b) {
                return a.site_y != b.site_y ? a.site_y < b.site_y : a.site_x < b.site_x;
            });
            REQUIRE(all.size() == parts.size());
            for (std::size_t i = 0; i < all.size(); ++i) {
                CHECK(same(all[i], parts[i]));
            }
        }
#ifdef NDEBUG
        // And a whole square against its own cells, one site cell at a time. Release only
        // (Ryan, 2026-10-08): the whole seed-1 square costs about 1.6 s under Debug+ASan.
        const auto whole = place_sites(kSeed, p, peo::test::seed1_square00());
        for (const auto& [sx, sy] : {std::pair<std::int64_t, std::int64_t>{0, 0}, {3, 5}, {7, 7}, {6, 1}}) {
            const auto one = place_sites(kSeed, p, generate_geography(kSeed, geo, site_cell_rect(p, sx, sy)));
            const auto in_whole = std::find_if(whole.begin(), whole.end(), [&](const SettlementSite& s) {
                return s.site_x == sx && s.site_y == sy;
            });
            CAPTURE(sx);
            CAPTURE(sy);
            REQUIRE((in_whole != whole.end()) == !one.empty());
            if (!one.empty()) {
                CHECK(same(*in_whole, one.front()));
            }
        }
#endif
    }

    TEST_CASE("each site's water bits match its disc") {
        const SiteParams p = always();
        const CellRect r{-125, 875, 250, 250}; // four site cells across x = 0 and the square edge
        const GeographyRegion g = generate_geography(kSeed, GeographyParams{}, r);
        const auto sites = place_sites(kSeed, p, g);
        REQUIRE_FALSE(sites.empty());
        for (const SettlementSite& s : sites) {
            CHECK(((s.water & kSiteSea) != 0) == disc_has(g, s, kCoverSea));
            CHECK(((s.water & kSiteLake) != 0) == disc_has(g, s, kCoverLake));
        }
    }
#ifdef NDEBUG
    // Release only (Ryan, 2026-10-08): it reads the whole seed-1 square, as above. The
    // release job and verify --full run it in full.
    TEST_CASE("a square's sites, in order and inside their cells, pinned") {
        const SiteParams p;
        const GeographyRegion& g = peo::test::seed1_square00(); // shared with the geography case
        const auto sites = place_sites(kSeed, p, g);
        const std::int64_t per_square = std::int64_t{p.site_cells_per_side} * p.site_cells_per_side;
        CHECK(static_cast<std::int64_t>(sites.size()) <= per_square);
        std::uint64_t h = 0xCBF29CE484222325ULL; // FNV-1a over every field
        const auto add = [&](std::uint64_t v) {
            for (int i = 0; i < 8; ++i) {
                h = (h ^ ((v >> (8 * i)) & 0xFFU)) * 0x100000001B3ULL;
            }
        };
        for (std::size_t i = 0; i < sites.size(); ++i) {
            const SettlementSite& s = sites[i];
            if (i > 0) {
                const SettlementSite& b = sites[i - 1];
                CHECK((b.site_y < s.site_y || (b.site_y == s.site_y && b.site_x < s.site_x)));
            }
            const CellRect r = site_cell_rect(p, s.site_x, s.site_y);
            CHECK(s.cell_x - s.radius_cells >= r.x);
            CHECK(s.cell_x + s.radius_cells < r.x + r.width);
            CHECK(s.cell_y - s.radius_cells >= r.y);
            CHECK(s.cell_y + s.radius_cells < r.y + r.height);
            for (const std::uint64_t v :
                 {s.key, static_cast<std::uint64_t>(s.site_x), static_cast<std::uint64_t>(s.site_y),
                  static_cast<std::uint64_t>(s.cell_x), static_cast<std::uint64_t>(s.cell_y),
                  static_cast<std::uint64_t>(s.size), static_cast<std::uint64_t>(s.radius_cells),
                  static_cast<std::uint64_t>(static_cast<std::uint16_t>(s.elevation_m)),
                  static_cast<std::uint64_t>(s.landmark), static_cast<std::uint64_t>(s.water), s.lake_key,
                  static_cast<std::uint64_t>(static_cast<std::uint32_t>(s.score))}) {
                add(v);
            }
        }
        MESSAGE("square (0,0): " << sites.size() << " sites, golden " << h);
        constexpr std::uint64_t kPinned = 7587008700070664778ULL;
        CHECK(h == kPinned);
    }

    // Release only, as scent_wave's large-stage cases are: 192 site cells, each generated
    // alone, cost about 2.5 s under GCC 13's sanitizers (scenario budget, D-033; CI run
    // 37707993190). The release job and verify --full run it in full.
    TEST_CASE("site, coast and lake shares sit in their bands") {
        constexpr int kCells = 64;
        constexpr int kSpread = 400; // site cells drawn from [-kSpread, kSpread)
        const SiteParams p;
        const GeographyParams geo;
        int cells = 0;
        int sites = 0;
        int sea = 0;
        int lake = 0;
        int dry = 0;
        int sea_landmarks = 0;
        int lake_landmarks = 0;
        for (const Seed seed : {1ULL, 2ULL, 3ULL}) {
            for (int i = 0; i < kCells; ++i) {
                const std::uint64_t h = hash_u64(seed, 0x5A3D1EULL, static_cast<std::uint64_t>(i));
                const std::int64_t sx = static_cast<std::int64_t>(h % (2 * kSpread)) - kSpread;
                const std::int64_t sy = static_cast<std::int64_t>((h >> 32) % (2 * kSpread)) - kSpread;
                ++cells;
                for (const SettlementSite& s :
                     place_sites(seed, p, generate_geography(seed, geo, site_cell_rect(p, sx, sy)))) {
                    ++sites;
                    sea += (s.water & kSiteSea) != 0 ? 1 : 0;
                    lake += (s.water & kSiteLake) != 0 ? 1 : 0;
                    dry += s.water == 0 ? 1 : 0;
                    sea_landmarks += s.landmark == Landmark::Sea ? 1 : 0;
                    lake_landmarks += s.landmark == Landmark::Lake ? 1 : 0;
                }
            }
        }
        const double site_share = static_cast<double>(sites) / cells;
        const double sea_share = static_cast<double>(sea) / sites;
        const double lake_share = static_cast<double>(lake) / sites;
        const double dry_share = static_cast<double>(dry) / sites;
        MESSAGE("site share " << site_share << ", with sea " << sea_share << ", with lake " << lake_share
                              << ", no water " << dry_share << "; sea landmarks " << sea_landmarks
                              << ", lake landmarks " << lake_landmarks);
        // Bands from the peo_geo review: 0.20 of cells, 8% coast, 13% lake, 79% no water
        // over 64 cells a seed, each band about half as wide again as the reading. Most sites
        // have no water within their limits.
        CHECK(site_share >= 0.12);
        CHECK(site_share <= 0.30);
        CHECK(sea_share >= 0.03);
        CHECK(sea_share <= 0.15);
        CHECK(lake_share >= 0.05);
        CHECK(lake_share <= 0.22);
        CHECK(dry_share >= 0.65);
        CHECK(dry_share <= 0.90);
        CHECK(sea_landmarks > 0);
        CHECK(lake_landmarks > 0);
    }
#endif
}
