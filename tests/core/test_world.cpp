#include "peo/core/rng.hpp"
#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
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

/// Cells whose scent differs in any bit. Bit-exact on purpose: both worlds run
/// the same binary in one process, so any difference at all is a real bug.
std::size_t scent_mismatches(const ScentField& a, const ScentField& b) {
    const Grid<float>& ca = a.cells();
    const Grid<float>& cb = b.cells();
    std::size_t differ = 0;
    for (std::size_t i = 0; i < ca.size(); ++i) {
        differ += std::bit_cast<std::uint32_t>(ca.data()[i]) != std::bit_cast<std::uint32_t>(cb.data()[i]);
    }
    return differ;
}

/// FNV-1a over the state that must not drift when the clock changes (PEO-040).
struct Fnv1a {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    void add(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h = (h ^ ((v >> (8 * i)) & 0xFFU)) * 0x100000001B3ULL;
        }
    }
};

/// Cooldown in updates, the unit it had before PEO-040 moved the Dead to seconds.
std::uint64_t cooldown_updates(const Dead& d) {
    return d.cooldown;
}

std::uint64_t world_hash(const World& w) {
    Fnv1a f;
    f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(w.player().x)));
    f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(w.player().y)));
    f.add(w.stage_index());
    for (const Dead& d : w.horde()) {
        f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(d.pos.x)));
        f.add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(d.pos.y)));
        f.add(cooldown_updates(d));
    }
    for (const float v : w.scent().cells()) {
        f.add(std::bit_cast<std::uint32_t>(v));
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
        REQUIRE(a.scent().cells().size() == b.scent().cells().size());
        CHECK(a.scent().total() > 0.0F); // the field is not trivially empty
        CHECK(scent_mismatches(a.scent(), b.scent()) == 0);
        REQUIRE(a.horde().size() == b.horde().size());
        for (std::size_t i = 0; i < a.horde().size(); ++i) {
            CHECK(a.horde()[i].pos == b.horde()[i].pos);
        }
    }

    TEST_CASE("the pre-clock world is pinned") {
        // PEO-040: recorded on main before the world clock existed. Default 6 s
        // steps and waits must reproduce it bit for bit after the rework.
        constexpr int kRandomActions = 100; // before and after the walk to the exit
        constexpr std::uint64_t kPinnedHash = 0xB2A3557FBD9AD93EULL;
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
        // Fast, lossless scent so the test is about World, not scent tuning (PEO-026).
        WorldParams params = small_world();
        params.scent = {.diffusion = 0.4F, .decay = 0.0F, .floor = 0.0F};
        World w(kSeed, params);
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
        // PEO-007 golden test. Random seeds and action sequences, biased east so
        // runs cross stage exits; walls ahead make some steps into waits.
        constexpr int kSequences = 200;
        constexpr int kTurns = 8;
        constexpr int kGoldenWidth = 6;
        constexpr int kGoldenHeight = 6;
        constexpr int kGoldenDead = 8;
        constexpr int kWaitOneIn = 5;
        constexpr int kEastOneIn = 3; // two in three steps go east
        const WorldParams params{
            .initial_dead = kGoldenDead, .stage_width = kGoldenWidth, .stage_height = kGoldenHeight};
        int transitions = 0;
        for (int seq = 0; seq < kSequences; ++seq) {
            const Seed seed = static_cast<Seed>(seq) + 1;
            Rng pick(seed);
            World a(seed, params);
            World b(seed, params);
            Speculation spec; // reused every turn, as the frontend does
            for (int t = 0; t < kTurns; ++t) {
                Action act = Action::wait();
                if (pick.range(1, kWaitOneIn) != 1) {
                    act = pick.range(1, kEastOneIn) != 1 ? Action::step({1, 0})
                                                         : Action::step(kNeighbours4[pick.range(0, 3)]);
                }
                const std::uint32_t stage_before = a.stage_index();
                a.step(act);
                b.speculate(spec);
                b.commit(spec, act);
                transitions += a.stage_index() != stage_before ? 1 : 0;
                // Plain branch, not a per-turn REQUIRE: doctest's bookkeeping under
                // the sanitizers cost more than the turn itself.
                if (!World::equivalent(a, b)) {
                    FAIL("diverged at sequence " << seq << " turn " << t);
                }
            }
        }
        CHECK(transitions > 0); // the sequences really do cross stages
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
    // Perf sanity (PEO-007), release builds only: under Debug+ASan this one case
    // would cost ~100 ms of the 500 ms suite. Reports, does not assert on time:
    // shared CI runners are too noisy. Budgets: commit < 1 ms, speculate < 16 ms.
    TEST_CASE("speculate and commit at 200x120 with 5000 Dead") {
        constexpr int kPerfWidth = 200;
        constexpr int kPerfHeight = 120;
        constexpr int kPerfDead = 5000;
        constexpr int kWarmTurns = 20;
        constexpr int kSamples = 10;
        using Clock = std::chrono::steady_clock;
        World w(kSeed, {.initial_dead = kPerfDead, .stage_width = kPerfWidth, .stage_height = kPerfHeight});
        for (int i = 0; i < kWarmTurns; ++i) {
            w.step(Action::wait());
        }
        Speculation spec;
        w.speculate(spec); // size the buffers once
        double best_spec_us = 1e30;
        double best_commit_us = 1e30;
        for (int i = 0; i < kSamples; ++i) {
            const auto t0 = Clock::now();
            w.speculate(spec);
            const auto t1 = Clock::now();
            w.commit(spec, Action::wait());
            const auto t2 = Clock::now();
            best_spec_us = std::min(best_spec_us, std::chrono::duration<double, std::micro>(t1 - t0).count());
            best_commit_us =
                std::min(best_commit_us, std::chrono::duration<double, std::micro>(t2 - t1).count());
        }
        MESSAGE("speculate " << best_spec_us << " us, commit " << best_commit_us << " us (best of "
                             << kSamples << ")");
        CHECK(w.turn() == static_cast<Tick>(kWarmTurns + kSamples));
    }
#endif
}
