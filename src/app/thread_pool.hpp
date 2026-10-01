#pragma once

// PEO-080: the frontend's executor (D-035, ADR-0014). Core never creates a thread; this
// pool lends it workers. The thread that calls run() works too, so width() is the
// workers plus one. A run that starts inside a piece, or while another thread's run
// holds the pool (the speculation worker and the main thread may both ask), runs its
// pieces inline on the calling thread: no deadlock, and the same bits either way.

#include "peo/core/executor.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace peo::app {

class ThreadPool final : public core::Executor {
public:
    /// `threads` is how many threads run pieces, the caller's included; 1 means serial.
    explicit ThreadPool(std::size_t threads) {
        const std::size_t workers = threads > 1 ? threads - 1 : 0;
        workers_.reserve(workers);
        for (std::size_t i = 0; i < workers; ++i) {
            workers_.emplace_back([this](const std::stop_token& stop) { work(stop); });
        }
    }
    ~ThreadPool() override {
        for (std::jthread& w : workers_) {
            w.request_stop();
        }
        {
            const std::lock_guard lock(mutex_);
            ++generation_; // wake every worker to see its stop
        }
        wake_.notify_all();
    }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    [[nodiscard]] std::size_t width() const noexcept override { return workers_.size() + 1; }

    void run(std::size_t pieces, core::PieceFn body) override {
        if (pieces <= 1 || workers_.empty() || in_piece()) {
            serial(pieces, body);
            return;
        }
        {
            std::unique_lock lock(mutex_);
            if (active_) {
                lock.unlock(); // another thread holds the pool: run here, without the lock
                serial(pieces, body);
                return;
            }
            active_ = true;
            body_ = &body;
            pieces_ = pieces;
            finished_ = 0;
            next_.store(0, std::memory_order_relaxed);
            ++generation_;
        }
        wake_.notify_all();
        const std::size_t mine = take(body, pieces);
        std::unique_lock lock(mutex_);
        finished_ += mine;
        done_.wait(lock, [&] { return finished_ == pieces_ && working_ == 0; });
        active_ = false;
        body_ = nullptr;
    }

private:
    static bool& in_piece() noexcept {
        thread_local bool inside = false;
        return inside;
    }
    static void serial(std::size_t pieces, core::PieceFn body) {
        for (std::size_t i = 0; i < pieces; ++i) {
            body(i);
        }
    }
    /// Takes pieces from the shared counter until none are left; returns how many it ran.
    std::size_t take(core::PieceFn body, std::size_t pieces) {
        in_piece() = true;
        std::size_t ran = 0;
        for (std::size_t i = next_.fetch_add(1, std::memory_order_relaxed); i < pieces;
             i = next_.fetch_add(1, std::memory_order_relaxed)) {
            body(i);
            ++ran;
        }
        in_piece() = false;
        return ran;
    }
    void work(const std::stop_token& stop) {
        std::uint64_t seen = 0;
        std::unique_lock lock(mutex_);
        while (true) {
            wake_.wait(lock, [&] { return stop.stop_requested() || generation_ != seen; });
            if (stop.stop_requested()) {
                return;
            }
            seen = generation_;
            if (!active_) {
                continue; // woke after that run finished
            }
            const core::PieceFn body = *body_;
            const std::size_t pieces = pieces_;
            ++working_;
            lock.unlock();
            const std::size_t ran = take(body, pieces);
            lock.lock();
            finished_ += ran;
            --working_;
            if (finished_ == pieces_ && working_ == 0) {
                done_.notify_all();
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;
    const core::PieceFn* body_ = nullptr;
    std::size_t pieces_ = 0;
    std::size_t finished_ = 0;
    std::size_t working_ = 0;
    std::uint64_t generation_ = 0;
    bool active_ = false;
    std::atomic<std::size_t> next_{0};
    std::vector<std::jthread> workers_; // last: started after every field above exists
};

} // namespace peo::app
