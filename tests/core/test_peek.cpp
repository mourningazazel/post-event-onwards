// PEO-127: the look-ahead strip (src/app/peek.hpp, header-only, no SDL).
#include <doctest/doctest.h>

#include <array>

#include "peek.hpp"

using namespace peo::app;
using peo::core::CellRect;
using peo::core::Vec2i;

namespace {

constexpr Vec2i kNorth{0, -1};
constexpr Vec2i kSouth{0, 1};
constexpr Vec2i kWest{-1, 0};
constexpr Vec2i kEast{1, 0};

// A sight window far larger than any strip here, so only the limit case stops a press.
constexpr CellRect kWide{{-1000, -1000}, {1000, 1000}};

// The constexpr contract: the frontend may size strips at compile time.
static_assert(peek_depth({80, 45}, kNorth) == 6);
static_assert(peek_depth({80, 45}, kEast) == 12);
static_assert(peek_stride(6) == 3);
static_assert(peek_stride(1) == 1);

} // namespace

TEST_SUITE("peek") {
    TEST_CASE("depth is 15% of the view across the edge, rounded down, at least one; stride halves it") {
        CHECK(peek_depth({80, 45}, kNorth) == 45 * 15 / 100);
        CHECK(peek_depth({80, 45}, kSouth) == 45 * 15 / 100);
        CHECK(peek_depth({80, 45}, kWest) == 80 * 15 / 100);
        CHECK(peek_depth({80, 45}, kEast) == 80 * 15 / 100);
        CHECK(peek_depth({1, 1}, kNorth) == 1);
        CHECK(peek_depth({1, 1}, kEast) == 1);
        CHECK(peek_depth({5, 5}, kNorth) == 1);
        CHECK(peek_depth({5, 5}, kWest) == 1);
        CHECK(peek_stride(12) == 6);
        CHECK(peek_stride(7) == 3);
        CHECK(peek_stride(1) == 1);
    }

    TEST_CASE("a closed peek has no strip; a press opens it on that edge at step zero") {
        Peek peek;
        CHECK_FALSE(peek.open());
        CHECK_FALSE(peek.strip({80, 45}, {0, 0}).has_value());
        CHECK(peek.press(kNorth, {80, 45}, {0, 0}, kWide));
        CHECK(peek.open());
        CHECK(peek.dir() == kNorth);
        CHECK(peek.steps() == 0);
        CHECK(peek.strip({80, 45}, {0, 0}).has_value());
    }

    TEST_CASE("at step zero each edge's strip shows the cells one past that edge of the view") {
        const std::array<View, 2> views{{{81, 45}, {80, 44}}};
        const Vec2i o{10, -7};
        for (const View& view : views) {
            CAPTURE(view.cols);
            CAPTURE(view.rows);
            const int dy = peek_depth(view, kNorth);
            const int dx = peek_depth(view, kEast);

            Peek north;
            north.press(kNorth, view, o, kWide);
            const PeekStrip n = *north.strip(view, o);
            CHECK(n.at == Vec2i{0, 0});
            CHECK(n.size.cols == view.cols);
            CHECK(n.size.rows == dy);
            CHECK(n.origin.x == o.x);
            CHECK(n.origin.y + dy == o.y);

            Peek south;
            south.press(kSouth, view, o, kWide);
            const PeekStrip s = *south.strip(view, o);
            CHECK(s.at == Vec2i{0, view.rows - dy});
            CHECK(s.size.cols == view.cols);
            CHECK(s.size.rows == dy);
            CHECK(s.origin.x == o.x);
            CHECK(s.origin.y == o.y + view.rows);

            Peek west;
            west.press(kWest, view, o, kWide);
            const PeekStrip w = *west.strip(view, o);
            CHECK(w.at == Vec2i{0, 0});
            CHECK(w.size.cols == dx);
            CHECK(w.size.rows == view.rows);
            CHECK(w.origin.x + dx == o.x);
            CHECK(w.origin.y == o.y);

            Peek east;
            east.press(kEast, view, o, kWide);
            const PeekStrip e = *east.strip(view, o);
            CHECK(e.at == Vec2i{view.cols - dx, 0});
            CHECK(e.size.cols == dx);
            CHECK(e.size.rows == view.rows);
            CHECK(e.origin.x == o.x + view.cols);
            CHECK(e.origin.y == o.y);
        }
    }

    TEST_CASE("pressing the same way again moves the strip out one stride and nothing else") {
        const View view{80, 45};
        const Vec2i o{0, 0};
        const int stride = peek_stride(peek_depth(view, kNorth));
        Peek peek;
        peek.press(kNorth, view, o, kWide);
        const PeekStrip before = *peek.strip(view, o);
        CHECK(peek.press(kNorth, view, o, kWide));
        CHECK(peek.steps() == 1);
        const PeekStrip after = *peek.strip(view, o);
        CHECK(after.at == before.at);
        CHECK(after.size.cols == before.size.cols);
        CHECK(after.size.rows == before.size.rows);
        CHECK(after.origin == Vec2i{before.origin.x, before.origin.y - stride});
    }

    TEST_CASE("a side press moves the strip to that edge at step zero") {
        const View view{80, 45};
        Peek peek;
        peek.press(kNorth, view, {0, 0}, kWide);
        peek.press(kNorth, view, {0, 0}, kWide);
        CHECK(peek.press(kEast, view, {0, 0}, kWide));
        CHECK(peek.open());
        CHECK(peek.dir() == kEast);
        CHECK(peek.steps() == 0);
        CHECK(peek.strip(view, {0, 0})->origin == Vec2i{view.cols, 0});
    }

    TEST_CASE("the opposite press closes the strip") {
        Peek peek;
        peek.press(kWest, {80, 45}, {0, 0}, kWide);
        CHECK(peek.press(kEast, {80, 45}, {0, 0}, kWide));
        CHECK_FALSE(peek.open());
        CHECK_FALSE(peek.strip({80, 45}, {0, 0}).has_value());
        CHECK(peek.press(kEast, {80, 45}, {0, 0}, kWide)); // and opens afresh after
        CHECK(peek.dir() == kEast);
        CHECK(peek.steps() == 0);
    }

    TEST_CASE("a press stops once the next strip would lie wholly outside the limit") {
        const View view{20, 20}; // depth 3, stride 1
        const Vec2i o{0, 0};
        REQUIRE(peek_depth(view, kNorth) == 3);
        // The north strip at step k covers rows -3 - k to -1 - k: the limit's top row -5
        // is still touched at step 4 (rows -7 to -5) and passed at step 5.
        const CellRect limit{{0, -5}, {19, 24}};
        Peek peek;
        peek.press(kNorth, view, o, limit);
        for (int k = 1; k <= 4; ++k) {
            CAPTURE(k);
            CHECK(peek.press(kNorth, view, o, limit));
            CHECK(peek.steps() == k);
        }
        CHECK(peek.strip(view, o)->origin.y == -7); // only its bottom row touches the limit
        CHECK_FALSE(peek.press(kNorth, view, o, limit));
        CHECK(peek.steps() == 4);
        CHECK(peek.open());

        // An empty window (no compute yet) stops every push but not the opening.
        Peek empty;
        CHECK(empty.press(kEast, view, o, CellRect{{0, 0}, {-1, -1}}));
        CHECK_FALSE(empty.press(kEast, view, o, CellRect{{0, 0}, {-1, -1}}));
        CHECK(empty.steps() == 0);
    }

    TEST_CASE("the strip follows the view: a moved origin moves it by the same amount") {
        const View view{80, 45};
        Peek peek;
        peek.press(kSouth, view, {0, 0}, kWide);
        peek.press(kSouth, view, {0, 0}, kWide);
        const PeekStrip here = *peek.strip(view, {0, 0});
        const PeekStrip walked = *peek.strip(view, {3, -2});
        CHECK(walked.at == here.at);
        CHECK(walked.origin == Vec2i{here.origin.x + 3, here.origin.y - 2});
    }

    TEST_CASE("close closes") {
        Peek peek;
        peek.press(kNorth, {80, 45}, {0, 0}, kWide);
        peek.close();
        CHECK_FALSE(peek.open());
        CHECK_FALSE(peek.strip({80, 45}, {0, 0}).has_value());
    }
}
