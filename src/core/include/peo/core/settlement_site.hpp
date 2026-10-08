#pragma once

#include "peo/core/geography.hpp"
#include "peo/core/noise.hpp"
#include "peo/core/types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace peo::core {

/// Settlement sites on the geography (ADR-0018 point 3, PEO-096): at most one site per
/// site cell, an eighth of a square a side (125 cells, 12.5 km). Site cells tile the world
/// from cell 0 by floor division, so each lies inside one square. A site is chosen from
/// hashed candidate centres by an integer score and reads only its own site cell of the
/// region, so the same seed gives the same sites in any generation order (D-041).
enum class SiteSize : std::uint8_t { Hamlet, Village, Town, City };
inline constexpr std::size_t kSiteSizes = 4;
/// What a site sits by, the largest bonus it took (PEO-112 adds River).
enum class Landmark : std::uint8_t { None, Sea, Lake, Mountains };

/// Water within a site's limits.
inline constexpr std::uint8_t kSiteSea = 1;
inline constexpr std::uint8_t kSiteLake = 2;
/// Reserved for PEO-112: a river beside the site, and through it.
inline constexpr std::uint8_t kSiteRiverBesideReserved = 4;
inline constexpr std::uint8_t kSiteRiverThroughReserved = 8;

/// Every tunable, with its unit. Defaults picked from peo_geo images (PEO-096's note).
struct SiteParams {
    /// Site cells per square side: the side must divide kSquareCells.
    std::int32_t site_cells_per_side = 8;
    /// Share of site cells that hold a site, Q16 (owner: long empty stretches, towns about
    /// 20 km apart).
    std::int32_t site_chance_q16 = kNoiseOne * 7 / 20;
    std::int32_t candidates_per_cell = 16;
    /// Candidate centres jitter only within [margin, side - margin) of their cell, so a
    /// disc of up to margin cells stays inside it.
    std::int32_t margin_cells = 40;
    /// Per SiteSize: the radius in cells (0.3 to 3.5 km) and the draw weight.
    std::array<std::int32_t, kSiteSizes> radius_cells{3, 6, 15, 35};
    std::array<std::int32_t, kSiteSizes> weight{40, 35, 20, 5};
    /// No site centre above this.
    std::int32_t max_site_elevation_m = 1400;
    /// Share of the disc that must be land, Q16.
    std::int32_t min_land_q16 = kNoiseOne * 3 / 5;
    /// Points lost per metre between the lowest and highest land cell of the disc, and per
    /// metre the centre stands above sea level.
    std::int32_t relief_points_per_m = 2;
    std::int32_t altitude_points_per_m = 1;
    /// Points for sea or lake inside the disc, and for mountains in view: the highest land
    /// cell in the site cell at least mountain_rise_m above the centre.
    std::int32_t sea_bonus = 500;
    std::int32_t lake_bonus = 400;
    std::int32_t mountain_bonus = 150;
    std::int32_t mountain_rise_m = 900;
    /// A hashed tie-breaker in [0, jitter_points] added to each candidate's score.
    std::int32_t jitter_points = 60;
    /// The best candidate must score at least this, else the cell stays empty.
    std::int32_t min_score = -900;
};

/// The side of a site cell, in cells.
[[nodiscard]] constexpr std::int32_t site_cell_side(const SiteParams& p) noexcept {
    return p.site_cells_per_side > 0 ? kSquareCells / p.site_cells_per_side : 0;
}

/// Params place_sites can use: the side divides kSquareCells, the margin holds the
/// largest disc and leaves room to jitter, a candidate at least, weights not all zero
/// (none negative), Q16 shares inside [0, 1 << 16].
[[nodiscard]] constexpr bool valid(const SiteParams& p) noexcept {
    if (p.site_cells_per_side <= 0 || kSquareCells % p.site_cells_per_side != 0) {
        return false;
    }
    std::int32_t largest = 0;
    std::int64_t weights = 0;
    for (std::size_t i = 0; i < kSiteSizes; ++i) {
        if (p.radius_cells[i] < 0 || p.weight[i] < 0) {
            return false;
        }
        largest = largest > p.radius_cells[i] ? largest : p.radius_cells[i];
        weights += p.weight[i];
    }
    const std::int32_t side = site_cell_side(p);
    return p.margin_cells >= largest && 2 * p.margin_cells < side && p.candidates_per_cell >= 1 &&
           weights > 0 && p.site_chance_q16 >= 0 && p.site_chance_q16 <= kNoiseOne && p.min_land_q16 >= 0 &&
           p.min_land_q16 <= kNoiseOne && p.jitter_points >= 0 && p.jitter_points <= kNoiseOne;
}

/// A settled place on the map.
struct SettlementSite {
    std::uint64_t key = 0;
    std::int64_t site_x = 0; ///< its site cell
    std::int64_t site_y = 0;
    std::int64_t cell_x = 0; ///< its centre, world cells
    std::int64_t cell_y = 0;
    SiteSize size = SiteSize::Hamlet;
    std::int32_t radius_cells = 0;
    std::int16_t elevation_m = 0;
    Landmark landmark = Landmark::None;
    std::uint8_t water = 0; ///< kSiteSea, kSiteLake
    /// The first lake body (in the region's bodies order) with a cell in the disc; 0 none.
    std::uint64_t lake_key = 0;
    /// Bonuses less relief and altitude, plus the jitter. Worst case: bonuses below 2^31
    /// each, relief and altitude under 2^16 metres times the per-metre points; summed in
    /// int64 and clamped.
    std::int32_t score = 0;
};

/// The rect of site cell (site_x, site_y), in world cells.
[[nodiscard]] CellRect site_cell_rect(const SiteParams& params, std::int64_t site_x,
                                      std::int64_t site_y) noexcept;

/// The cell's candidate centres, candidates_per_cell of them, each a hash of a named salt,
/// the site cell and the candidate's index.
[[nodiscard]] std::vector<CellCorner> site_candidates(Seed world_seed, const SiteParams& params,
                                                      std::int64_t site_x, std::int64_t site_y);

/// A centre's score for a disc of radius_cells, from the region's cells inside its site
/// cell; none when the centre is sea or lake, above max_site_elevation_m, or the disc's
/// land share is below min_land_q16. The region must hold the site cell.
[[nodiscard]] std::optional<std::int32_t> score_site(const SiteParams& params, const GeographyRegion& region,
                                                     std::int64_t cell_x, std::int64_t cell_y,
                                                     std::int32_t radius_cells) noexcept;

/// The sites of every site cell wholly inside the region, in (site_y, site_x) order: the
/// cell's chance roll, a hashed size, each candidate scored at that size plus its jitter,
/// the best (ties to the lower index) kept when it reaches min_score. Pure; allocates only
/// the result. `params` must be valid().
[[nodiscard]] std::vector<SettlementSite> place_sites(Seed world_seed, const SiteParams& params,
                                                      const GeographyRegion& region);

} // namespace peo::core
