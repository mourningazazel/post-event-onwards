#include "peo/core/turn_input.hpp"

#include <doctest/doctest.h>

#include <type_traits>
#include <vector>

using namespace peo::core;

namespace {

constexpr Nanos kMs = 1'000'000;
constexpr Nanos kInterval = kNanosPerSecond / kMaxTurnsPerSecond;
const Action kEast = Action::step({1, 0});
const Action kNorth = Action::step({0, -1});

/// The frontend's loop, synthetically: offer a press, then take what is due; `ticks`
/// are the wake-ups the app arranges at next_due(). Returns the times turns were taken.
struct Driver {
    TurnInput input;
    std::vector<Nanos> taken;
    std::vector<Action> actions;

    explicit Driver(TurnInputParams params = {}) : input(params) {}

    void press(Action a, bool repeat, Nanos now) {
        input.offer(a, repeat, now);
        drain(now);
    }
    void drain(Nanos now) {
        if (const auto a = input.take(now)) {
            taken.push_back(now);
            actions.push_back(*a);
        }
    }
    /// Wake at every next_due() up to `until`, as the app's timer does.
    void run_until(Nanos until) {
        while (const auto due = input.next_due()) {
            if (*due > until) {
                break;
            }
            drain(*due);
        }
    }
};

} // namespace

TEST_SUITE("turn_input") {
    TEST_CASE("five taps within 0.2 s give four turns at the cap") {
        // D-032: one at once, three queued, the fifth dropped; played out 1/3 s apart.
        Driver d;
        for (Nanos t = 0; t <= 200 * kMs; t += 50 * kMs) {
            d.press(kEast, false, t);
        }
        CHECK(d.input.waiting() == kMaxWaitingTaps);
        d.run_until(5 * kNanosPerSecond);
        REQUIRE(d.taken.size() == 4);
        for (std::size_t i = 1; i < d.taken.size(); ++i) {
            CHECK(d.taken[i] - d.taken[i - 1] == kInterval); // spacing exact
        }
        CHECK(d.input.waiting() == 0);
    }

    TEST_CASE("holding a key for 2 s gives about six turns and none after release") {
        // A press, then the OS auto-repeat: a 500 ms delay, then every 33 ms.
        constexpr Nanos kRepeatDelay = 500 * kMs;
        constexpr Nanos kRepeatEvery = 33 * kMs;
        constexpr Nanos kHold = 2 * kNanosPerSecond;
        Driver d;
        d.press(kEast, false, 0);
        for (Nanos t = kRepeatDelay; t <= kHold; t += kRepeatEvery) {
            d.press(kEast, true, t);
        }
        CHECK(d.taken.size() >= 5);
        CHECK(d.taken.size() <= 7);
        CHECK(d.input.waiting() == 0); // repeats never wait
        const std::size_t at_release = d.taken.size();
        d.run_until(kHold + 5 * kNanosPerSecond);
        CHECK(d.taken.size() == at_release); // nothing after release
    }

    TEST_CASE("a tap during a hold waits only if there is room") {
        constexpr Nanos kRepeatEvery = 33 * kMs;
        Driver d;
        d.press(kEast, false, 0);
        d.press(kNorth, false, 100 * kMs); // too soon: waits
        CHECK(d.input.waiting() == 1);
        for (Nanos t = 150 * kMs; t < kInterval; t += kRepeatEvery) {
            d.press(kEast, true, t); // repeats while a tap waits: never queued
        }
        CHECK(d.input.waiting() == 1);
        d.run_until(kNanosPerSecond);
        REQUIRE(d.actions.size() == 2);
        CHECK(d.actions[1].dir == kNorth.dir); // the tap, not a repeat
        // A full queue drops a further tap.
        Driver full;
        for (int i = 0; i < 5; ++i) {
            full.press(kEast, false, static_cast<Nanos>(i) * kMs);
        }
        CHECK(full.input.waiting() == kMaxWaitingTaps);
        full.press(kNorth, false, 10 * kMs);
        CHECK(full.input.waiting() == kMaxWaitingTaps);
    }

    TEST_CASE("taps keep their order and walk or run is fixed at press time") {
        Driver d;
        const Action run_east = Action::step({1, 0}, kRunStepSeconds);
        d.press(kEast, false, 0);
        d.press(kNorth, false, 10 * kMs);
        d.press(run_east, false, 20 * kMs);
        d.run_until(kNanosPerSecond);
        REQUIRE(d.actions.size() == 3);
        CHECK(d.actions[1].dir == kNorth.dir);
        CHECK(d.actions[2].dir == run_east.dir);
        CHECK(d.actions[2].seconds == kRunStepSeconds);
    }

    TEST_CASE("clear forgets waiting taps") {
        Driver d;
        d.press(kEast, false, 0);
        d.press(kEast, false, 10 * kMs);
        REQUIRE(d.input.waiting() == 1);
        d.input.clear();
        CHECK(d.input.waiting() == 0);
        CHECK_FALSE(d.input.next_due().has_value());
    }

    TEST_CASE("depth and rate are tunables, and storage is fixed") {
        // Trivially copyable: no heap-owning members, so nothing allocates after
        // construction.
        static_assert(std::is_trivially_copyable_v<TurnInput>);
        Driver d({.max_waiting = 1, .min_interval = 100 * kMs});
        for (Nanos t = 0; t < 30 * kMs; t += 10 * kMs) {
            d.press(kEast, false, t);
        }
        CHECK(d.input.waiting() == 1);
        d.run_until(kNanosPerSecond);
        REQUIRE(d.taken.size() == 2);
        CHECK(d.taken[1] - d.taken[0] == 100 * kMs);
        CHECK(TurnInput({.max_waiting = 99}).params().max_waiting == kTurnInputCapacity);
    }
}
