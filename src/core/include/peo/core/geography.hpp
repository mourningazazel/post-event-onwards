#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/noise.hpp"
#include "peo/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace peo::core {

/// The geography map (ADR-0018, PEO-095): coarse cells of 100 m (100 tiles of 1 m,
/// ADR-0013), in 100 km squares of 1000 x 1000 cells. Every cell is a pure function of the
/// world seed and its world cell coordinates (terrain_at), so squares join without seams
/// in any order (D-041 A, endless).
inline constexpr std::int32_t kGeoCellTiles = 100;
inline constexpr std::int32_t kSquareCells = 1000;

struct SquareCoord {
    std::int32_t x = 0;
    std::int32_t y = 0;
    friend bool operator==(const SquareCoord&, const SquareCoord&) = default;
};
/// A rectangle of world cells: its top-left cell and its size.
struct CellRect {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    friend bool operator==(const CellRect&, const CellRect&) = default;
};
/// The square a world cell lies in (floor division: cell -1 is in square -1).
[[nodiscard]] SquareCoord square_of(std::int64_t cell_x, std::int64_t cell_y) noexcept;
[[nodiscard]] CellRect square_rect(SquareCoord square) noexcept;

/// Every tunable of the map, with its unit. The defaults were picked by reading peo_geo
/// images (PEO-095's note says what and why).
struct GeographyParams {
    std::int16_t sea_level_m = 0;
    /// Continents and seas: a long wavelength (2^11 cells, about 200 km), so coasts and
    /// seas run across squares. Elevation = continent_bias_m + continent_m x fbm.
    Octaves continent{.first_wavelength_log2 = 11, .count = 5, .gain_q16 = kNoiseOne / 2};
    std::int32_t continent_bias_m = 60;
    std::int32_t continent_m = 700;
    /// Ranges: ridged noise, squared for sharp crests, inside a low-frequency mask so
    /// mountains come in bands. The mask is full above range_mask_full and none below
    /// range_mask_none (both Q16 fbm values).
    Octaves range{.first_wavelength_log2 = 9, .count = 4, .gain_q16 = kNoiseOne / 2};
    Octaves range_mask{.first_wavelength_log2 = 12, .count = 2, .gain_q16 = kNoiseOne / 2};
    std::int32_t range_mask_none = kNoiseOne / 4;
    std::int32_t range_mask_full = kNoiseOne * 3 / 5;
    std::int32_t range_m = 1800;
    /// Hills everywhere: a gentle fbm.
    Octaves hills{.first_wavelength_log2 = 7, .count = 5, .gain_q16 = kNoiseOne * 3 / 5};
    std::int32_t hills_m = 160;
    /// Forest: where this fbm is above forest_threshold (Q16), on land below treeline_m.
    Octaves forest{.first_wavelength_log2 = 8, .count = 4, .gain_q16 = kNoiseOne / 2};
    std::int32_t forest_threshold = 0;
    std::int32_t treeline_m = 1700;
    /// Lakes: one candidate site per lake_site_spacing_cells square of the grid, jittered
    /// so its lake_max_radius_cells disc stays inside its grid square (no two discs
    /// overlap); it rolls a lake with lake_chance_q16. The lake fills the hollow round the
    /// lowest cell within lake_seed_radius_cells up to a hashed depth, never spilling.
    std::int32_t lake_site_spacing_cells = 64;
    std::int32_t lake_chance_q16 = kNoiseOne * 3 / 5;
    std::int32_t lake_seed_radius_cells = 16;
    std::int32_t lake_max_radius_cells = 24;
    std::int16_t lake_min_depth_m = 4;
    std::int16_t lake_max_depth_m = 30;
};

/// The largest lake disc radius: it sizes a scratch square per site.
inline constexpr std::int32_t kMaxLakeRadiusCells = 1024;
/// The cells the map is defined over: |x| and |y| below this, so every sum near them
/// (lattice + 1, a rect's far edge, a ring corner) stays far inside int64.
inline constexpr std::int64_t kMaxCellCoordinate = std::int64_t{1} << 40;

/// Params the generator can use: every Octaves valid, the range band ordered inside the
/// noise's range, and lake discs bounded that cannot overlap (spacing at least twice the
/// max radius plus one) or miss their seed.
[[nodiscard]] constexpr bool valid(const GeographyParams& p) noexcept {
    return valid(p.continent) && valid(p.range) && valid(p.range_mask) && valid(p.hills) && valid(p.forest) &&
           p.range_mask_none >= -kNoiseOne && p.range_mask_full <= kNoiseOne &&
           p.range_mask_full > p.range_mask_none && p.lake_max_radius_cells > 0 &&
           p.lake_max_radius_cells <= kMaxLakeRadiusCells && p.lake_seed_radius_cells >= 0 &&
           p.lake_seed_radius_cells <= p.lake_max_radius_cells &&
           p.lake_site_spacing_cells > 2 * p.lake_max_radius_cells && p.lake_chance_q16 >= 0 &&
           p.lake_chance_q16 <= kNoiseOne && p.lake_min_depth_m >= 1 &&
           p.lake_max_depth_m >= p.lake_min_depth_m;
}

/// One cell of ground: its elevation (lake beds included) and whether forest grows on it.
/// Sea is exactly elevation_m < sea_level_m; forest is only ever on land below treeline_m
/// (lakes are found later, by generate_geography, and clear forest under them).
struct TerrainSample {
    std::int16_t elevation_m = 0;
    bool forest = false;
};
/// The source of truth: any region's elevation and forest come from here.
[[nodiscard]] TerrainSample terrain_at(Seed world_seed, const GeographyParams& params, std::int64_t cell_x,
                                       std::int64_t cell_y) noexcept;

/// A corner of the cell grid, in world cells (cell (x, y) spans corners x..x+1, y..y+1).
struct CellCorner {
    std::int64_t x = 0;
    std::int64_t y = 0;
    friend bool operator==(const CellCorner&, const CellCorner&) = default;
};

/// A lake or a sea, with its outline as polygons (ADR-0018 point 2). A lake is whole and
/// keyed by its site, so it comes out the same from every region it touches. A sea is a
/// 4-connected sea component inside one region (key 0), its rings running along the
/// region's edge where it reaches it.
struct WaterBody {
    enum class Kind : std::uint8_t { Sea, Lake };
    Kind kind = Kind::Sea;
    std::uint64_t key = 0;
    std::int16_t level_m = 0;
    std::uint32_t cells = 0;
    CellRect bounds{};
    /// Closed rings of cell corners: each point one axis step from the next, the last one
    /// step from the first. Outer rings have positive signed area (the shoelace sum in
    /// world cell coordinates, y down), island holes negative.
    std::vector<std::vector<CellCorner>> rings;
};

/// Cover bits per cell.
inline constexpr std::uint8_t kCoverSea = 1;
inline constexpr std::uint8_t kCoverLake = 2;
inline constexpr std::uint8_t kCoverForest = 4;
/// Reserved for river courses (PEO-111).
inline constexpr std::uint8_t kCoverRiverReserved = 8;

/// A generated piece of the map: ground elevation, cover bits, and for each water cell
/// the body it belongs to (0 none, else index + 1 into bodies).
struct GeographyRegion {
    CellRect rect{};
    Grid<std::int16_t> elevation_m;
    Grid<std::uint8_t> cover;
    Grid<std::uint16_t> body;
    std::vector<WaterBody> bodies;
    [[nodiscard]] std::size_t bytes() const noexcept;
};

/// The region `rect` of the map of `world_seed`: every cell equals terrain_at, lakes from
/// every site whose disc meets the rect (whole, in site order), then the sea components
/// inside the rect in scan order. Pure: no globals, no I/O, no Rng stream. It allocates;
/// it runs once per square, far ahead of the player, never per update. `params` must be
/// valid(), the rect's size non-negative and inside ±kMaxCellCoordinate, and it holds at
/// most 65,535 water bodies (a square holds tens): all asserted, as ScentWave asserts.
[[nodiscard]] GeographyRegion generate_geography(Seed world_seed, const GeographyParams& params,
                                                 CellRect rect);
[[nodiscard]] GeographyRegion generate_square(Seed world_seed, const GeographyParams& params,
                                              SquareCoord square);

} // namespace peo::core
