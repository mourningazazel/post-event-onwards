#pragma once

// PEO-097: the Dwarf Fortress grid tileset layout (ADR-0015 point 3). One square PNG atlas
// holds 16 x 16 square glyphs in code page 437 order; CP437 keeps printable ASCII at its own
// byte values, so every glyph the game draws today indexes the atlas by that byte. SDL-free,
// so tests/core checks it headlessly; loading the PNG is the frontend's job.

#include <algorithm>
#include <optional>

namespace peo::app {

/// Glyphs along each side of a DF-grid atlas.
constexpr int kAtlasGlyphsPerSide = 16;
/// The on-screen cell ASCII mode already has (the 8 px debug font at 2x): smaller glyphs
/// scale up to reach it, so an 8 px atlas opens the same window.
constexpr int kTargetCellPx = 16;

/// A glyph's place in the atlas, in glyphs from the top-left.
struct AtlasCell {
    int col;
    int row;

    friend constexpr bool operator==(AtlasCell, AtlasCell) noexcept = default;
};

/// The glyph side of a `width_px` by `height_px` atlas, or nullopt when the atlas is not
/// square, not positive, or its side is not a multiple of kAtlasGlyphsPerSide.
[[nodiscard]] constexpr std::optional<int> atlas_glyph_px(int width_px, int height_px) noexcept {
    if (width_px <= 0 || width_px != height_px || width_px % kAtlasGlyphsPerSide != 0) {
        return std::nullopt;
    }
    return width_px / kAtlasGlyphsPerSide;
}

/// Where code page 437 glyph `cp437` sits in the atlas: row-major, 16 to a row.
[[nodiscard]] constexpr AtlasCell atlas_cell(unsigned char cp437) noexcept {
    return {cp437 % kAtlasGlyphsPerSide, cp437 / kAtlasGlyphsPerSide};
}

/// The whole-number scale that draws `glyph_px` glyphs nearest kTargetCellPx without
/// passing it, at least 1: 8 px draws at 2x as ASCII mode does, 16 px or larger at 1x.
[[nodiscard]] constexpr int display_scale(int glyph_px) noexcept {
    return glyph_px > 0 ? std::max(1, kTargetCellPx / glyph_px) : 1;
}

} // namespace peo::app
