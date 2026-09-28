#include "peo/core/grid.hpp"

#include <doctest/doctest.h>

using peo::core::Grid;
using peo::core::Vec2i;

TEST_SUITE("grid") {
    TEST_CASE("construction fills and sizes") {
        Grid<int> g(4, 3, 7);
        CHECK(g.width() == 4);
        CHECK(g.height() == 3);
        CHECK(g.size() == 12);
        CHECK(g.at(3, 2) == 7);
    }

    TEST_CASE("bounds") {
        Grid<int> g(4, 3);
        CHECK(g.in_bounds({0, 0}));
        CHECK(g.in_bounds({3, 2}));
        CHECK_FALSE(g.in_bounds({4, 0}));
        CHECK_FALSE(g.in_bounds({0, 3}));
        CHECK_FALSE(g.in_bounds({-1, 0}));
    }

    TEST_CASE("row-major layout") {
        Grid<int> g(4, 3);
        g.at(1, 2) = 5;
        CHECK(g.data()[2 * 4 + 1] == 5);
    }
}
