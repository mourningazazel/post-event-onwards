#pragma once

// PEO-080: executors for tests. Core's passes must give the same bits whatever order and
// whatever thread runs their pieces; these two make that checkable. Test-only: they
// allocate per run, which the frontend's pool does not.

#include "peo/core/executor.hpp"
#include "peo/core/rng.hpp"

#include <atomic>
#include <cstddef>
#include <numeric>
#include <thread>
#include <vector>

namespace peo::test {

/// Runs every piece on the calling thread, in an order shuffled from a seed per run.
/// width() says 4, so passes split as they would on a pool.
class ShuffledExecutor final : public core::Executor {
public:
    explicit ShuffledExecutor(core::Seed seed) : rng_(seed) {}
    void run(std::size_t pieces, core::PieceFn body) override {
        std::vector<std::size_t> order(pieces);
        std::iota(order.begin(), order.end(), std::size_t{0});
        for (std::size_t i = pieces; i > 1; --i) {
            const auto j = static_cast<std::size_t>(rng_.range(0, static_cast<int>(i) - 1));
            std::swap(order[i - 1], order[j]);
        }
        for (const std::size_t i : order) {
            body(i);
        }
    }
    [[nodiscard]] std::size_t width() const noexcept override { return kWidth; }

private:
    static constexpr std::size_t kWidth = 4;
    core::Rng rng_;
};

/// Real threads: each run starts `threads` std::threads that take pieces from a shared
/// counter, and joins them. Nested runs start their own.
class ThreadedExecutor final : public core::Executor {
public:
    explicit ThreadedExecutor(std::size_t threads) : threads_(threads) {}
    void run(std::size_t pieces, core::PieceFn body) override {
        std::atomic<std::size_t> next{0};
        std::vector<std::thread> workers;
        workers.reserve(threads_);
        for (std::size_t t = 0; t < threads_; ++t) {
            workers.emplace_back([&] {
                for (std::size_t i = next.fetch_add(1); i < pieces; i = next.fetch_add(1)) {
                    body(i);
                }
            });
        }
        for (std::thread& w : workers) {
            w.join();
        }
    }
    [[nodiscard]] std::size_t width() const noexcept override { return threads_; }

private:
    std::size_t threads_;
};

} // namespace peo::test
