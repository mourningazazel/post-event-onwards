// Review probe: scent transport on a cramped (house/office) map vs open.
// Links peo_core for ScentField / Dead. Everything else is local.
#include "peo/core/dead.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <vector>

using namespace peo::core;
using Clock = std::chrono::steady_clock;

constexpr int W = 200, H = 120;

static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// ---------------------------------------------------------------- map
// Streets every 24 x 20 cells (3 wide). Blocks alternate: house (2x2 rooms,
// one front door, doors in partitions) and office (1-wide central corridor,
// 4x3 rooms off it, each with a door; one front door).
static Grid<bool> make_town(Rng& rng, bool cramped) {
    Grid<bool> b(W, H, false);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            b.at(x, y) = x == 0 || y == 0 || x == W - 1 || y == H - 1;
    if (!cramped) return b;
    const int BX = 24, BY = 20, ST = 3;
    int bi = 0;
    for (int by = ST; by + BY - ST <= H - 1; by += BY) {
        for (int bx = ST; bx + BX - ST <= W - 1; bx += BX, ++bi) {
            const int x0 = bx, y0 = by, x1 = bx + BX - ST - 1, y1 = by + BY - ST - 1; // inclusive
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                    b.at(x, y) = (x == x0 || x == x1 || y == y0 || y == y1);
            // front door on the top wall
            b.at(x0 + 1 + rng.range(0, x1 - x0 - 2), y0) = false;
            if (bi % 2 == 0) { // house: cross partition with a door in each arm
                const int mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
                for (int y = y0; y <= y1; ++y) b.at(mx, y) = true;
                for (int x = x0; x <= x1; ++x) b.at(x, my) = true;
                b.at(mx, y0 + 1 + rng.range(0, my - y0 - 2)) = false;
                b.at(mx, my + 1 + rng.range(0, y1 - my - 2)) = false;
                b.at(x0 + 1 + rng.range(0, mx - x0 - 2), my) = false;
                b.at(mx + 1 + rng.range(0, x1 - mx - 2), my) = false;
            } else { // office: corridor at my, rooms 4 wide above and below
                const int my = (y0 + y1) / 2;
                for (int y = y0 + 1; y < y1; ++y)
                    if (y != my) {
                        for (int x = x0 + 1; x < x1; ++x) b.at(x, y) = (y == my - 1 || y == my + 1);
                    }
                for (int x = x0 + 5; x < x1; x += 5) {
                    for (int y = y0 + 1; y < my - 1; ++y) b.at(x, y) = true;
                    for (int y = my + 2; y < y1; ++y) b.at(x, y) = true;
                }
                // doors from corridor into each room
                for (int x = x0 + 1; x < x1; x += 5) {
                    const int dx = x + 1 + rng.range(0, std::min(2, x1 - x - 2));
                    b.at(dx, my - 1) = false;
                    b.at(dx, my + 1) = false;
                }
            }
        }
    }
    return b;
}

static std::vector<int> bfs_dist(const Grid<bool>& b, Vec2i src) {
    std::vector<int> d(W * H, -1);
    std::deque<Vec2i> q{src};
    d[src.y * W + src.x] = 0;
    while (!q.empty()) {
        const Vec2i c = q.front();
        q.pop_front();
        for (const Vec2i dd : kNeighbours8) {
            const Vec2i n = c + dd;
            if (!b.in_bounds(n) || b.at(n) || d[n.y * W + n.x] >= 0) continue;
            d[n.y * W + n.x] = d[c.y * W + c.x] + 1;
            q.push_back(n);
        }
    }
    return d;
}

// Greedy climb of a field from every reachable open cell. Returns the fraction
// that reach `goal` within `limit` moves, and mean path / bfs distance.
struct Climb { double success; double stretch; int frozen; };
template <typename F>
static Climb climb_all(const Grid<bool>& b, const std::vector<int>& dist, Vec2i goal, F sample, int limit = 600) {
    int reachable = 0, ok = 0, frozen = 0;
    double stretch = 0;
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            const int i = y * W + x;
            if (b.at(x, y) || dist[i] <= 0) continue;
            ++reachable;
            Vec2i p{x, y};
            int steps = 0;
            bool stuck = false;
            while (p != goal && steps < limit) {
                float best = sample(p);
                Vec2i next = p;
                for (const Vec2i d : kNeighbours8) {
                    const Vec2i n = p + d;
                    if (!b.in_bounds(n) || b.at(n)) continue;
                    const float v = sample(n);
                    if (v > best) { best = v; next = n; }
                }
                if (next == p) { stuck = true; break; }
                p = next;
                ++steps;
            }
            if (p == goal) { ++ok; stretch += double(steps) / dist[i]; }
            else if (stuck) ++frozen;
        }
    return {double(ok) / reachable, ok ? stretch / ok : 0.0, frozen};
}

// ----------------------------------------------- renormalised diffusion
// Same as ScentField::sweep but the diffused share is split among OPEN
// neighbours only (no loss into walls). Decay is the only loss.
struct Renorm {
    Grid<float> f, back;
    ScentParams p;
    Renorm(ScentParams pp) : f(W, H, 0.f), back(W, H, 0.f), p(pp) {}
    void step(const Grid<bool>& b) {
        const float keep = 1.f - p.decay;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const Vec2i c{x, y};
                if (b.at(c)) { back.at(c) = 0; continue; }
                float in = 0;
                for (const Vec2i d : kNeighbours4) {
                    const Vec2i n = c + d;
                    if (!b.in_bounds(n) || b.at(n)) continue;
                    int open = 0;
                    for (const Vec2i e : kNeighbours4) { const Vec2i m = n + e; open += b.in_bounds(m) && !b.at(m); }
                    in += f.at(n) * (p.diffusion / open);
                }
                float v = (f.at(c) * (1.f - p.diffusion) + in) * keep;
                if (v < p.floor) v = 0;
                back.at(c) = v;
            }
        std::swap(f, back);
    }
};

// ------------------------------------------------- separable IIR (D-008)
struct Iir {
    Grid<float> f;
    float a, g;
    Iir(float lambda) : f(W, H, 0.f), a(std::exp(-1.f / lambda)), g((1 - a) / (1 + a)) {}
    void pass(const Grid<bool>& b) { // rows then columns, accumulator reset at walls
        std::vector<float> yf(std::max(W, H)), yb(std::max(W, H));
        for (int y = 0; y < H; ++y) {
            float acc = 0;
            for (int x = 0; x < W; ++x) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yf[x] = acc; }
            acc = 0;
            for (int x = W - 1; x >= 0; --x) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yb[x] = acc; }
            for (int x = 0; x < W; ++x) f.at(x, y) = b.at(x, y) ? 0.f : g * (yf[x] + yb[x] - f.at(x, y));
        }
        for (int x = 0; x < W; ++x) {
            float acc = 0;
            for (int y = 0; y < H; ++y) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yf[y] = acc; }
            acc = 0;
            for (int y = H - 1; y >= 0; --y) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yb[y] = acc; }
            for (int y = 0; y < H; ++y) f.at(x, y) = b.at(x, y) ? 0.f : g * (yf[y] + yb[y] - f.at(x, y));
        }
    }
};

// ---------------------------------------- propagating max-plus field
// Integer field. value(cell) = strength - age*K - dist*C, realised by a
// frontier that advances `speed` cells per update. Cost is the active set.
struct Wave {
    std::vector<int32_t> val;
    std::vector<int> active, next;
    std::vector<uint32_t> stamp;
    uint32_t turn = 0;
    int K, C, speed;
    Wave(int k, int c, int s) : val(W * H, INT32_MIN / 2), stamp(W * H, 0), K(k), C(c), speed(s) {}
    void deposit(Vec2i p, int strength) {
        const int i = p.y * W + p.x;
        const int32_t v = int32_t(turn) * K + strength;
        if (v > val[i]) { val[i] = v; if (stamp[i] != turn + 1) { stamp[i] = turn + 1; active.push_back(i); } }
    }
    size_t update(const Grid<bool>& b) {
        ++turn;
        size_t work = 0;
        for (int r = 0; r < speed; ++r) {
            next.clear();
            for (const int u : active) {
                ++work;
                const int32_t cand = val[u] - C;
                const int ux = u % W, uy = u / W;
                for (const Vec2i d : kNeighbours8) {
                    const int nx = ux + d.x, ny = uy + d.y;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    const int n = ny * W + nx;
                    if (b.at(nx, ny)) continue;
                    if (cand > val[n] && cand > int32_t(turn) * K - 1000000) { // reach limit: strength exhausted
                        val[n] = cand;
                        if (stamp[n] != turn + 1) { stamp[n] = turn + 1; next.push_back(n); }
                    }
                }
            }
            std::swap(active, next);
        }
        return work;
    }
    float read(Vec2i p) const { const int32_t v = val[p.y * W + p.x] - int32_t(turn) * K; return v > 0 ? float(v) : 0.f; }
};

int main() {
    Rng rng(11);
    for (const bool cramped : {false, true}) {
        Grid<bool> b = make_town(rng, cramped);
        int open = 0;
        for (auto v : b) open += !v;
        // deepest open cell from the map centre-top (a room far from streets): pick
        // the open cell with the largest BFS distance from (100,2) that is not on a street.
        std::vector<int> d0 = bfs_dist(b, {100, 1});
        Vec2i src{100, 60};
        if (cramped) {
            int best = -1;
            for (int y = 1; y < H - 1; ++y) for (int x = 1; x < W - 1; ++x)
                if (!b.at(x, y) && d0[y * W + x] > best && x > 60 && x < 140 && y > 40 && y < 80) { best = d0[y * W + x]; src = {x, y}; }
        }
        std::vector<int> dist = bfs_dist(b, src);
        int reach_cells = 0, maxd = 0;
        for (int v : dist) if (v > 0) { ++reach_cells; maxd = std::max(maxd, v); }
        std::printf("\n=== %s map: %d open cells (%.0f%% walls), source (%d,%d), %d reachable, max geodesic %d\n",
                    cramped ? "CRAMPED" : "OPEN", open, 100.0 * (W * H - open) / (W * H), src.x, src.y, reach_cells, maxd);

        // --- current field: standing source
        {
            ScentField f(W, H);
            auto t0 = Clock::now();
            int steps = 0;
            for (const int T : {10, 50, 100, 300, 1000}) {
                for (; steps < T; ++steps) { f.deposit(src, kPlayerScent); f.step(&b); }
                int nz = 0, gt14 = 0;
                for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (!b.at(x, y) && dist[y * W + x] > 0) {
                    nz += f.sample({x, y}) > 0; gt14 += f.sample({x, y}) > 14.f; }
                const Climb c = climb_all(b, dist, src, [&](Vec2i p) { return f.sample(p); });
                std::printf("  diffusion  T=%4d: reached %5.1f%%  >14: %5.1f%%  climb ok %5.1f%% stretch %.2f frozen %d\n",
                            T, 100.0 * nz / reach_cells, 100.0 * gt14 / reach_cells, 100 * c.success, c.stretch, c.frozen);
            }
            std::printf("  diffusion step cost: %.3f ms/update (dense sweep)\n", ms_since(t0) / steps);
        }
        // --- renormalised diffusion
        {
            Renorm r({});
            int steps = 0;
            for (const int T : {50, 300, 1000}) {
                for (; steps < T; ++steps) { r.f.at(src) += kPlayerScent; r.step(b); }
                int nz = 0, gt14 = 0;
                for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (!b.at(x, y) && dist[y * W + x] > 0) {
                    nz += r.f.at(x, y) > 0; gt14 += r.f.at(x, y) > 14.f; }
                const Climb c = climb_all(b, dist, src, [&](Vec2i p) { return r.f.at(p); });
                std::printf("  renorm     T=%4d: reached %5.1f%%  >14: %5.1f%%  climb ok %5.1f%% stretch %.2f frozen %d\n",
                            T, 100.0 * nz / reach_cells, 100.0 * gt14 / reach_cells, 100 * c.success, c.stretch, c.frozen);
            }
        }
        // --- IIR iterations
        for (const int iters : {1, 2, 3, 4, 6, 10}) {
            Iir ii(4.47f);
            ii.f.at(src) = kPlayerScent;
            auto t0 = Clock::now();
            for (int k = 0; k < iters; ++k) ii.pass(b);
            const double ms = ms_since(t0);
            int nz = 0;
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (!b.at(x, y) && dist[y * W + x] > 0) nz += ii.f.at(x, y) > 0;
            const Climb c = climb_all(b, dist, src, [&](Vec2i p) { return ii.f.at(p); });
            std::printf("  IIR x%-2d: reached %5.1f%%  climb ok %5.1f%% stretch %.2f frozen %d   (%.3f ms)\n",
                        iters, 100.0 * nz / reach_cells, 100 * c.success, c.stretch, c.frozen, ms);
        }
        // --- BFS geodesic field, full rebuild cost
        {
            auto t0 = Clock::now();
            for (int i = 0; i < 100; ++i) { std::vector<int> d = bfs_dist(b, src); if (d[0] == 12345) std::puts("x"); }
            std::printf("  BFS geodesic full rebuild (deque, 8-conn): %.3f ms\n", ms_since(t0) / 100);
            const Climb c = climb_all(b, dist, src, [&](Vec2i p) { const int v = dist[p.y * W + p.x]; return v < 0 ? -1e9f : -float(v); });
            std::printf("  BFS geodesic climb ok %5.1f%% stretch %.2f\n", 100 * c.success, c.stretch);
        }
        // --- propagating max-plus wave: standing then walking source
        {
            Wave w(/*K*/ 8, /*C*/ 8, /*speed*/ 1);
            std::vector<size_t> work;
            for (int t = 0; t < 300; ++t) { w.deposit(src, 500 * 8); work.push_back(w.update(b)); }
            int nz = 0;
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (!b.at(x, y) && dist[y * W + x] > 0) nz += w.read({x, y}) > 0;
            const Climb c = climb_all(b, dist, src, [&](Vec2i p) { return w.read(p); });
            size_t mx = 0, sum = 0; for (size_t v : work) { mx = std::max(mx, v); sum += v; }
            auto t0 = Clock::now();
            for (int t = 0; t < 100; ++t) { w.deposit(src, 500 * 8); w.update(b); }
            std::printf("  wave standing 300 updates: reached %5.1f%% climb ok %5.1f%% stretch %.2f; active cells/update max %zu mean %zu; steady cost %.3f ms/update\n",
                        100.0 * nz / reach_cells, 100 * c.success, c.stretch, mx, sum / work.size(), ms_since(t0) / 100);
            // walking: along a street row 1 (y=1..) eastward 80 cells
            Wave w2(8, 8, 1);
            Vec2i p{20, 1 + (cramped ? 0 : 0)};
            // find a street row: y=1 is open in both maps (border at 0)
            size_t mx2 = 0;
            for (int t = 0; t < 80; ++t) { w2.deposit(p, 500 * 8); mx2 = std::max(mx2, w2.update(b)); if (!b.at(p + Vec2i{1, 0})) p = p + Vec2i{1, 0}; }
            std::printf("  wave walking 80 updates: active cells/update max %zu\n", mx2);
        }
    }

    // --- the Dead: strongest_neighbour cost vs a pow() weighted draw
    {
        Grid<bool> b = make_town(rng, true);
        ScentField f(W, H);
        for (int i = 0; i < 50; ++i) { f.deposit({100, 60}, kPlayerScent); f.step(&b); }
        std::vector<Dead> horde;
        Rng r2(3);
        while (horde.size() < 5000) { Vec2i p{r2.range(1, W - 2), r2.range(1, H - 2)}; if (!b.at(p)) horde.push_back({.pos = p}); }
        auto t0 = Clock::now();
        for (int i = 0; i < 100; ++i) { for (Dead& d : horde) d.cooldown_s = 0; step_horde(horde, f, b, kUpdatePeriodSeconds); }
        std::printf("\n5000 Dead strongest_neighbour: %.3f ms/update\n", ms_since(t0) / 100);
        // weighted draw with powf on 8 neighbours
        volatile float sink = 0;
        t0 = Clock::now();
        for (int i = 0; i < 100; ++i)
            for (const Dead& d : horde) {
                float tot = 0;
                for (const Vec2i dd : kNeighbours8) { const Vec2i n = d.pos + dd; if (!b.at(n)) tot += std::pow(f.sample(n) + 1e-30f, 1.3f); }
                sink += tot;
            }
        std::printf("5000 Dead x 8 powf weights: %.3f ms/update\n", ms_since(t0) / 100);
        // exponent-bits log2 + LUT
        t0 = Clock::now();
        for (int i = 0; i < 100; ++i)
            for (const Dead& d : horde) {
                int tot = 0;
                for (const Vec2i dd : kNeighbours8) { const Vec2i n = d.pos + dd; if (!b.at(n)) { const float v = f.sample(n); uint32_t bits; std::memcpy(&bits, &v, 4); tot += int(bits >> 20); } }
                sink += float(tot);
            }
        std::printf("5000 Dead x 8 log2-bits integer weights: %.3f ms/update\n", ms_since(t0) / 100);
    }
    return 0;
}
