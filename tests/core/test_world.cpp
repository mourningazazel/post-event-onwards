#include "peo/core/rng.hpp"
#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <optional>
#include <vector>

using namespace peo::core;

namespace {

constexpr Seed kSeed = 7;
constexpr int kDeterminismTurns = 100;
constexpr int kWaitTurns = 30;
/// A World turn sweeps the whole scent field, so cost scales with stage area.
/// A 24x16 stage is ~1/9 of the default 80x45 and keeps these cases in budget.
constexpr int kTestStageWidth = 24;
constexpr int kTestStageHeight = 16;
constexpr int kTestDead = 20;
/// Waits for 'every Dead moves'. A 24x16 stage fits inside even the old 29-cell
/// reach, so 300 waits would pass at floor 1e-6 too. Speed is what differs here:
/// measured, the last unit first moves at turn 23-26 at 1e-30 (seeds 7/1/42/3) but
/// 56-70 at 1e-6. 40 passes with margin and catches a regression to the old floor.
constexpr int kEveryDeadWaits = 40;

WorldParams small_world(int dead = kTestDead) {
    return {.initial_dead = dead, .stage_width = kTestStageWidth, .stage_height = kTestStageHeight};
}

/// Cells whose scent differs, and a differing age line counts as one more. The
/// field is integer (D-024), so equal means equal.
std::size_t scent_mismatches(const ScentWave& a, const ScentWave& b) {
    std::size_t differ = a.updates() != b.updates() ? 1U : 0U;
    for (std::size_t i = 0; i < a.values().size(); ++i) {
        differ += a.values()[i] != b.values()[i] ? 1U : 0U;
    }
    return differ;
}

/// Sum of the field's samples over the stage.
long long scent_total(const World& w) {
    long long sum = 0;
    for (int y = 0; y < w.stage().spec.height; ++y) {
        for (int x = 0; x < w.stage().spec.width; ++x) {
            sum += w.scent().sample({x, y});
        }
    }
    return sum;
}

/// FNV-1a over the player, the Dead's positions and the scent values.
struct Fnv1a {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    void add(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h = (h ^ ((v >> (8 * i)) & 0xFFU)) * 0x100000001B3ULL;
        }
    }
};

std::uint64_t world_hash(const World& w) {
    Fnv1a f;
    f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(w.player().x)));
    f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(w.player().y)));
    f.add(w.stage_index());
    for (const Dead& d : w.horde()) {
        f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(d.pos.x)));
        f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(d.pos.y)));
    }
    f.add(w.scent().updates());
    for (const std::int32_t v : w.scent().values()) {
        f.add(static_cast<std::uint32_t>(v));
    }
    return f.h;
}

/// A fixed, varied action script: steps in all four directions plus waits.
Action scripted_action(int i) {
    const int k = i % 5;
    return k == 4 ? Action::wait() : Action::step(kNeighbours4[k]);
}

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

long long horde_distance(const World& w) {
    long long sum = 0;
    for (const Dead& d : w.horde()) {
        sum += chebyshev(d.pos, w.player());
    }
    return sum;
}

/// Shortest 4-connected path from the player to the stage exit, as step directions.
std::vector<Vec2i> path_to_exit(const World& w) {
    const Grid<bool>& blocked = w.stage().blocked;
    Grid<int> came_from(blocked.width(), blocked.height(), -1);
    std::deque<Vec2i> open{w.player()};
    came_from.at(w.player()) = 4; // start marker
    while (!open.empty()) {
        const Vec2i c = open.front();
        open.pop_front();
        if (c == w.stage().exit) {
            break;
        }
        for (int d = 0; d < 4; ++d) {
            const Vec2i n = c + kNeighbours4[d];
            if (blocked.in_bounds(n) && !blocked.at(n) && came_from.at(n) == -1) {
                came_from.at(n) = d;
                open.push_back(n);
            }
        }
    }
    // Walk back from the exit, appending, then reverse: prepending with insert()
    // trips a -Wnull-dereference false positive in libstdc++ at -O3.
    std::vector<Vec2i> steps;
    for (Vec2i c = w.stage().exit; c != w.player();) {
        const int d = came_from.at(c);
        REQUIRE(d >= 0);
        steps.push_back(kNeighbours4[d]);
        c = c - kNeighbours4[d];
    }
    std::reverse(steps.begin(), steps.end());
    return steps;
}

} // namespace

TEST_SUITE("world") {
    TEST_CASE("same seed same world after 100 turns") {
        World a(kSeed, small_world());
        World b(kSeed, small_world());
        for (int i = 0; i < kDeterminismTurns; ++i) {
            a.step(scripted_action(i));
            b.step(scripted_action(i));
        }
        CHECK(a.player() == b.player());
        CHECK(a.turn() == b.turn());
        REQUIRE(a.scent().values().size() == b.scent().values().size());
        CHECK(scent_total(a) > 0); // the field is not trivially empty
        CHECK(scent_mismatches(a.scent(), b.scent()) == 0);
        REQUIRE(a.horde().size() == b.horde().size());
        for (std::size_t i = 0; i < a.horde().size(); ++i) {
            CHECK(a.horde()[i].pos == b.horde()[i].pos);
        }
    }

    TEST_CASE("the pre-clock world is pinned") {
        // PEO-040 recorded this before the world clock existed and the clock kept it
        // bit for bit. PEO-058 (D-031) re-pinned it on purpose: the Dead now move in
        // hashed slots every second, never share a tile and no longer carry a
        // cooldown, so their paths and the hashed state changed.
        // PEO-030 (D-024) re-pinned it again on purpose: the scent is now the integer
        // geodesic field, so every sample, and the Dead's routes, changed.
        constexpr int kRandomActions = 100; // before and after the walk to the exit
        constexpr std::uint64_t kPinnedHash = 0x4B5B9E9B2BE5F1E7ULL;
        World w(kSeed, small_world());
        Rng pick(kSeed);
        const auto random_actions = [&] {
            for (int i = 0; i < kRandomActions; ++i) {
                const int k = pick.range(0, 4); // 0-3 the four directions, 4 wait
                w.step(k == 4 ? Action::wait() : Action::step(kNeighbours4[k]));
            }
        };
        random_actions(); // walls turn some of these steps into waits
        for (const Vec2i d : path_to_exit(w)) {
            w.step(Action::step(d));
        }
        random_actions();
        CHECK(w.stage_index() == 1); // the script crosses one stage
        CHECK(world_hash(w) == kPinnedHash);
    }

    TEST_CASE("step into wall consumes the turn but does not move") {
        World w(kSeed, small_world());
        // Walk west until blocked (the border is always a wall).
        while (!w.stage().blocked.at(w.player() + Vec2i{-1, 0})) {
            w.step(Action::step({-1, 0}));
        }
        const Vec2i before = w.player();
        const Tick turn = w.turn();
        REQUIRE(w.stage().blocked.at(before + Vec2i{-1, 0}));

        w.step(Action::step({-1, 0}));
        CHECK(w.player() == before);
        CHECK(w.turn() == turn + 1);
    }

    TEST_CASE("wait advances the turn and the dead approach") {
        World w(kSeed, small_world());
        const long long before = horde_distance(w);
        const Vec2i where = w.player();
        for (int i = 0; i < kWaitTurns; ++i) {
            w.step(Action::wait());
        }
        CHECK(w.turn() == kWaitTurns);
        CHECK(w.player() == where);
        CHECK(horde_distance(w) < before);
    }

    TEST_CASE("every Dead moves when the player waits") {
        // PEO-026: with the default scent params the field must reach every one of
        // the Dead on a real stage; one frozen on a flat zero field never moves.
        World w(kSeed, small_world());
        const std::vector<Dead> start = w.horde();
        std::vector<bool> moved(start.size(), false);
        for (int t = 0; t < kEveryDeadWaits; ++t) {
            w.step(Action::wait());
            for (std::size_t i = 0; i < start.size(); ++i) {
                moved[i] = moved[i] || w.horde()[i].pos != start[i].pos;
            }
        }
        for (std::size_t i = 0; i < start.size(); ++i) {
            CAPTURE(i);
            CHECK(moved[i]);
        }
    }

    TEST_CASE("a tiny stage is clamped to 3x3") {
        // PEO-034: a 1x1 stage used to reach rng.range(1, -1) and spin load_stage.
        World w(kSeed, {.initial_dead = 1, .stage_width = 1, .stage_height = 1});
        CHECK(w.stage().spec.width == kMinStageSide);
        CHECK(w.stage().spec.height == kMinStageSide);
        constexpr int kTinyTurns = 5;
        for (int i = 0; i < kTinyTurns; ++i) {
            w.step(Action::wait()); // entry is the exit here, so each turn loads a stage
        }
        CHECK(w.stage().spec.width == kMinStageSide);
    }

    TEST_CASE("more Dead than floor spawns only what fits") {
        constexpr int kSide = 6;
        constexpr int kTooMany = 1000;
        World w(kSeed, {.initial_dead = kTooMany, .stage_width = kSide, .stage_height = kSide});
        const Grid<bool>& blocked = w.stage().blocked;
        int open = 0;
        for (int y = 0; y < kSide; ++y) {
            for (int x = 0; x < kSide; ++x) {
                open += !blocked.at(x, y) && Vec2i{x, y} != w.player() ? 1 : 0;
            }
        }
        CHECK(w.horde().size() == static_cast<std::size_t>(open));
        for (const Dead& d : w.horde()) {
            CHECK_FALSE(blocked.at(d.pos));
            CHECK(d.pos != w.player());
        }
        CHECK(
            World(kSeed, {.initial_dead = -1, .stage_width = kSide, .stage_height = kSide}).horde().empty());
    }

    TEST_CASE("reaching exit advances stage_index") {
        World w(kSeed, small_world(0));
        const std::vector<Vec2i> steps = path_to_exit(w);
        REQUIRE_FALSE(steps.empty());
        for (const Vec2i d : steps) {
            w.step(Action::step(d));
        }
        CHECK(w.stage_index() == 1);
        CHECK(w.turn() == 0);
        CHECK(w.player() == w.stage().entry);
    }

    TEST_CASE("commit(speculate()) is bit-identical to step()") {
        // PEO-007 golden test, extended for D-015 and D-031. Random seeds and action
        // sequences, biased east so runs cross stage exits; walls make some steps into
        // waits. Durations are 1, 3, 6 or 12 s, so some actions cross no update
        // boundary, some one and some two, and the Dead's slots fall at every offset;
        // B re-speculates either like the frontend, only when the update or stage
        // moved on, so one speculation serves several short actions, or after every
        // action, so recorded windows of the Dead's seconds also start mid-update
        // (PEO-060). Odd and even seeds take one mode each, so both run without
        // doubling the case's cost under the sanitizers (suite budget, 1 s). A and B
        // are compared after every action.
        constexpr int kSequences = 200;
        constexpr int kTurns = 8;
        constexpr int kGoldenWidth = 6;
        constexpr int kGoldenHeight = 6;
        constexpr int kGoldenDead = 16; // fills the 6x6 interior (capped at open cells)
        constexpr int kWaitOneIn = 5;
        constexpr int kEastOneIn = 3; // two in three steps go east
        constexpr Seconds kDurations[] = {1, 3, 6, 12};
        const WorldParams params{
            .initial_dead = kGoldenDead, .stage_width = kGoldenWidth, .stage_height = kGoldenHeight};
        int transitions = 0;
        for (int seq = 0; seq < kSequences; ++seq) {
            const bool every_action = seq % 2 == 1;
            const Seed seed = static_cast<Seed>(seq) + 1;
            Rng pick(seed);
            World a(seed, params);
            World b(seed, params);
            Speculation spec; // reused every turn, as the frontend does
            b.speculate(spec);
            for (int t = 0; t < kTurns; ++t) {
                const Seconds secs = kDurations[pick.range(0, 3)];
                Action act = Action::wait(secs);
                if (pick.range(1, kWaitOneIn) != 1) {
                    act = pick.range(1, kEastOneIn) != 1 ? Action::step({1, 0}, secs)
                                                         : Action::step(kNeighbours4[pick.range(0, 3)], secs);
                }
                const std::uint32_t stage_before = a.stage_index();
                const Tick update_before = b.updates();
                a.step(act);
                b.commit(spec, act);
                if (every_action || b.updates() != update_before || b.stage_index() != stage_before) {
                    b.speculate(spec);
                }
                transitions += a.stage_index() != stage_before ? 1 : 0;
                // Plain branch, not a per-turn REQUIRE: doctest's bookkeeping under
                // the sanitizers cost more than the turn itself.
                if (!World::equivalent(a, b)) {
                    FAIL("diverged at sequence " << seq << " turn " << t << " every_action " << every_action);
                }
            }
        }
        CHECK(transitions > 0); // the sequences really do cross stages
    }

    TEST_CASE("two 3 s steps each deposit a little less than a walking step") {
        // D-015 on the geodesic field (PEO-030): a tile held for part of the period
        // deposits as if the scent were that much older, strength less age_cost x the
        // empty share, never more than a walking step's strength.
        constexpr Seconds kHalf = kUpdatePeriodSeconds / 2;
        const WorldParams params = small_world(0);
        World w(kSeed, params);
        const Grid<bool>& blocked = w.stage().blocked;
        const Vec2i start = w.player();
        Vec2i dir{};
        for (const Vec2i d : kNeighbours4) {
            if (blocked.in_bounds(start + d) && !blocked.at(start + d)) {
                dir = d;
                break;
            }
        }
        REQUIRE(dir != Vec2i{});
        ScentWave expected = w.scent();

        w.step(Action::step(dir, kHalf));
        CHECK(w.updates() == 0);
        REQUIRE(w.occupancy().size() == 1);
        CHECK(w.occupancy()[0].tile == start + dir);
        CHECK(w.occupancy()[0].seconds == kHalf);

        w.step(Action::step(Vec2i{} - dir, kHalf)); // back to the start tile
        CHECK(w.updates() == 1);
        CHECK(w.occupancy().empty());

        const std::int32_t part = params.scent.strength - params.scent.age_cost / 2;
        CHECK(part < params.scent.strength); // a partly held tile never deposits more
        expected.deposit(start + dir, part);
        expected.deposit(start, part);
        expected.update(blocked);
        CHECK(scent_mismatches(w.scent(), expected) == 0);
    }

    TEST_CASE("a runner moves two cells per update") {
        // D-015 / PEO-041: a running step takes kRunStepSeconds, so two fit in one
        // update. Scent follows time: each cell deposits as a tile held half the
        // period, a little less than a walking step's strength (PEO-030).
        constexpr int kRunSteps = 4;
        constexpr int kStepsPerUpdate = static_cast<int>(kUpdatePeriodSeconds / kRunStepSeconds);
        static_assert(kStepsPerUpdate == 2);
        const WorldParams params = small_world(0);
        // Find a seed whose entry has kRunSteps open cells in a straight line.
        std::optional<World> found;
        Vec2i dir{};
        for (Seed seed = kSeed; !found && seed < kSeed + 100; ++seed) {
            World w(seed, params);
            const Grid<bool>& blocked = w.stage().blocked;
            for (const Vec2i d : kNeighbours4) {
                bool clear = true;
                for (int i = 1; i <= kRunSteps; ++i) {
                    const Vec2i c = w.player() + Vec2i{d.x * i, d.y * i};
                    clear = clear && blocked.in_bounds(c) && !blocked.at(c) && c != w.stage().exit;
                }
                if (clear) {
                    found.emplace(w);
                    dir = d;
                    break;
                }
            }
        }
        REQUIRE(found);
        World& runner = *found;
        World walker = runner;
        const Grid<bool>& blocked = runner.stage().blocked;
        const Vec2i start = runner.player();
        ScentWave expected = runner.scent();
        const std::int32_t part = params.scent.strength - params.scent.age_cost / 2;

        for (int i = 1; i <= kRunSteps; ++i) {
            runner.step(Action::step(dir, kRunStepSeconds));
            const Vec2i at = start + Vec2i{dir.x * i, dir.y * i};
            CHECK(runner.player() == at);
            CHECK(runner.updates() == static_cast<Tick>(i / kStepsPerUpdate));
            if (i % kStepsPerUpdate == 1) {
                REQUIRE(runner.occupancy().size() == 1);
                CHECK(runner.occupancy()[0].seconds == kRunStepSeconds);
            } else {
                // The update just ran on this cell and the one before, half a period each.
                expected.deposit(at - dir, part);
                expected.deposit(at, part);
                expected.update(blocked);
            }
        }
        CHECK(runner.player() == start + Vec2i{dir.x * kRunSteps, dir.y * kRunSteps});
        CHECK(runner.updates() == 2);
        CHECK(scent_mismatches(runner.scent(), expected) == 0);
        CHECK(part < params.scent.strength); // a runner never deposits more than a walker

        // A walker spends the same 12 s on two cells: the same seconds, so the Dead
        // (who move in slots per second, D-031) get no more moves against a runner.
        walker.step(Action::step(dir));
        walker.step(Action::step(dir));
        CHECK(walker.seconds() == runner.seconds());
        CHECK(walker.updates() == runner.updates());
        CHECK(walker.player() == start + Vec2i{dir.x * kStepsPerUpdate, dir.y * kStepsPerUpdate});
    }

    TEST_CASE("a 12 s action runs two updates") {
        constexpr Seconds kTwoPeriods = 2 * kUpdatePeriodSeconds;
        World once(kSeed, small_world());
        World twice(kSeed, small_world());
        once.step(Action::wait(kTwoPeriods));
        twice.step(Action::wait());
        twice.step(Action::wait());
        CHECK(once.updates() == 2);
        CHECK(once.seconds() == kTwoPeriods);
        // The Dead lived the same twelve seconds: the same as two 6 s waits.
        REQUIRE(once.horde().size() == twice.horde().size());
        for (std::size_t i = 0; i < once.horde().size(); ++i) {
            CHECK(once.horde()[i].pos == twice.horde()[i].pos);
        }
        CHECK(scent_mismatches(once.scent(), twice.scent()) == 0);
    }

    TEST_CASE("an action that does not reach a boundary changes no scent") {
        // The scent moves only at updates; the Dead live every second (D-031), so a
        // 3 s wait leaves them exactly where three 1 s waits do.
        constexpr Seconds kShort = kUpdatePeriodSeconds / 2;
        World w(kSeed, small_world());
        World seconds(kSeed, small_world());
        const ScentWave scent_before = w.scent();
        w.step(Action::wait(kShort));
        for (Seconds s = 0; s < kShort; ++s) {
            seconds.step(Action::wait(1));
        }
        CHECK(w.updates() == 0);
        CHECK(w.seconds() == kShort);
        CHECK(w.turn() == 1);
        CHECK(scent_mismatches(w.scent(), scent_before) == 0);
        REQUIRE(w.horde().size() == seconds.horde().size());
        for (std::size_t i = 0; i < w.horde().size(); ++i) {
            CHECK(w.horde()[i].pos == seconds.horde()[i].pos);
        }
    }

    TEST_CASE("calm Dead never share a tile") {
        // D-031: 300 one-second waits on the small stage and on a crowded 6x6 one
        // (16 Dead in 16 open cells, so every move is into a just-vacated tile).
        constexpr int kSeconds = 300;
        constexpr int kCrowdSide = 6;
        constexpr int kCrowdDead = 16;
        const WorldParams worlds[] = {
            small_world(),
            {.initial_dead = kCrowdDead, .stage_width = kCrowdSide, .stage_height = kCrowdSide},
        };
        for (const WorldParams& params : worlds) {
            World w(kSeed, params);
            Grid<std::uint8_t> count(w.stage().spec.width, w.stage().spec.height, 0);
            int shared = 0;
            for (int t = 0; t < kSeconds; ++t) {
                w.step(Action::wait(1));
                count.fill(0);
                for (const Dead& d : w.horde()) {
                    shared += ++count.at(d.pos) > 1 ? 1 : 0;
                }
            }
            CHECK(shared == 0);
        }
    }

    TEST_CASE("a vacated tile waits a second") {
        // D-031: a unit deciding at t still sees one that decided at t-1 on its old
        // tile, so a tile left at t can be landed on at t+2 at the earliest. Five
        // seeds on the small stage give dozens of follow-ups, some at exactly 2 s.
        constexpr int kSeconds = 300;
        constexpr Seed kSeeds = 5;
        constexpr int kMinGap = 2;
        constexpr int kNever = -1000;
        int follow_ups = 0;
        int at_min_gap = 0;
        for (Seed seed = kSeed; seed < kSeed + kSeeds; ++seed) {
            World w(seed, small_world());
            Grid<int> left_at(kTestStageWidth, kTestStageHeight, kNever);
            Grid<int> left_by(kTestStageWidth, kTestStageHeight, -1);
            for (int t = 1; t <= kSeconds && w.stage_index() == 0; ++t) {
                const std::vector<Dead> before = w.horde();
                w.step(Action::wait(1));
                if (w.stage_index() != 0) {
                    break; // a new stage: positions no longer compare
                }
                for (std::size_t i = 0; i < before.size(); ++i) {
                    const Vec2i from = before[i].pos;
                    const Vec2i to = w.horde()[i].pos;
                    if (from == to) {
                        continue;
                    }
                    if (left_by.at(to) >= 0 && left_by.at(to) != static_cast<int>(i)) {
                        ++follow_ups;
                        at_min_gap += t - left_at.at(to) == kMinGap ? 1 : 0;
                        CHECK(t - left_at.at(to) >= kMinGap);
                    }
                    left_at.at(from) = t;
                    left_by.at(from) = static_cast<int>(i);
                }
            }
        }
        CHECK(follow_ups > 0);
        CHECK(at_min_gap > 0); // the rule is exercised at its edge, not just far from it
    }

    TEST_CASE("slots differ each cycle") {
        // D-031: a unit's first slot is a hash of (salt, cycle, index), so it
        // wanders over the whole cycle, and the same inputs repeat it exactly.
        constexpr int kCycles = 100;
        constexpr std::uint64_t kSalt = 7;
        constexpr std::size_t kUnit = 3;
        std::vector<bool> seen(kDeadCycleSeconds, false);
        std::vector<Seconds> first;
        for (int c = 0; c < kCycles; ++c) {
            const SlotPlan plan = plan_slots(kSalt, static_cast<std::uint64_t>(c), kUnit,
                                             kUpdatePeriodSeconds, kDeadCycleSeconds);
            REQUIRE(plan.count > 0);
            first.push_back(slot_second(plan, 0, kDeadCycleSeconds));
            seen[first.back()] = true;
        }
        for (Seconds s = 0; s < kDeadCycleSeconds; ++s) {
            CAPTURE(s);
            CHECK(seen[s]);
        }
        for (int c = 0; c < kCycles; ++c) {
            const SlotPlan again = plan_slots(kSalt, static_cast<std::uint64_t>(c), kUnit,
                                              kUpdatePeriodSeconds, kDeadCycleSeconds);
            CHECK(slot_second(again, 0, kDeadCycleSeconds) == first[static_cast<std::size_t>(c)]);
        }
    }

    TEST_CASE("speed is slots per cycle") {
        // D-031: dead_cycle / step_seconds slots, the whole part every cycle and the
        // fraction as a hashed chance.
        constexpr int kCycles = 1000;
        constexpr double kTolerance = 0.05;
        constexpr std::uint64_t kSalt = 11;
        constexpr std::size_t kUnit = 5;
        const auto mean_slots = [&](Seconds step) {
            double sum = 0.0;
            for (int c = 0; c < kCycles; ++c) {
                sum += plan_slots(kSalt, static_cast<std::uint64_t>(c), kUnit, step, kDeadCycleSeconds).count;
            }
            return sum / kCycles;
        };
        constexpr Seconds kFast = 3;
        constexpr Seconds kSlow = 18;
        for (int c = 0; c < kCycles; ++c) {
            CHECK(plan_slots(kSalt, static_cast<std::uint64_t>(c), kUnit, kFast, kDeadCycleSeconds).count ==
                  3);
        }
        CHECK(std::abs(mean_slots(kUpdatePeriodSeconds) - 1.5) < kTolerance);
        CHECK(std::abs(mean_slots(kSlow) - 0.5) < kTolerance);
    }

    TEST_CASE("a speculation kept across a stage change falls back live") {
        // PEO-060: B speculates once and never again. Its recorded seconds serve the
        // first update; after that, and after the walk to the exit loads stage 1, the
        // speculation is stale and every second runs live. B matches A throughout.
        constexpr int kAfterExit = 10;
        World a(kSeed, small_world());
        World b(kSeed, small_world());
        Speculation spec = b.speculate();
        const auto both = [&](Action act) {
            a.step(act);
            b.commit(spec, act);
            REQUIRE(World::equivalent(a, b));
        };
        for (const Vec2i d : path_to_exit(a)) {
            both(Action::step(d));
        }
        REQUIRE(a.stage_index() == 1);
        for (int i = 0; i < kAfterExit; ++i) {
            both(Action::wait(i % 2 == 0 ? 1 : kUpdatePeriodSeconds));
        }
    }

    TEST_CASE("a reused or a fresh speculation commits exactly as step") {
        // PEO-078: a reused Speculation syncs its wave from the World's partner (only
        // changed tiles); a fresh one copies the whole field. Both match step() after
        // every one of 200 mixed actions, and a warm speculate copies less than the field.
        constexpr int kActions = 200;
        constexpr int kWaitOneIn = 5;
        constexpr Seconds kDurations[] = {1, 3, 6, 12};
        for (const bool reuse : {true, false}) {
            Rng pick(kSeed);
            World a(kSeed, small_world());
            World b(kSeed, small_world());
            Speculation kept;
            for (int i = 0; i < kActions; ++i) {
                const Seconds secs = kDurations[pick.range(0, 3)];
                const Action act = pick.range(1, kWaitOneIn) == 1
                                       ? Action::wait(secs)
                                       : Action::step(kNeighbours4[pick.range(0, 3)], secs);
                Speculation fresh;
                Speculation& spec = reuse ? kept : fresh;
                b.speculate(spec);
                a.step(act);
                b.commit(spec, act);
                if (!World::equivalent(a, b)) {
                    FAIL("diverged at action " << i << " reuse " << reuse);
                }
            }
        }
    }

    TEST_CASE("a warm speculate copies only the tiles that changed") {
        // PEO-078: on a 384x128 stage (48 tiles of 128x8) the first speculate copies the
        // field; once the World and the Speculation have swapped waves, a speculate
        // copies only what either wrote since, a few tiles round the player.
        constexpr int kW = 384;
        constexpr int kH = 128;
        constexpr std::size_t kTiles = (kW / kWaveTileWidth) * (kH / kWaveTileHeight);
        constexpr int kWaits = 6;
        World w(kSeed, {.initial_dead = 0, .stage_width = kW, .stage_height = kH});
        Speculation spec;
        w.speculate(spec);
        CHECK(spec.synced_tiles == kTiles); // cold: the whole field
        for (int i = 0; i < kWaits; ++i) {
            w.commit(spec, Action::wait());
            w.speculate(spec);
            CAPTURE(i);
            CHECK(spec.synced_tiles < kTiles);
        }
    }

    TEST_CASE("a stale speculation falls back to step") {
        World a(kSeed, small_world());
        World b(kSeed, small_world());
        Speculation stale = b.speculate();
        a.step(Action::wait());
        b.step(Action::wait());
        a.step(Action::step({1, 0}));
        b.commit(stale, Action::step({1, 0}));
        CHECK(World::equivalent(a, b));
    }

#ifdef NDEBUG
    // Perf variants (PEO-007, PEO-043, D-021), release builds only: under
    // Debug+ASan they would cost most of the suite. Report, do not assert on time:
    // shared CI runners are too noisy. D-021 budget on the M1 Air: commit < 1 ms,
    // speculated update < 100 ms. `tools/verify.py --perf` prints these lines.
    // Each sample speculates, then commits a 6 s wait: commit's time includes the
    // Dead's seconds replayed from the speculation (PEO-060), speculate's their run-ahead.
    void run_perf(int width, int height, int dead, int emitters_per_1000_tiles) {
        constexpr int kWarmTurns = 20;
        constexpr int kSamples = 10;
        constexpr int kTilesPer = 1000;
        using Clock = std::chrono::steady_clock;
        World w(kSeed, {.initial_dead = dead, .stage_width = width, .stage_height = height});
        for (int i = 0; i < kWarmTurns; ++i) {
            w.step(Action::wait());
        }
        // Emitter tiles: fixed floor cells drawn once from kSeed (benchmark deposits only).
        const Grid<bool>& blocked = w.stage().blocked;
        const int count = width * height * emitters_per_1000_tiles / kTilesPer;
        std::vector<Vec2i> emitters;
        emitters.reserve(static_cast<std::size_t>(count));
        Rng rng(kSeed);
        while (static_cast<int>(emitters.size()) < count) {
            const Vec2i c{rng.range(1, width - 2), rng.range(1, height - 2)};
            if (!blocked.at(c)) {
                emitters.push_back(c);
            }
        }
        const std::int32_t strength = WorldParams{}.scent.strength;
        Speculation spec;
        w.speculate(spec); // size the buffers once
        double best_spec_us = 1e30;
        double best_commit_us = 1e30;
        for (int i = 0; i < kSamples; ++i) {
            for (const Vec2i e : emitters) {
                w.deposit(e, strength);
            }
            const auto t0 = Clock::now();
            w.speculate(spec);
            const auto t1 = Clock::now();
            w.commit(spec, Action::wait());
            const auto t2 = Clock::now();
            best_spec_us = std::min(best_spec_us, std::chrono::duration<double, std::micro>(t1 - t0).count());
            best_commit_us =
                std::min(best_commit_us, std::chrono::duration<double, std::micro>(t2 - t1).count());
        }
        MESSAGE("perf " << width << "x" << height << " dead=" << dead
                        << " emitters=" << emitters_per_1000_tiles << " speculate=" << best_spec_us
                        << " us commit=" << best_commit_us << " us");
        CHECK(w.turn() == static_cast<Tick>(kWarmTurns + kSamples));
    }

    TEST_CASE("perf: 200x120, 5000 Dead, no emitters") {
        run_perf(200, 120, 5000, 0);
    }

    TEST_CASE("perf: saturated scent update, 512x512, 6 emitters per 1000 tiles") {
        // PEO-078: the scent update alone at steady state. 80 warm-up updates fill the
        // field round every emitter; then the median and p95 of 40 updates.
        constexpr int kSide = 512;
        constexpr int kEmitters = 6; // per 1000 tiles
        constexpr int kPerMille = 1000;
        constexpr int kWarm = 80;
        constexpr int kSamples = 40;
        constexpr std::size_t kP95 = kSamples * 95 / 100;
        using Clock = std::chrono::steady_clock;
        Grid<bool> walls(kSide, kSide, false);
        for (int i = 0; i < kSide; ++i) {
            walls.at(i, 0) = walls.at(i, kSide - 1) = walls.at(0, i) = walls.at(kSide - 1, i) = true;
        }
        Rng rng(kSeed);
        std::vector<Vec2i> emitters(static_cast<std::size_t>(kSide * kSide * kEmitters / kPerMille));
        for (Vec2i& e : emitters) {
            e = {rng.range(1, kSide - 2), rng.range(1, kSide - 2)};
        }
        ScentWave wave(kSide, kSide);
        std::vector<double> us;
        for (int i = 0; i < kWarm + kSamples; ++i) {
            for (const Vec2i e : emitters) {
                wave.deposit(e, wave.params().strength);
            }
            const auto t0 = Clock::now();
            wave.update(walls);
            if (i >= kWarm) {
                us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
            }
        }
        std::sort(us.begin(), us.end());
        MESSAGE("perf 512x512 scent saturated emitters=6 update_median=" << us[us.size() / 2]
                                                                         << " us p95=" << us[kP95] << " us");
        CHECK(wave.active_cells() > 0);
    }

    TEST_CASE("perf: 200x120, 5000 Dead, 6 emitters per 1000 tiles") {
        run_perf(200, 120, 5000, 6);
    }
    TEST_CASE("perf: 512x512, 20000 Dead, 6 emitters per 1000 tiles") {
        run_perf(512, 512, 20000, 6);
    }
    TEST_CASE("perf: 512x512, 50000 Dead, 6 emitters per 1000 tiles") {
        run_perf(512, 512, 50000, 6);
    }
    TEST_CASE("perf: 512x512, 50000 Dead, 20 emitters per 1000 tiles") {
        run_perf(512, 512, 50000, 20);
    }
#endif
}
