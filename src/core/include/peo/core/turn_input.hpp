#pragma once

#include "peo/core/action.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace peo::core {

/// Wall-clock nanoseconds, as the frontend reads them (SDL_GetTicksNS). The only
/// wall clock in core, and only here: it paces input, never the simulation.
using Nanos = std::uint64_t;

/// Most taps a TurnInput can ever hold; TurnInputParams::max_waiting is capped at it.
inline constexpr std::size_t kTurnInputCapacity = 8;
/// D-032: one second's worth of taps at the cap.
inline constexpr std::size_t kMaxWaitingTaps = 3;
/// D-011 / D-032: at most three turns a second.
inline constexpr Nanos kNanosPerSecond = 1'000'000'000;
inline constexpr Nanos kMaxTurnsPerSecond = 3;

/// Tunables Ryan judges in playtest (D-032).
struct TurnInputParams {
    std::size_t max_waiting = kMaxWaitingTaps;
    Nanos min_interval = kNanosPerSecond / kMaxTurnsPerSecond;
};

/// Turn keys to turns (D-032, amending D-011). A distinct press is taken at once when
/// the cap allows, else waits in a queue of at most max_waiting, else is dropped; the
/// queue plays out no faster than one turn per min_interval. A held key's auto-repeat
/// is taken only when the cap allows and nothing waits; it never waits, so releasing
/// the key stops it at once. The Action (walk or run) is fixed at press time. No SDL,
/// fixed storage, no allocation.
class TurnInput {
public:
    explicit TurnInput(TurnInputParams params = {}) noexcept;

    /// A turn key went down at `now`; `repeat` for the key's own auto-repeat.
    void offer(Action action, bool repeat, Nanos now) noexcept;
    /// The next turn to take at `now`, if one waits and the cap allows it.
    [[nodiscard]] std::optional<Action> take(Nanos now) noexcept;
    /// When the next waiting turn may be taken; empty when none waits.
    [[nodiscard]] std::optional<Nanos> next_due() const noexcept;
    /// Forget waiting taps (a new stage, quitting).
    void clear() noexcept { count_ = 0; }

    [[nodiscard]] std::size_t waiting() const noexcept { return count_; }
    [[nodiscard]] const TurnInputParams& params() const noexcept { return params_; }

private:
    [[nodiscard]] bool due(Nanos now) const noexcept {
        return !last_ || now - *last_ >= params_.min_interval;
    }

    TurnInputParams params_;
    std::array<Action, kTurnInputCapacity> queue_{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;
    /// When the last turn was taken; empty before the first.
    std::optional<Nanos> last_;
};

} // namespace peo::core
