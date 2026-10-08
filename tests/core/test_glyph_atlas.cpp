// PEO-097: the DF-grid tileset layout (src/app/glyph_atlas.hpp, header-only, no SDL).
#include <doctest/doctest.h>

#include "glyph_atlas.hpp"

using namespace peo::app;

namespace {

// The constexpr contract: the frontend may size and index an atlas at compile time.
static_assert(atlas_glyph_px(256, 256) == 16);
static_assert(atlas_cell('@') == AtlasCell{0, 4});

} // namespace

TEST_SUITE("glyph_atlas") {
    TEST_CASE("a square atlas whose side divides by 16 gives its glyph side") {
        CHECK(atlas_glyph_px(128, 128) == 8);
        CHECK(atlas_glyph_px(256, 256) == 16);
        CHECK(atlas_glyph_px(192, 192) == 12);
    }

    TEST_CASE("any other atlas is refused") {
        CHECK_FALSE(atlas_glyph_px(0, 0).has_value());
        CHECK_FALSE(atlas_glyph_px(100, 100).has_value()); // not a multiple of 16
        CHECK_FALSE(atlas_glyph_px(128, 192).has_value()); // not square
        CHECK_FALSE(atlas_glyph_px(-16, -16).has_value());
    }

    TEST_CASE("a code page 437 byte sits at column code % 16, row code / 16") {
        CHECK(atlas_cell('@') == AtlasCell{0, 4});
        CHECK(atlas_cell('#') == AtlasCell{3, 2});
        CHECK(atlas_cell(0) == AtlasCell{0, 0});
        CHECK(atlas_cell(255) == AtlasCell{15, 15});
    }

    TEST_CASE("small glyphs scale up to the 16 px cell, larger ones draw at 1x") {
        CHECK(display_scale(8) == 2);
        CHECK(display_scale(16) == 1);
        CHECK(display_scale(32) == 1);
        CHECK(display_scale(12) == 1);
    }
}
