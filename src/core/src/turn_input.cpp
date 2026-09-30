#include "peo/core/turn_input.hpp"

#include <algorithm>

namespace peo::core {

TurnInput::TurnInput(TurnInputParams params) noexcept : params_(params) {
    params_.max_waiting = std::min(params_.max_waiting, kTurnInputCapacity);
}

void TurnInput::offer(Action action, bool repeat, Nanos now) noexcept {
    if (repeat) {
        // A hold never builds a queue: only when the cap allows and nothing waits.
        if (count_ != 0 || !due(now)) {
            return;
        }
    } else if (count_ >= params_.max_waiting) {
        return; // the queue is full: dropped, never delayed further
    }
    queue_[(head_ + count_) % kTurnInputCapacity] = action;
    ++count_;
}

std::optional<Action> TurnInput::take(Nanos now) noexcept {
    if (count_ == 0 || !due(now)) {
        return std::nullopt;
    }
    const Action a = queue_[head_];
    head_ = (head_ + 1) % kTurnInputCapacity;
    --count_;
    last_ = now;
    return a;
}

std::optional<Nanos> TurnInput::next_due() const noexcept {
    if (count_ == 0) {
        return std::nullopt;
    }
    return last_ ? *last_ + params_.min_interval : Nanos{0};
}

} // namespace peo::core
