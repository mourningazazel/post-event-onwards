#include "peo/core/dead.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace peo::core;

namespace {

constexpr std::uint64_t kSalt = 0x5A17;

/// A lone unit on an otherwise empty map: the desire field of `f` with no other Dead,
/// and nothing occupies or reserves its tiles.
struct Alone {
    DesireField desire;
    Grid<std::uint8_t> occupied;
    Grid<bool> reserved;
    Alone(const ScentWave& f, const Grid<bool>& walls, const DeadDrawParams& p = {})
        : desire(walls.width(), walls.height()), occupied(walls.width(), walls.height(), 0),
          reserved(walls.width(), walls.height(), false) {
        desire.build(f, walls, occupied, p, nullptr);
    }

    /// One decision with the draw word for (second, 0); moves the unit if it decided.
    bool step(Dead& unit, Slot second) const {
        if (const auto to = decide_move(unit, desire, occupied, reserved, draw_word(kSalt, second, 0))) {
            unit.pos = *to;
            return true;
        }
        return false;
    }
};

/// A standing source: `updates` deposits of full strength, each followed by an update.
ScentWave standing(int width, int height, Vec2i source, const Grid<bool>& walls, int updates) {
    ScentWave f(width, height);
    for (int i = 0; i < updates; ++i) {
        f.deposit(source, f.params().strength);
        f.update(walls);
    }
    return f;
}

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

/// The choice (kNeighbours8 index) that leads from `from` to `to`.
std::size_t choice_to(Vec2i from, Vec2i to) {
    for (std::size_t d = 0; d < std::size(kNeighbours8); ++d) {
        if (from + kNeighbours8[d] == to) {
            return d;
        }
    }
    return kStayChoice;
}

} // namespace

TEST_SUITE("dead") {
    TEST_CASE("the draw's weights") {
        // PEO-009 (D-038 B): a neighbour weighs more the more it climbs, the lean grows
        // with the scent where the unit stands, walls and cut corners weigh nothing, and
        // where scent and company are flat every open neighbour weighs the same.
        constexpr int kSide = 41;
        constexpr Vec2i kSource{20, 20};
        Grid<bool> walls(kSide, kSide, false);
        walls.at(kSource.x + 5, kSource.y) = true; // a wall beside a cell 4 out, east
        const ScentWave f = standing(kSide, kSide, kSource, walls, 30);
        const Alone alone(f, walls);
        // Toward the source beats level beats away.
        const Vec2i near{kSource.x + 4, kSource.y};
        const auto w = alone.desire.weights(near);
        CHECK(w[choice_to(near, near + Vec2i{-1, 0})] > w[choice_to(near, near + Vec2i{0, -1})]);
        CHECK(w[choice_to(near, near + Vec2i{0, -1})] > w[choice_to(near, near + Vec2i{1, -1})]);
        CHECK(w[choice_to(near, near + Vec2i{1, 0})] == 0); // the wall
        // The same one-cell climb counts for more nearer the source (stronger scent).
        const Vec2i far{kSource.x - 20, kSource.y};
        const auto wf = alone.desire.weights(far);
        const double near_odds =
            static_cast<double>(w[choice_to(near, near + Vec2i{-1, 0})]) / w[kStayChoice];
        const double far_odds = static_cast<double>(wf[choice_to(far, far + Vec2i{1, 0})]) / wf[kStayChoice];
        CHECK(far_odds > 1.0);
        CHECK(near_odds > far_odds);
        // Walls and cut corners weigh nothing: the wall at (25,20) itself, and the
        // diagonals from (24,20) and (26,20) that would pass its corner (PEO-044).
        CHECK(alone.desire.weights({24, 21})[choice_to({24, 21}, {25, 20})] == 0);
        CHECK(alone.desire.weights({24, 20})[choice_to({24, 20}, {25, 19})] == 0);
        CHECK(alone.desire.weights({24, 20})[choice_to({24, 20}, {25, 21})] == 0);
        CHECK(alone.desire.weights({26, 20})[choice_to({26, 20}, {25, 21})] == 0);
        CHECK(alone.desire.weights({24, 21})[choice_to({24, 21}, {25, 22})] > 0); // both sides open
        CHECK(alone.desire.weights({24, 19})[choice_to({24, 19}, {25, 18})] > 0);
        // Flat: no scent, no company: every open neighbour and staying weigh alike.
        const ScentWave none(kSide, kSide);
        const Grid<bool> open(kSide, kSide, false);
        const Alone flat(none, open);
        const auto u = flat.desire.weights({10, 10});
        CHECK(std::all_of(u.begin(), u.end(), [&](std::uint32_t x) { return x == u[0]; }));
        CHECK(u[0] == weight_of(0));
    }

    TEST_CASE("company pulls a little toward other Dead") {
        constexpr int kSide = 31;
        const Grid<bool> open(kSide, kSide, false);
        const ScentWave none(kSide, kSide);
        Grid<std::uint8_t> occupied(kSide, kSide, 0);
        // The box east of (15,15): half-size 3, centred 4 cells out, full of the Dead.
        const int r = DeadDrawParams{}.company_radius;
        for (int y = 15 - r; y <= 15 + r; ++y) {
            for (int x = 15 + 1; x <= 15 + 2 * r + 1; ++x) {
                occupied.at(x, y) = 1;
            }
        }
        DesireField desire(kSide, kSide);
        desire.build(none, open, occupied, DeadDrawParams{}, nullptr);
        const auto w = desire.weights({15, 15});
        CHECK(w[2] > w[6]); // east, toward it, over west
        CHECK(desire.log_odds({15, 15}, 2) == DeadDrawParams{}.company_gain);
    }

    TEST_CASE("the draw is deterministic and each unit's own") {
        // Counter-based (salt, second, unit): the same words in any order give the same
        // choices, and a choice follows the weights.
        std::array<std::uint32_t, kDrawChoices> w{};
        w[2] = 3 * weight_of(0);
        w[kStayChoice] = weight_of(0);
        int east = 0;
        constexpr int kDraws = 4000;
        for (int i = 0; i < kDraws; ++i) {
            const std::size_t c = draw_choice(w, draw_word(kSalt, 7, static_cast<std::size_t>(i)));
            CHECK((c == 2 || c == kStayChoice));
            east += c == 2 ? 1 : 0;
        }
        CHECK(std::abs(east - kDraws * 3 / 4) < kDraws / 20); // 3:1, within 5%
        CHECK(draw_word(kSalt, 7, 3) == draw_word(kSalt, 7, 3));
        CHECK(draw_word(kSalt, 7, 3) != draw_word(kSalt, 7, 4));
        CHECK(draw_word(kSalt, 7, 3) != draw_word(kSalt, 8, 3));
        CHECK(draw_choice(std::array<std::uint32_t, kDrawChoices>{}, 12345) == kStayChoice);
    }

    TEST_CASE("the dead mostly climb toward scent near it") {
        // D-038 B: six cells from a standing source the scent is strong, so nearly
        // every draw that moves the unit brings it nearer.
        constexpr Vec2i kSource{7, 4};
        constexpr int kDraws = 400;
        const Grid<bool> walls(9, 9, false);
        const ScentWave f = standing(9, 9, kSource, walls, 6);
        const Alone alone(f, walls);
        int moved = 0;
        int nearer = 0;
        for (Slot t = 0; t < kDraws; ++t) {
            Dead unit{.pos = {1, 4}};
            if (alone.step(unit, t)) {
                ++moved;
                nearer += chebyshev(unit.pos, kSource) < chebyshev({1, 4}, kSource) ? 1 : 0;
            }
        }
        CHECK(moved > 0);
        CHECK(nearer * 10 >= moved * 9);
    }

    TEST_CASE("the dead go round a wall corner, never through it") {
        // PEO-044: a wall arm down x = 3 from the top edge to (3,2); the unit at
        // (2,2) sits inside the corner, the source round it at (5,3). The diagonal
        // (3,3) is open but passes the wall at (3,2): no draw ever takes it.
        constexpr int kSide = 7;
        constexpr int kArmEnd = 2;
        constexpr int kWallX = 3;
        constexpr int kWarmup = 30;
        constexpr int kDecisions = 60;
        constexpr Vec2i kSource{5, 3};
        Grid<bool> walls(kSide, kSide, false);
        for (int y = 0; y <= kArmEnd; ++y) {
            walls.at(kWallX, y) = true;
        }
        const ScentWave f = standing(kSide, kSide, kSource, walls, kWarmup);
        REQUIRE(f.sample({3, 3}) > f.sample({2, 3})); // the corner cut would pull harder
        const Alone alone(f, walls);
        CHECK(alone.desire.weights({2, 2})[choice_to({2, 2}, {3, 3})] == 0);
        Dead unit{.pos = {2, 2}};
        for (Slot t = 0; t < kDecisions; ++t) {
            const Vec2i before = unit.pos;
            alone.step(unit, t);
            const Vec2i d = unit.pos - before;
            CHECK_FALSE(walls.at(unit.pos));
            if (d.x != 0 && d.y != 0) { // a diagonal: both cells beside it are open
                CHECK_FALSE(walls.at(before + Vec2i{d.x, 0}));
                CHECK_FALSE(walls.at(before + Vec2i{0, d.y}));
            }
        }
        CHECK(unit.pos.x > kWallX); // it did come round
    }

    TEST_CASE("a drawn tile that is taken or reserved means staying") {
        // D-031 under the draw: the unit stays and draws no second time.
        constexpr int kSide = 7;
        constexpr Vec2i kFrom{3, 3};
        const Grid<bool> walls(kSide, kSide, false);
        const ScentWave f = standing(kSide, kSide, {6, 3}, walls, 8);
        const Alone alone(f, walls);
        const Dead unit{.pos = kFrom};
        // A word whose draw moves the unit east.
        std::uint64_t word = 0;
        for (std::size_t i = 0;; ++i) {
            word = draw_word(kSalt, 1, i);
            if (draw_choice(alone.desire.weights(kFrom), word) == 2) {
                break;
            }
        }
        Grid<std::uint8_t> occupied(kSide, kSide, 0);
        Grid<bool> reserved(kSide, kSide, false);
        CHECK(decide_move(unit, alone.desire, occupied, reserved, word) == Vec2i{4, 3});
        occupied.at(4, 3) = 1;
        CHECK_FALSE(decide_move(unit, alone.desire, occupied, reserved, word).has_value());
        occupied.at(4, 3) = 0;
        reserved.at(4, 3) = true;
        CHECK_FALSE(decide_move(unit, alone.desire, occupied, reserved, word).has_value());
    }

    TEST_CASE("the dead never enter walls") {
        ScentWave f(5, 5);
        Grid<bool> walls(5, 5, false);
        walls.at(2, 2) = true;
        f.deposit({2, 2}, f.params().strength); // scent inside a wall cell: nothing should walk in
        f.update(walls);
        const Alone alone(f, walls);
        for (Slot t = 0; t < 200; ++t) {
            Dead unit{.pos = {1, 2}};
            alone.step(unit, t);
            CHECK(unit.pos != Vec2i{2, 2});
        }
    }

    // The warm-up only has to lay a gradient the horde can climb: the Dead start
    // within 12 cells of the source, and the front moves a cell per update, so it
    // arrives well inside this many updates.
    constexpr int kHordeWarmupTicks = 20;

    TEST_CASE("a thousand dead decide without touching each other") {
        // Scale, and D-031's rule at scale: 1000 of the Dead on distinct tiles all
        // decide in one second; no decision lands on another's tile or on a tile
        // someone already reserved.
        constexpr int kSide = 64;
        constexpr int kBlock = 25; // a 25x40 block of the Dead just east of the source
        constexpr int kRows = 40;
        constexpr Vec2i kSource{32, 32};
        constexpr Vec2i kCorner{36, 12};
        const Grid<bool> walls(kSide, kSide, false);
        const ScentWave f = standing(kSide, kSide, kSource, walls, kHordeWarmupTicks);
        std::vector<Dead> horde;
        Grid<std::uint8_t> occupied(kSide, kSide, 0);
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kBlock; ++x) {
                horde.push_back({.pos = kCorner + Vec2i{x, y}});
                occupied.at(horde.back().pos) = 1;
            }
        }
        REQUIRE(horde.size() == 1000);
        DesireField desire(kSide, kSide);
        desire.build(f, walls, occupied, DeadDrawParams{}, nullptr);
        Grid<bool> reserved(kSide, kSide, false);
        std::size_t decided = 0;
        for (std::size_t i = 0; i < horde.size(); ++i) {
            if (const auto to = decide_move(horde[i], desire, occupied, reserved, draw_word(kSalt, 1, i))) {
                CHECK(occupied.at(*to) == 0);
                CHECK_FALSE(reserved.at(*to));
                reserved.at(*to) = true;
                ++decided;
            }
        }
        // The block's edges have free tiles; the packed inside cannot move.
        CHECK(decided > 0);
        CHECK(decided < horde.size());
    }

    TEST_CASE("distant Dead close in") {
        // A standing source at (4,5) on a 34x11 map, 40 updates, then frozen: the
        // unit at (19,5) is inside the reach and each call is one draw, so it closes
        // at least 10 of the 15 cells in 50 calls.
        constexpr Vec2i kSource{4, 5};
        constexpr Vec2i kStart{19, 5};
        constexpr int kWarmupSteps = 40;
        constexpr int kCalls = 50;
        constexpr int kMinClosed = 10;
        const Grid<bool> walls(34, 11, false);
        const ScentWave f = standing(34, 11, kSource, walls, kWarmupSteps);
        const Alone alone(f, walls);
        Dead unit{.pos = kStart};
        for (Slot t = 0; t < kCalls; ++t) {
            alone.step(unit, t);
        }
        CHECK(kStart.x - unit.pos.x >= kMinClosed);
    }
}
