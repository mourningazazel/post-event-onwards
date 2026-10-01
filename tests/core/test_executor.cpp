#include "peo/core/executor.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "test_executors.hpp"
#include "thread_pool.hpp"

using namespace peo::core;

namespace {

/// Every piece of a run of `pieces` was called exactly once.
bool each_once(Executor* executor, std::size_t pieces) {
    std::vector<std::atomic<int>> calls(pieces);
    run_pieces(executor, pieces, [&](std::size_t i) { calls[i].fetch_add(1); });
    for (const std::atomic<int>& c : calls) {
        if (c.load() != 1) {
            return false;
        }
    }
    return true;
}

/// run_ranges covers [0, count) exactly once, in contiguous ranges.
bool ranges_cover(Executor* executor, std::size_t count) {
    std::vector<std::atomic<int>> calls(count);
    run_ranges(executor, count, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            calls[i].fetch_add(1);
        }
    });
    for (const std::atomic<int>& c : calls) {
        if (c.load() != 1) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_SUITE("executor") {
    TEST_CASE("every executor calls every piece exactly once") {
        // PEO-080: the contract every pass relies on, for the serial default (null), the
        // reference, the shuffler, real threads and the frontend's pool.
        constexpr std::size_t kThreads = 4;
        SerialExecutor serial;
        peo::test::ShuffledExecutor shuffled(7);
        peo::test::ThreadedExecutor threaded(kThreads);
        peo::app::ThreadPool pool(kThreads);
        Executor* const executors[] = {nullptr, &serial, &shuffled, &threaded, &pool};
        for (Executor* e : executors) {
            for (const std::size_t pieces :
                 {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{1000}}) {
                CAPTURE(pieces);
                CHECK(each_once(e, pieces));
                CHECK(ranges_cover(e, pieces));
            }
        }
    }

    TEST_CASE("a run inside a piece, or beside another thread's, still runs every piece") {
        // The pool runs a nested run, and a run while another thread holds it, inline
        // on the calling thread: neither may deadlock or drop a piece.
        constexpr std::size_t kOuter = 8;
        constexpr std::size_t kInner = 50;
        peo::app::ThreadPool pool(4);
        std::atomic<int> calls{0};
        pool.run(kOuter, [&](std::size_t) { pool.run(kInner, [&](std::size_t) { calls.fetch_add(1); }); });
        CHECK(calls.load() == static_cast<int>(kOuter * kInner));
        calls = 0;
        std::thread other([&] {
            for (int r = 0; r < 20; ++r) {
                pool.run(kInner, [&](std::size_t) { calls.fetch_add(1); });
            }
        });
        for (int r = 0; r < 20; ++r) {
            pool.run(kInner, [&](std::size_t) { calls.fetch_add(1); });
        }
        other.join();
        CHECK(calls.load() == static_cast<int>(2 * 20 * kInner));
        CHECK(pool.width() == 4);
        CHECK(peo::app::ThreadPool(1).width() == 1);
    }
}
