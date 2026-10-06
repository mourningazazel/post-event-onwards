#include "peo/core/aftermath.hpp"
#include "peo/core/revisit.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/save.hpp"
#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

using namespace peo::core;

namespace {
constexpr Seed kSeed = 53;
constexpr EventSubsteps kDay = kSubstepsPerDay;

WorldParams small_params(int dead = 40) {
    return {.initial_dead = dead, .stage_width = 40, .stage_height = 24};
}

WorldParams on_day(WorldParams p, std::uint32_t day) {
    p.start_day = day;
    return p;
}

Action mixed(Rng& pick) {
    constexpr int kWaitOneIn = 4;
    constexpr int kRunOneIn = 3;
    const Vec2i dir = kNeighbours4[pick.range(0, 3)];
    if (pick.range(1, kWaitOneIn) == 1) {
        return Action::wait();
    }
    return Action::step(dir, pick.range(1, kRunOneIn) == 1 ? kRunStepSubsteps : kStepSubsteps);
}

/// A shortest 4-way walk from the player to the exit.
std::vector<Vec2i> path_to_exit(const World& w) {
    const Grid<bool>& b = w.stage().blocked;
    Grid<int> from(b.width(), b.height(), -1);
    std::deque<Vec2i> q{w.player()};
    from.at(w.player()) = 4;
    while (!q.empty() && from.at(w.stage().exit) < 0) {
        const Vec2i c = q.front();
        q.pop_front();
        for (int k = 0; k < 4; ++k) {
            const Vec2i n = c + kNeighbours4[k];
            if (b.in_bounds(n) && !b.at(n) && from.at(n) < 0) {
                from.at(n) = k;
                q.push_back(n);
            }
        }
    }
    std::vector<Vec2i> steps;
    for (Vec2i c = w.stage().exit; from.at(c) >= 0 && from.at(c) < 4; c = c - kNeighbours4[from.at(c)]) {
        steps.push_back(kNeighbours4[from.at(c)]);
    }
    std::reverse(steps.begin(), steps.end());
    return steps;
}

bool same_horde(const std::vector<Dead>& a, const std::vector<Dead>& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](const Dead& x, const Dead& y) {
               return x.pos == y.pos && x.step_substeps == y.step_substeps && x.fate == y.fate;
           });
}

/// `record` resumed into a fresh World of the seed on `day`. Its own spawn is replaced by
/// the record's horde, so it spawns none (the spawn stream is reseeded per stage anyway).
World resumed(const WorldParams& p, std::uint32_t day, const AreaRecord& record) {
    WorldParams none = on_day(p, day);
    none.initial_dead = 0;
    World w(kSeed, none);
    w.resume_area(record);
    return w;
}
} // namespace

TEST_SUITE("revisit") {
    TEST_CASE("the attrition curve rises, saturates and meets its knots") {
        const Curve& c = kDefaultAttrition;
        constexpr std::uint32_t kTenYears = 3650;
        CHECK(affected_at(c, 0) == 0);
        std::uint32_t last = 0;
        for (std::uint32_t day = 0; day <= kTenYears; ++day) {
            const std::uint32_t now = affected_at(c, day * kDay);
            CHECK(now >= last);
            last = now;
        }
        for (std::size_t i = 0; i < c.count; ++i) {
            CHECK(affected_at(c, c.knots[i].day * kDay) == c.knots[i].affected);
        }
        CHECK(affected_at(c, kTenYears * kDay) == c.knots[c.count - 1].affected);
        CHECK(affected_at(c, std::uint64_t{1} << 62) == c.knots[c.count - 1].affected);
    }

    TEST_CASE("valid() refuses a bad curve and World keeps the default instead") {
        Curve backwards = kDefaultAttrition;
        std::swap(backwards.knots[1].day, backwards.knots[2].day);
        Curve falling = kDefaultAttrition;
        falling.knots[3].affected = falling.knots[2].affected - 1;
        Curve empty = kDefaultAttrition;
        empty.count = 0;
        Curve past = kDefaultAttrition;
        past.knots[4].affected = kAffectedScale; // a fate tops out one below it
        for (const Curve& bad : {backwards, falling, empty, past}) {
            CHECK_FALSE(valid(bad));
            WorldParams p = small_params();
            p.attrition = bad;
            CHECK(World(kSeed, p).params().attrition == kDefaultAttrition);
        }
    }

    TEST_CASE("a fate drawn at t is never below the curve at t and spreads evenly above it") {
        constexpr int kHashes = 10000;
        constexpr int kQuarters = 4;
        constexpr double kTolerance = 0.05;
        constexpr std::uint32_t kTop = 65535;
        const EventSubsteps t = 30 * kDay;
        const std::uint32_t low = affected_at(kDefaultAttrition, t);
        std::array<int, kQuarters> counts{};
        for (int i = 0; i < kHashes; ++i) {
            const std::uint16_t fate =
                draw_fate(kDefaultAttrition, t, hash_u64(kSeed, static_cast<std::uint64_t>(i), 0));
            REQUIRE(fate >= low);
            ++counts[static_cast<std::size_t>((fate - low) * kQuarters / (kTop - low + 1))];
        }
        for (const int n : counts) {
            CHECK(std::abs(static_cast<double>(n) / kHashes - 1.0 / kQuarters) <=
                  kTolerance * (1.0 / kQuarters));
        }
    }

    TEST_CASE("thin_horde drops exactly the units the curve has passed, keeping order") {
        const EventSubsteps now = 365 * kDay;
        const std::uint32_t gone = affected_at(kDefaultAttrition, now);
        std::vector<Dead> horde;
        for (int i = 0; i < 40; ++i) {
            horde.push_back({.pos = {i, 0},
                             .fate = static_cast<std::uint16_t>(gone - 20 + static_cast<std::uint32_t>(i))});
        }
        std::vector<Dead> expected;
        std::copy_if(horde.begin(), horde.end(), std::back_inserter(expected),
                     [&](const Dead& d) { return d.fate >= gone; });
        thin_horde(horde, kDefaultAttrition, now);
        CHECK(same_horde(horde, expected));
        CHECK(horde.size() == 20);
    }

    TEST_CASE("losses grow with the absence and level off after five years") {
        constexpr int kDead = 2000;
        constexpr double kLongSurvivors = 0.15;
        constexpr double kPoints = 0.02;
        const WorldParams p{.initial_dead = kDead, .stage_width = 64, .stage_height = 48};
        World a(kSeed, p);
        REQUIRE(a.horde().size() == static_cast<std::size_t>(kDead));
        const AreaRecord record = *a.store_area();
        // Back on the day it was spawned: nothing has met the curve.
        CHECK(resumed(p, static_cast<std::uint32_t>(record.updated_at / kDay), record).horde().size() ==
              record.horde.size());
        std::size_t last = 0;
        for (const std::uint32_t day : {8U, 37U, 100U, 365U, 1825U, 3650U}) {
            CAPTURE(day);
            const std::size_t lost = record.horde.size() - resumed(p, day, record).horde().size();
            CHECK(lost >= last);
            if (day == 3650U) {
                CHECK(lost == last); // flat after the last knot
            }
            last = lost;
        }
        const double kept = static_cast<double>(resumed(p, 1825, record).horde().size()) / kDead;
        MESSAGE("kept at five years: " << kept);
        CHECK(std::abs(kept - kLongSurvivors) <= kPoints);
    }

    TEST_CASE("the event clock keeps running across a stage exit") {
        World w(kSeed, small_params(0));
        const EventSubsteps start = w.since_event();
        CHECK(start == 7 * kDay);
        EventSubsteps last = start;
        for (const Vec2i d : path_to_exit(w)) {
            w.step(Action::step(d));
            CHECK(w.since_event() >= last);
            last = w.since_event();
        }
        REQUIRE(w.stage_index() == 1);
        CHECK(w.substeps() == 0);
        CHECK(w.since_event() > start);
        w.step(Action::wait());
        CHECK(w.since_event() == last + kWaitSubsteps);
    }

    TEST_CASE("a unit stored on the entry moves to the nearest free open cell") {
        World a(kSeed, small_params());
        AreaRecord record = *a.store_area();
        const Vec2i entry = a.stage().entry;
        std::erase_if(record.horde, [&](const Dead& d) { return d.pos == entry; });
        record.horde.insert(record.horde.begin(), Dead{.pos = entry, .fate = 0xFFFF});
        const World b = resumed(small_params(), 7, record);
        REQUIRE(!b.horde().empty());
        const Vec2i moved = b.horde().front().pos;
        CHECK(moved != entry);
        CHECK_FALSE(b.stage().blocked.at(moved));
        CHECK(std::max(std::abs(moved.x - entry.x), std::abs(moved.y - entry.y)) == 1);
        Grid<std::uint8_t> count(b.stage().spec.width, b.stage().spec.height, 0);
        for (const Dead& d : b.horde()) {
            CHECK(++count.at(d.pos) == 1);
        }
    }

    TEST_CASE("two resumes of one record play alike") {
        // The same record, seed and clock: equivalent through 20 actions, one World
        // stepping and the other committing speculations.
        constexpr int kActions = 20;
        World a(kSeed, small_params());
        const AreaRecord record = *a.store_area();
        World x = resumed(small_params(), 60, record);
        World y = resumed(small_params(), 60, record);
        REQUIRE(World::equivalent(x, y));
        Rng pick(kSeed);
        Speculation spec;
        for (int i = 0; i < kActions; ++i) {
            const Action act = mixed(pick);
            x.step(act);
            y.speculate(spec);
            y.commit(spec, act);
            CHECK(World::equivalent(x, y));
        }
    }

    TEST_CASE("a hand-built stage cannot be stored") {
        World w(kSeed, small_params(0));
        Stage stage;
        stage.blocked = Grid<bool>(10, 10, false);
        w.load_layout(stage, {5, 5}, {});
        CHECK_FALSE(w.store_area().has_value());
    }

    TEST_CASE("a resumed World saves and restores bit-identical, fates and clock included") {
        World a(kSeed, small_params());
        const World b = resumed(small_params(), 400, *a.store_area());
        const std::vector<std::byte> bytes = write_save(b.save("revisit"));
        World c(kSeed + 1, small_params(0));
        std::vector<SaveIssue> issues;
        REQUIRE(c.restore(read_save(bytes).image, issues) == SaveStatus::Ok);
        CHECK(World::equivalent(b, c));
        CHECK(c.since_event() == b.since_event());
        CHECK(same_horde(c.horde(), b.horde()));
        CHECK(write_save(c.save("revisit")) == bytes);
    }
}

TEST_SUITE("scenario: revisit") {
    TEST_CASE("one long absence equals several short ones") {
        // ADR-0019: thinning reads only the clock at the return, so day 30, 90 and 365
        // in turn leave exactly what day 365 at once does; both Worlds then play alike,
        // one stepping and one committing speculations.
        constexpr int kDead = 2000;
        constexpr int kActions = 20;
        WorldParams p{.initial_dead = kDead, .stage_width = 64, .stage_height = 48};
        p.wind_max = kWindFull;
        World a(kSeed, p);
        Rng pick(kSeed);
        for (int i = 0; i < kActions; ++i) {
            a.step(mixed(pick));
        }
        const AreaRecord record = *a.store_area();
        AreaRecord chain = record;
        World last = resumed(p, 30, chain);
        for (const std::uint32_t day : {90U, 365U}) {
            chain = *last.store_area();
            last = resumed(p, day, chain);
        }
        World once = resumed(p, 365, record);
        CHECK(same_horde(last.horde(), once.horde()));
        CHECK(once.horde().size() < record.horde.size());
        REQUIRE(World::equivalent(last, once));
        Speculation spec;
        for (int i = 0; i < kActions; ++i) {
            const Action act = mixed(pick);
            last.step(act);
            once.speculate(spec);
            once.commit(spec, act);
            if (!World::equivalent(last, once)) {
                FAIL("diverged at action " << i);
            }
        }
    }
}
