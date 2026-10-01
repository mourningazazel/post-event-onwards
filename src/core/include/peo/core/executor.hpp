#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace peo::core {

/// A non-owning reference to a callable taking a piece index (ADR-0014): two pointers,
/// no allocation, valid only while the callable it refers to lives (a run() call).
class PieceFn {
public:
    template <typename F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, PieceFn> && std::is_invocable_v<F&, std::size_t>)
    PieceFn(F&& f) noexcept
        : object_(const_cast<void*>(static_cast<const void*>(&f))),
          call_([](void* object, std::size_t piece) {
              (*static_cast<std::remove_reference_t<F>*>(object))(piece);
          }) {}
    void operator()(std::size_t piece) const { call_(object_, piece); }

private:
    void* object_;
    void (*call_)(void*, std::size_t);
};

/// A parallel-for core's heavy passes are handed (D-035, ADR-0014): run(pieces, body)
/// calls body(i) once for every i in [0, pieces), in any order and on any thread, and
/// returns when all have finished. Core never creates a thread; the frontend hands it a
/// pooled executor, and anything that has none runs its pieces serially (a null
/// Executor* means serial). Every pass that uses one gives the same bits whatever the
/// order: pieces write disjoint cells or their own slots, and shared state is updated
/// after the run, in index order.
class Executor {
public:
    Executor() = default;
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    virtual ~Executor() = default;

    virtual void run(std::size_t pieces, PieceFn body) = 0;
    /// How many pieces may run at once; passes size their pieces from it. At least 1.
    [[nodiscard]] virtual std::size_t width() const noexcept = 0;
};

/// Runs every piece on the calling thread, in index order: the reference.
class SerialExecutor final : public Executor {
public:
    void run(std::size_t pieces, PieceFn body) override {
        for (std::size_t i = 0; i < pieces; ++i) {
            body(i);
        }
    }
    [[nodiscard]] std::size_t width() const noexcept override { return 1; }
};

/// `pieces` calls of body through `executor`, or serially when it is null.
inline void run_pieces(Executor* executor, std::size_t pieces, PieceFn body) {
    if (executor == nullptr || pieces <= 1) {
        for (std::size_t i = 0; i < pieces; ++i) {
            body(i);
        }
        return;
    }
    executor->run(pieces, body);
}

/// Splits [0, count) into contiguous ranges, a few per runnable piece so uneven work
/// evens out, and calls body(begin, end) for each through `executor`.
template <typename Body> void run_ranges(Executor* executor, std::size_t count, Body&& body) {
    constexpr std::size_t kRangesPerWidth = 4;
    const std::size_t width = executor == nullptr ? 1 : executor->width();
    const std::size_t ranges = width <= 1 ? (count == 0 ? 0 : 1) : std::min(count, width * kRangesPerWidth);
    if (ranges == 0) {
        return;
    }
    run_pieces(executor, ranges, [&](std::size_t r) { body(count * r / ranges, count * (r + 1) / ranges); });
}

} // namespace peo::core
