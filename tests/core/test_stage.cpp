#include "peo/core/stage.hpp"

#include <doctest/doctest.h>

using namespace peo::core;

TEST_SUITE("stage") {
    TEST_CASE("stage seeds are stable and distinct") {
        CHECK(stage_seed(123, 0) == stage_seed(123, 0));
        CHECK(stage_seed(123, 0) != stage_seed(123, 1));
        CHECK(stage_seed(123, 5) != stage_seed(124, 5));
    }

    TEST_CASE("generation is deterministic") {
        const Stage a = generate_stage(stage_spec(9001, 3));
        const Stage b = generate_stage(stage_spec(9001, 3));
        REQUIRE(a.blocked.size() == b.blocked.size());
        for (int y = 0; y < a.blocked.height(); ++y) {
            for (int x = 0; x < a.blocked.width(); ++x) {
                CHECK(a.blocked.at(x, y) == b.blocked.at(x, y));
            }
        }
        CHECK(a.entry == b.entry);
        CHECK(a.exit == b.exit);
    }

    TEST_CASE("borders are walls, entry and exit are open") {
        const Stage s = generate_stage(stage_spec(1, 0));
        for (int x = 0; x < s.spec.width; ++x) {
            CHECK(s.blocked.at(x, 0));
            CHECK(s.blocked.at(x, s.spec.height - 1));
        }
        for (int y = 0; y < s.spec.height; ++y) {
            CHECK(s.blocked.at(0, y));
            CHECK(s.blocked.at(s.spec.width - 1, y));
        }
        CHECK_FALSE(s.blocked.at(s.entry));
        CHECK_FALSE(s.blocked.at(s.exit));
    }

    TEST_CASE("difficulty curve rises and caps") {
        CHECK(stage_spec(1, 0).wall_density < stage_spec(1, 10).wall_density);
        CHECK(stage_spec(1, 1000).wall_density == doctest::Approx(0.35));
    }
}
