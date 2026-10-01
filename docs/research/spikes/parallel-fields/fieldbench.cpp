// Measurement spike for docs/research/parallel-and-gpu.md. Throwaway code, not the game.
//
// Compares the game's ScentWave (sparse push, one thread) with a dense "pull" form of
// the same update (every open cell takes the max over its neighbours' offers), run
// whole-stage, over active 128x8 tiles only, and over active tiles on several threads.
// Every variant is checked bit for bit against ScentWave on every update. Then it
// compares the Dead's decision (strongest_neighbour per unit) with a precomputed
// flow-direction grid, sequential and on threads with a per-tile min claim, and
// checks the moves are identical.
//
//   g++ -std=c++20 -O3 -march=x86-64-v3 -I src/core/include \
//       docs/research/spikes/parallel-fields/fieldbench.cpp \
//       build/headless-release/src/core/libpeo_core.a -pthread -o fieldbench
//   ./fieldbench

#include "peo/core/dead.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent_wave.hpp"
#include "peo/core/stage.hpp"

#include "../../../../tests/core/town_fixture.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <thread>
#include <vector>

#if defined(__x86_64__)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#else
#define CPU_PAUSE() std::this_thread::yield()
#endif

using namespace peo::core;
using Clock = std::chrono::steady_clock;

namespace {

constexpr std::int32_t kUnreached = std::numeric_limits<std::int32_t>::min() / 2;
#ifndef TILE_W
#define TILE_W 128
#define TILE_H 8
#endif
constexpr int kTileW = TILE_W;
constexpr int kTileH = TILE_H;

double us_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
}

struct Stats {
    std::vector<double> v;
    void add(double x) { v.push_back(x); }
    double pct(double p) {
        std::sort(v.begin(), v.end());
        return v.empty() ? 0 : v[std::min(v.size() - 1, static_cast<std::size_t>(p * v.size()))];
    }
};

// A spinning pool: lowest wake latency, for measuring. A game would use a job system.
class Pool {
public:
    explicit Pool(int n) : n_(n) {
        for (int i = 1; i < n_; ++i) {
            threads_.emplace_back([this, i] { work(i); });
        }
    }
    ~Pool() {
        quit_ = true;
        gen_.fetch_add(1);
        for (auto& t : threads_) {
            t.join();
        }
    }
    int size() const { return n_; }
    void run(const std::function<void(int)>& f) {
        job_ = &f;
        done_.store(0);
        gen_.fetch_add(1, std::memory_order_release);
        f(0);
        while (done_.load(std::memory_order_acquire) != n_ - 1) {
            CPU_PAUSE();
        }
    }

private:
    void work(int i) {
        std::uint64_t seen = 0;
        for (;;) {
            while (gen_.load(std::memory_order_acquire) == seen) {
                CPU_PAUSE();
            }
            seen = gen_.load();
            if (quit_) {
                return;
            }
            (*job_)(i);
            done_.fetch_add(1, std::memory_order_release);
        }
    }
    int n_;
    std::vector<std::thread> threads_;
    std::atomic<std::uint64_t> gen_{0};
    std::atomic<int> done_{0};
    std::atomic<bool> quit_{false};
    const std::function<void(int)>* job_ = nullptr;
};

// The same field as ScentWave, as a pull over two buffers. Bit-identical for speed 1:
// a round is a max over offers from round-start values, and a cell that did not
// change last round has already offered everything it can (values only rise).
class DenseWave {
public:
    DenseWave(const Grid<bool>& blocked, WaveParams p)
        : p_(p), w_(blocked.width()), h_(blocked.height()), tw_((w_ + kTileW - 1) / kTileW),
          th_((h_ + kTileH - 1) / kTileH) {
        const std::size_t n = static_cast<std::size_t>(w_) * h_;
        buf_[0].assign(n, kUnreached);
        buf_[1].assign(n, kUnreached);
        open_.assign(n, 0);
        diag_.assign(n, 0);
        for (int y = 1; y < h_ - 1; ++y) {
            for (int x = 1; x < w_ - 1; ++x) {
                if (blocked.at(x, y)) {
                    continue;
                }
                open_[idx(x, y)] = 1;
                // Bit k: the diagonal kNeighbours8[2k+1] may offer into this cell; the
                // step is symmetric, so both cells beside it must be open.
                std::uint8_t m = 0;
                for (int k = 0; k < 4; ++k) {
                    const Vec2i d = kNeighbours8[2 * k + 1];
                    if (!blocked.at(x + d.x, y) && !blocked.at(x, y + d.y)) {
                        m |= static_cast<std::uint8_t>(1U << k);
                    }
                }
                diag_[idx(x, y)] = m;
            }
        }
        tile_active_.assign(static_cast<std::size_t>(tw_) * th_, 0);
        tile_changed_.assign(tile_active_.size(), 0);
        dir_.assign(n, 8);
    }

    std::size_t idx(int x, int y) const { return static_cast<std::size_t>(y) * w_ + x; }
    std::int32_t age_line() const { return p_.age_cost * static_cast<std::int32_t>(updates_); }
    const std::vector<std::int32_t>& values() const { return buf_[cur_]; }
    const std::vector<std::uint8_t>& dir() const { return dir_; }
    std::size_t active_tiles() const { return active_.size(); }

    void deposit(Vec2i at, std::int32_t s) {
        const std::size_t i = idx(at.x, at.y);
        const std::int32_t v = age_line() + s;
        if (v > buf_[cur_][i]) {
            buf_[cur_][i] = v;
            tile_changed_[tile_of(at.x, at.y)] = 1;
        }
    }

    // One update over every interior cell.
    void update_full() {
        ++updates_;
        const auto& a = buf_[cur_];
        auto& b = buf_[cur_ ^ 1];
        for (int y = 1; y < h_ - 1; ++y) {
            pull_row(a, b, y, 1, w_ - 1);
        }
        cur_ ^= 1;
    }

    // One update over the tiles that changed last round and their neighbours. With a
    // pool, the tiles are split statically across threads: each writes only its own
    // tiles, reads only the round-start buffer, so the result does not depend on it.
    void update_tiles(Pool* pool) {
        ++updates_;
        expand(active_);
        std::fill(tile_changed_.begin(), tile_changed_.end(), 0);
        const auto& a = buf_[cur_];
        auto& b = buf_[cur_ ^ 1];
        auto body = [&](int t) {
            const int n = pool ? pool->size() : 1;
            const std::size_t lo = active_.size() * t / n;
            const std::size_t hi = active_.size() * (t + 1) / n;
            for (std::size_t k = lo; k < hi; ++k) {
                tile_changed_[active_[k]] = pull_tile(a, b, active_[k]);
            }
        };
        if (pool) {
            const std::function<void(int)> f = body;
            pool->run(f);
        } else {
            body(0);
        }
        cur_ ^= 1;
    }

    // Flow direction: for each open cell in an active tile, the first neighbour (in
    // kNeighbours8 order) with the highest stored value above the cell's own; 8 when
    // none. Age-line invariant: the Dead check value[target] > age line at query time.
    // Only cells whose own value or a neighbour's changed this round can turn: the
    // tiles that changed and their neighbours.
    void update_dir_tiles(Pool* pool) {
        const auto& a = buf_[cur_];
        expand(dir_active_);
        auto body = [&](int t) {
            const int n = pool ? pool->size() : 1;
            const std::size_t lo = dir_active_.size() * t / n;
            const std::size_t hi = dir_active_.size() * (t + 1) / n;
            for (std::size_t k = lo; k < hi; ++k) {
                dir_tile(a, dir_active_[k]);
            }
        };
        if (pool) {
            const std::function<void(int)> f = body;
            pool->run(f);
        } else {
            body(0);
        }
    }
    void update_dir_full() {
        const auto& a = buf_[cur_];
        for (int y = 1; y < h_ - 1; ++y) {
            dir_row(a, y, 1, w_ - 1);
        }
    }

private:
    std::size_t tile_of(int x, int y) const { return static_cast<std::size_t>(y / kTileH) * tw_ + x / kTileW; }

    // The tiles that changed, and their neighbours, into `out`.
    void expand(std::vector<std::uint32_t>& out) {
        std::fill(tile_active_.begin(), tile_active_.end(), 0);
        for (int ty = 0; ty < th_; ++ty) {
            for (int tx = 0; tx < tw_; ++tx) {
                if (!tile_changed_[static_cast<std::size_t>(ty) * tw_ + tx]) {
                    continue;
                }
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int x = tx + dx;
                        const int y = ty + dy;
                        if (x >= 0 && y >= 0 && x < tw_ && y < th_) {
                            tile_active_[static_cast<std::size_t>(y) * tw_ + x] = 1;
                        }
                    }
                }
            }
        }
        out.clear();
        for (std::size_t t = 0; t < tile_active_.size(); ++t) {
            if (tile_active_[t]) {
                out.push_back(static_cast<std::uint32_t>(t));
            }
        }
    }

    bool pull_tile(const std::vector<std::int32_t>& a, std::vector<std::int32_t>& b, std::uint32_t t) {
        const int tx = static_cast<int>(t % tw_) * kTileW;
        const int ty = static_cast<int>(t / tw_) * kTileH;
        const int x0 = std::max(tx, 1);
        const int x1 = std::min(tx + kTileW, w_ - 1);
        bool changed = false;
        for (int y = std::max(ty, 1); y < std::min(ty + kTileH, h_ - 1); ++y) {
            changed |= pull_row(a, b, y, x0, x1);
        }
        return changed;
    }

    // The kernel: branch-free over a row segment, so it vectorises.
    bool pull_row(const std::vector<std::int32_t>& av, std::vector<std::int32_t>& bv, int y, int x0, int x1) {
        const std::int32_t* __restrict a = av.data();
        std::int32_t* __restrict b = bv.data();
        const std::uint8_t* __restrict open = open_.data();
        const std::uint8_t* __restrict diag = diag_.data();
        const std::int32_t c = p_.distance_cost;
        const std::int32_t line = age_line();
        const std::ptrdiff_t W = w_;
        int changed = 0;
        for (std::ptrdiff_t i = y * W + x0; i < y * W + x1; ++i) {
            const std::int32_t m = diag[i];
            std::int32_t best = std::max(std::max(a[i - W], a[i + 1]), std::max(a[i + W], a[i - 1]));
            best = std::max(best, (m & 1) ? a[i - W + 1] : kUnreached);
            best = std::max(best, (m & 2) ? a[i + W + 1] : kUnreached);
            best = std::max(best, (m & 4) ? a[i + W - 1] : kUnreached);
            best = std::max(best, (m & 8) ? a[i - W - 1] : kUnreached);
            const std::int32_t cand = best - c;
            const std::int32_t old = a[i];
            const std::int32_t v = (open[i] && cand > line && cand > old) ? cand : old;
            b[i] = v;
            changed |= v != old;
        }
        return changed != 0;
    }

    void dir_tile(const std::vector<std::int32_t>& a, std::uint32_t t) {
        const int tx = static_cast<int>(t % tw_) * kTileW;
        const int ty = static_cast<int>(t / tw_) * kTileH;
        for (int y = std::max(ty, 1); y < std::min(ty + kTileH, h_ - 1); ++y) {
            dir_row(a, y, std::max(tx, 1), std::min(tx + kTileW, w_ - 1));
        }
    }
    // Branch-free so it vectorises: the best value first, then the first direction
    // holding it. Walls hold kUnreached, so only the diagonal corner rule needs a mask.
    void dir_row(const std::vector<std::int32_t>& av, int y, int x0, int x1) {
        const std::int32_t* __restrict a = av.data();
        const std::uint8_t* __restrict open = open_.data();
        const std::uint8_t* __restrict diag = diag_.data();
        std::uint8_t* __restrict dir = dir_.data();
        const std::ptrdiff_t W = w_;
        for (std::ptrdiff_t i = y * W + x0; i < y * W + x1; ++i) {
            const std::int32_t m = diag[i];
            const std::int32_t v0 = a[i - W];
            const std::int32_t v1 = (m & 1) ? a[i - W + 1] : kUnreached;
            const std::int32_t v2 = a[i + 1];
            const std::int32_t v3 = (m & 2) ? a[i + W + 1] : kUnreached;
            const std::int32_t v4 = a[i + W];
            const std::int32_t v5 = (m & 4) ? a[i + W - 1] : kUnreached;
            const std::int32_t v6 = a[i - 1];
            const std::int32_t v7 = (m & 8) ? a[i - W - 1] : kUnreached;
            const std::int32_t best =
                std::max(std::max(std::max(v0, v1), std::max(v2, v3)), std::max(std::max(v4, v5), std::max(v6, v7)));
            std::int32_t d = 8;
            d = v7 == best ? 7 : d;
            d = v6 == best ? 6 : d;
            d = v5 == best ? 5 : d;
            d = v4 == best ? 4 : d;
            d = v3 == best ? 3 : d;
            d = v2 == best ? 2 : d;
            d = v1 == best ? 1 : d;
            d = v0 == best ? 0 : d;
            dir[i] = static_cast<std::uint8_t>((open[i] && best > a[i]) ? d : 8);
        }
    }

    WaveParams p_;
    int w_, h_, tw_, th_;
    std::vector<std::int32_t> buf_[2];
    int cur_ = 0;
    std::vector<std::uint8_t> open_, diag_, tile_active_, tile_changed_, dir_;
    std::vector<std::uint32_t> active_, dir_active_;
    std::uint32_t updates_ = 0;
};

Grid<bool> open_stage(int w, int h) {
    StageSpec s = stage_spec(7, 0);
    s.width = w;
    s.height = h;
    return generate_stage(s).blocked;
}

// The PEO-035 town (houses, offices, one-wide corridors) tiled to any size.
Grid<bool> big_town(int w, int h) {
    Rng rng(11);
    const Grid<bool> town = peo::test::make_town(rng, true);
    const int iw = town.width() - 2;
    const int ih = town.height() - 2;
    Grid<bool> b(w, h, true);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            b.at(x, y) = town.at(1 + (x - 1) % iw, 1 + (y - 1) % ih);
        }
    }
    return b;
}

std::vector<Vec2i> pick_open(const Grid<bool>& b, std::size_t n, std::uint64_t seed, bool unique) {
    Rng rng(seed);
    std::vector<Vec2i> out;
    Grid<bool> used(b.width(), b.height(), false);
    while (out.size() < n) {
        const Vec2i c{rng.range(1, b.width() - 2), rng.range(1, b.height() - 2)};
        if (!b.at(c) && !(unique && used.at(c))) {
            used.at(c) = true;
            out.push_back(c);
        }
    }
    return out;
}

// A player walking one tile per update (D-015), a random walk over open cells.
struct Walker {
    Vec2i pos;
    Rng rng{3};
    void step(const Grid<bool>& b) {
        for (int tries = 0; tries < 16; ++tries) {
            const Vec2i n = pos + kNeighbours8[rng.range(0, 7)];
            if (!b.at(n)) {
                pos = n;
                return;
            }
        }
    }
};

struct FieldResult {
    double sparse_med, sparse_p95, full_med, tiles_med, tiles_p95, mt_med, mt_p95, copy_med;
    double dir_tiles_med, dir_mt_med;
    std::size_t active_cells, active_tiles;
    bool equal;
};

// Run the field `warm` updates, then time `samples` more. Every update checks the dense
// variants against ScentWave.
FieldResult run_field(const Grid<bool>& b, int emitters_per_1000, int warm, int samples, Pool& pool) {
    const WaveParams p{};
    ScentWave ref(b.width(), b.height(), p);
    ScentWave copy_target(b.width(), b.height(), p);
    DenseWave full(b, p), tiles(b, p), mt(b, p);
    const auto emitters =
        pick_open(b, static_cast<std::size_t>(b.width()) * b.height() * emitters_per_1000 / 1000, 5, false);
    Walker walker{pick_open(b, 1, 9, false)[0]};
    Stats s_sparse, s_full, s_tiles, s_mt, s_copy, s_dir, s_dirmt;
    bool equal = true;
    for (int u = 0; u < warm + samples; ++u) {
        walker.step(b);
        const bool timed = u >= warm;
        auto dep = [&](auto& f) {
            f.deposit(walker.pos, p.strength);
            for (const Vec2i e : emitters) {
                f.deposit(e, p.strength);
            }
        };
        dep(ref);
        dep(full);
        dep(tiles);
        dep(mt);
        auto t0 = Clock::now();
        copy_target = ref; // what speculate() pays today before it updates
        if (timed) s_copy.add(us_since(t0));
        t0 = Clock::now();
        ref.update(b);
        if (timed) s_sparse.add(us_since(t0));
        // The full-stage pass costs the same every update: sample it sparsely.
        t0 = Clock::now();
        full.update_full();
        if (timed) s_full.add(us_since(t0));
        t0 = Clock::now();
        tiles.update_tiles(nullptr);
        if (timed) s_tiles.add(us_since(t0));
        t0 = Clock::now();
        tiles.update_dir_tiles(nullptr);
        if (timed) s_dir.add(us_since(t0));
        t0 = Clock::now();
        mt.update_tiles(&pool);
        if (timed) s_mt.add(us_since(t0));
        t0 = Clock::now();
        mt.update_dir_tiles(&pool);
        if (timed) s_dirmt.add(us_since(t0));
        full.update_dir_full(); // untimed: the reference for the incremental grids
        equal = equal && ref.values() == full.values() && ref.values() == tiles.values() &&
                ref.values() == mt.values() && tiles.dir() == full.dir() && mt.dir() == full.dir();
    }
    return {s_sparse.pct(0.5), s_sparse.pct(0.95), s_full.pct(0.5), s_tiles.pct(0.5), s_tiles.pct(0.95),
            s_mt.pct(0.5),     s_mt.pct(0.95),     s_copy.pct(0.5), s_dir.pct(0.5),   s_dirmt.pct(0.5),
            ref.active_cells(), tiles.active_tiles(), equal};
}

// The Dead's decision for one batch (one second's slot, D-031), three ways. Returns
// whether all three produced the same moves.
struct DeadResult {
    double ref_ns, flow_ns, mt_ns;
    std::size_t moves;
    bool equal;
};

DeadResult run_dead(const Grid<bool>& b, std::size_t n_dead, Pool& pool) {
    const WaveParams p{};
    ScentWave ref(b.width(), b.height(), p);
    DenseWave dense(b, p);
    const auto emitters = pick_open(b, static_cast<std::size_t>(b.width()) * b.height() * 6 / 1000, 5, false);
    for (int u = 0; u < 80; ++u) {
        for (const Vec2i e : emitters) {
            ref.deposit(e, p.strength);
            dense.deposit(e, p.strength);
        }
        ref.update(b);
        dense.update_tiles(nullptr);
    }
    dense.update_dir_full();
    const auto pos = pick_open(b, n_dead, 21, true);
    Grid<std::uint8_t> occupied(b.width(), b.height(), 0);
    for (const Vec2i c : pos) {
        occupied.at(c) = 1;
    }
    std::vector<Dead> horde(pos.size());
    for (std::size_t i = 0; i < pos.size(); ++i) {
        horde[i].pos = pos[i];
    }
    const std::int32_t line = p.age_cost * static_cast<std::int32_t>(ref.updates());
    const auto& val = dense.values();
    const auto& dir = dense.dir();
    const int W = b.width();
    const std::ptrdiff_t off[9] = {-W, -W + 1, 1, W + 1, W, W - 1, -1, -W - 1, 0};
    auto target_of = [&](Vec2i c) -> std::int64_t {
        const std::size_t i = static_cast<std::size_t>(c.y) * W + c.x;
        const std::uint8_t d = dir[i];
        if (d == 8) return -1;
        const std::size_t t = i + off[d];
        return val[t] > line ? static_cast<std::int64_t>(t) : -1;
    };

    std::vector<std::uint32_t> ref_moves, flow_moves, mt_moves; // target per unit, ~0 none
    ref_moves.assign(horde.size(), ~0U);
    flow_moves.assign(horde.size(), ~0U);
    mt_moves.assign(horde.size(), ~0U);
    Grid<bool> reserved(b.width(), b.height(), false);
    Stats s_ref, s_flow, s_mt;
    std::vector<std::uint32_t> claim(static_cast<std::size_t>(W) * b.height(), ~0U);
    std::vector<std::int64_t> intent(horde.size());

    for (int rep = 0; rep < 7; ++rep) {
        // Reference: decide_move in ascending unit order, reserving as it goes.
        reserved.fill(false);
        auto t0 = Clock::now();
        for (std::size_t u = 0; u < horde.size(); ++u) {
            if (const auto to = decide_move(horde[u], ref, b, occupied, reserved)) {
                reserved.at(*to) = true;
                ref_moves[u] = static_cast<std::uint32_t>(to->y * W + to->x);
            }
        }
        s_ref.add(us_since(t0) * 1000.0 / horde.size());

        // Flow grid, sequential.
        reserved.fill(false);
        t0 = Clock::now();
        for (std::size_t u = 0; u < horde.size(); ++u) {
            const std::int64_t t = target_of(horde[u].pos);
            if (t >= 0 && occupied.data()[t] == 0 && !reserved.data()[t]) {
                reserved.data()[t] = 1;
                flow_moves[u] = static_cast<std::uint32_t>(t);
            }
        }
        s_flow.add(us_since(t0) * 1000.0 / horde.size());

        // Flow grid on threads: intents, then each tile goes to its lowest claimant
        // (what ascending order gives: a calm Dead never retries elsewhere, D-031).
        t0 = Clock::now();
        const std::function<void(int)> intents = [&](int t) {
            const std::size_t lo = horde.size() * t / pool.size(), hi = horde.size() * (t + 1) / pool.size();
            for (std::size_t u = lo; u < hi; ++u) {
                std::int64_t tg = target_of(horde[u].pos);
                if (tg >= 0 && occupied.data()[tg] != 0) tg = -1;
                intent[u] = tg;
                if (tg >= 0) {
                    std::atomic_ref<std::uint32_t> c(claim[static_cast<std::size_t>(tg)]);
                    std::uint32_t cur = c.load(std::memory_order_relaxed);
                    while (u < cur && !c.compare_exchange_weak(cur, static_cast<std::uint32_t>(u),
                                                               std::memory_order_relaxed)) {
                    }
                }
            }
        };
        pool.run(intents);
        const std::function<void(int)> resolve = [&](int t) {
            const std::size_t lo = horde.size() * t / pool.size(), hi = horde.size() * (t + 1) / pool.size();
            for (std::size_t u = lo; u < hi; ++u) {
                const std::int64_t tg = intent[u];
                mt_moves[u] = (tg >= 0 && claim[static_cast<std::size_t>(tg)] == u) ? static_cast<std::uint32_t>(tg)
                                                                                     : ~0U;
            }
        };
        pool.run(resolve);
        // Reset only the claimed cells (in a real tick: the reservation pass).
        const std::function<void(int)> reset = [&](int t) {
            const std::size_t lo = horde.size() * t / pool.size(), hi = horde.size() * (t + 1) / pool.size();
            for (std::size_t u = lo; u < hi; ++u) {
                if (intent[u] >= 0) claim[static_cast<std::size_t>(intent[u])] = ~0U;
            }
        };
        pool.run(reset);
        s_mt.add(us_since(t0) * 1000.0 / horde.size());
    }
    std::size_t moves = 0;
    for (const auto m : ref_moves) {
        moves += m != ~0U;
    }
    return {s_ref.pct(0.5), s_flow.pct(0.5), s_mt.pct(0.5), moves, ref_moves == flow_moves && ref_moves == mt_moves};
}

} // namespace

int main(int argc, char** argv) {
    const int threads = argc > 1 ? std::atoi(argv[1]) : static_cast<int>(std::thread::hardware_concurrency());
    Pool pool(threads);
    std::printf("threads=%d\n", threads);
    std::printf("\n# scent update, us (median unless noted); each dense variant checked bit for bit\n");
    std::printf("%-22s %4s %9s %9s %9s %8s %8s %8s %8s %8s %8s %9s %6s %5s\n", "map", "emit", "sparse", "sparse95",
                "full", "tiles", "tiles95", "tilesMT", "MT95", "copy", "dir", "dirMT", "tiles", "equal");
    struct Case {
        const char* name;
        int w, h, emit;
        bool town;
    };
    const Case cases[] = {
        {"open 512x512", 512, 512, 0, false},    {"open 512x512", 512, 512, 6, false},
        {"town 512x512", 512, 512, 0, true},     {"town 512x512", 512, 512, 6, true},
        {"open 1024x1024", 1024, 1024, 6, false}, {"town 1024x1024", 1024, 1024, 6, true},
        {"open 2048x2048", 2048, 2048, 0, false}, {"open 2048x2048", 2048, 2048, 6, false},
        {"town 2048x2048", 2048, 2048, 6, true},
    };
    const char* only = argc > 2 ? argv[2] : nullptr; // run only cases whose name contains this
    for (const Case& c : cases) {
        if (only && !std::strstr(c.name, only)) {
            continue;
        }
        const Grid<bool> b = c.town ? big_town(c.w, c.h) : open_stage(c.w, c.h);
        const FieldResult r = run_field(b, c.emit, 80, 40, pool);
        std::printf("%-22s %4d %9.0f %9.0f %9.0f %8.0f %8.0f %8.0f %8.0f %8.0f %8.0f %9.0f %6zu %5s\n", c.name,
                    c.emit, r.sparse_med, r.sparse_p95, r.full_med, r.tiles_med, r.tiles_p95, r.mt_med, r.mt_p95,
                    r.copy_med, r.dir_tiles_med, r.dir_mt_med, r.active_tiles, r.equal ? "yes" : "NO");
    }
    std::printf("\n# one batch of the Dead deciding, ns per unit (median of 7)\n");
    std::printf("%-22s %9s %9s %9s %9s %9s %6s\n", "map", "dead", "decide", "flow", "flowMT", "moves", "equal");
    struct DCase {
        const char* name;
        int w, h;
        std::size_t dead;
    };
    const DCase dcases[] = {
        {"open 512x512", 512, 512, 50000},
        {"open 1024x1024", 1024, 1024, 200000},
        {"open 2048x2048", 2048, 2048, 1000000},
        {"town 2048x2048", 2048, 2048, 1000000},
    };
    for (const DCase& c : dcases) {
        if (only && !std::strstr(c.name, only)) {
            continue;
        }
        const Grid<bool> b = std::strcmp(c.name, "town 2048x2048") == 0 ? big_town(c.w, c.h) : open_stage(c.w, c.h);
        const DeadResult r = run_dead(b, c.dead, pool);
        std::printf("%-22s %9zu %9.1f %9.1f %9.1f %9zu %6s\n", c.name, c.dead, r.ref_ns, r.flow_ns, r.mt_ns, r.moves,
                    r.equal ? "yes" : "NO");
    }
    return 0;
}
