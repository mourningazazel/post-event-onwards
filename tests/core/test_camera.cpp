// PEO-117: the frontend's player-centred view (src/app/camera.hpp, header-only, no SDL).
#include <doctest/doctest.h>

#include <array>

#include "camera.hpp"

using namespace peo::app;
using peo::core::Vec2i;

namespace {

constexpr int kMapCellPx = 16; // the debug font's 8 px at the game's scale of 2
constexpr int kHudPx = 2 * kMapCellPx;

// The constexpr contract: the frontend may lay out and place cells at compile time.
static_assert(layout_view(1280, 752, kMapCellPx, kHudPx).view.cols == 80);
static_assert(view_to_screen({5, 5}, view_origin({5, 5}, {9, 7}), {9, 7}) == Vec2i{4, 3});

} // namespace

TEST_SUITE("camera") {
    TEST_CASE("the view is the whole map cells below the HUD, the rest even dark bars") {
        const ViewLayout exact = layout_view(1280, 752, kMapCellPx, kHudPx);
        CHECK(exact.view.cols == 80);
        CHECK(exact.view.rows == 45);
        CHECK(exact.left_px == 0);
        CHECK(exact.top_px == 0);

        const ViewLayout spare = layout_view(1290, 760, kMapCellPx, kHudPx);
        CHECK(spare.view.cols == 80);
        CHECK(spare.view.rows == 45);
        CHECK(spare.left_px == 5);
        CHECK(spare.top_px == 4);

        const ViewLayout short_px = layout_view(1279, 751, kMapCellPx, kHudPx);
        CHECK(short_px.view.cols == 79);
        CHECK(short_px.view.rows == 44);
        CHECK(short_px.left_px == (1279 - 79 * kMapCellPx) / 2);

        // The zoom path (D-050): a map cell twice the size halves the counts.
        const ViewLayout zoomed = layout_view(1280, 752, 2 * kMapCellPx, kHudPx);
        CHECK(zoomed.view.cols == 40);
        CHECK(zoomed.view.rows == 45 / 2);

        const ViewLayout tiny = layout_view(10, 10, kMapCellPx, kHudPx);
        CHECK(tiny.view.cols >= 1);
        CHECK(tiny.view.rows >= 1);
        CHECK(tiny.left_px >= 0);
        CHECK(tiny.top_px >= 0);
    }

    TEST_CASE("the player is always on the view's centre cell, wherever on the stage") {
        constexpr int kStageW = 200;
        constexpr int kStageH = 120;
        const std::array<View, 4> views{{{80, 45}, {81, 45}, {80, 44}, {1, 1}}};
        const std::array<Vec2i, 9> players{{{0, 0},
                                            {kStageW - 1, 0},
                                            {0, kStageH - 1},
                                            {kStageW - 1, kStageH - 1},
                                            {kStageW / 2, kStageH / 2},
                                            {1, kStageH / 2},
                                            {kStageW - 2, kStageH / 2},
                                            {kStageW / 2, 1},
                                            {kStageW / 2, kStageH - 2}}};
        for (const View& view : views) {
            for (const Vec2i& player : players) {
                CAPTURE(view.cols);
                CAPTURE(view.rows);
                CAPTURE(player.x);
                CAPTURE(player.y);
                const Vec2i origin = view_origin(player, view);
                CHECK(view_to_screen(player, origin, view) == Vec2i{view.cols / 2, view.rows / 2});
            }
        }
    }

    TEST_CASE("a cell one past each view edge is off view; origins may be negative") {
        const View view{80, 45};
        const Vec2i player{3, 2}; // near the top-left: the view reaches past the stage
        const Vec2i origin = view_origin(player, view);
        CHECK(origin == Vec2i{3 - 40, 2 - 22});
        CHECK(view_to_screen(origin, origin, view) == Vec2i{0, 0});
        CHECK(view_to_screen(origin + Vec2i{79, 44}, origin, view) == Vec2i{79, 44});
        CHECK_FALSE(view_to_screen(origin + Vec2i{-1, 0}, origin, view));
        CHECK_FALSE(view_to_screen(origin + Vec2i{0, -1}, origin, view));
        CHECK_FALSE(view_to_screen(origin + Vec2i{80, 0}, origin, view));
        CHECK_FALSE(view_to_screen(origin + Vec2i{0, 45}, origin, view));
    }
}
