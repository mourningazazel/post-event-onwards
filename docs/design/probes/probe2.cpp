// Probe 2: debug renorm/IIR ordering near the source, wave with bounded reach,
// dense sweep scaling with map size, and a trail-following scenario.
#include "peo/core/dead.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>
using namespace peo::core;
using Clock = std::chrono::steady_clock;
constexpr int W = 200, H = 120;
static double ms_since(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

static Grid<bool> make_town(Rng& rng) { // identical to probe.cpp's cramped map
    Grid<bool> b(W, H, false);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) b.at(x, y) = x == 0 || y == 0 || x == W - 1 || y == H - 1;
    const int BX = 24, BY = 20, ST = 3; int bi = 0;
    for (int by = ST; by + BY - ST <= H - 1; by += BY)
        for (int bx = ST; bx + BX - ST <= W - 1; bx += BX, ++bi) {
            const int x0 = bx, y0 = by, x1 = bx + BX - ST - 1, y1 = by + BY - ST - 1;
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) b.at(x, y) = (x == x0 || x == x1 || y == y0 || y == y1);
            b.at(x0 + 1 + rng.range(0, x1 - x0 - 2), y0) = false;
            if (bi % 2 == 0) {
                const int mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
                for (int y = y0; y <= y1; ++y) b.at(mx, y) = true;
                for (int x = x0; x <= x1; ++x) b.at(x, my) = true;
                b.at(mx, y0 + 1 + rng.range(0, my - y0 - 2)) = false; b.at(mx, my + 1 + rng.range(0, y1 - my - 2)) = false;
                b.at(x0 + 1 + rng.range(0, mx - x0 - 2), my) = false; b.at(mx + 1 + rng.range(0, x1 - mx - 2), my) = false;
            } else {
                const int my = (y0 + y1) / 2;
                for (int y = y0 + 1; y < y1; ++y) if (y != my) for (int x = x0 + 1; x < x1; ++x) b.at(x, y) = (y == my - 1 || y == my + 1);
                for (int x = x0 + 5; x < x1; x += 5) { for (int y = y0 + 1; y < my - 1; ++y) b.at(x, y) = true; for (int y = my + 2; y < y1; ++y) b.at(x, y) = true; }
                for (int x = x0 + 1; x < x1; x += 5) { const int dx = x + 1 + rng.range(0, std::min(2, x1 - x - 2)); b.at(dx, my - 1) = false; b.at(dx, my + 1) = false; }
            }
        }
    return b;
}
static std::vector<int> bfs_dist(const Grid<bool>& b, Vec2i src, std::vector<int>* parent = nullptr) {
    std::vector<int> d(W * H, -1); if (parent) parent->assign(W * H, -1);
    std::deque<Vec2i> q{src}; d[src.y * W + src.x] = 0;
    while (!q.empty()) { const Vec2i c = q.front(); q.pop_front();
        for (const Vec2i dd : kNeighbours8) { const Vec2i n = c + dd; if (!b.in_bounds(n) || b.at(n) || d[n.y * W + n.x] >= 0) continue;
            d[n.y * W + n.x] = d[c.y * W + c.x] + 1; if (parent) (*parent)[n.y * W + n.x] = c.y * W + c.x; q.push_back(n); } }
    return d;
}
struct Wave {
    std::vector<int32_t> val; std::vector<int> active, next; std::vector<uint32_t> stamp; uint32_t turn = 0; int K, C, speed;
    Wave(int k, int c, int s) : val(W * H, INT32_MIN / 2), stamp(W * H, 0), K(k), C(c), speed(s) {}
    void deposit(Vec2i p, int strength) { const int i = p.y * W + p.x; const int32_t v = int32_t(turn) * K + strength;
        if (v > val[i]) { val[i] = v; if (stamp[i] != turn + 1) { stamp[i] = turn + 1; active.push_back(i); } } }
    size_t update(const Grid<bool>& b) { ++turn; size_t work = 0;
        for (int r = 0; r < speed; ++r) { next.clear();
            for (const int u : active) { ++work; const int32_t cand = val[u] - C; if (cand <= int32_t(turn) * K) continue; // exhausted: reads as 0 anyway
                const int ux = u % W, uy = u / W;
                for (const Vec2i d : kNeighbours8) { const int nx = ux + d.x, ny = uy + d.y; const int n = ny * W + nx;
                    if (b.at(nx, ny)) continue;
                    if (cand > val[n]) { val[n] = cand; if (stamp[n] != turn + 1) { stamp[n] = turn + 1; next.push_back(n); } } } }
            std::swap(active, next); }
        return work; }
    float read(Vec2i p) const { const int32_t v = val[p.y * W + p.x] - int32_t(turn) * K; return v > 0 ? float(v) : 0.f; }
};
struct Iir { Grid<float> f; float a, g; Iir(float l) : f(W, H, 0.f), a(std::exp(-1.f / l)), g((1 - a) / (1 + a)) {}
    void pass(const Grid<bool>& b) { std::vector<float> yf(std::max(W, H)), yb(std::max(W, H));
        for (int y = 0; y < H; ++y) { float acc = 0; for (int x = 0; x < W; ++x) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yf[x] = acc; }
            acc = 0; for (int x = W - 1; x >= 0; --x) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yb[x] = acc; }
            for (int x = 0; x < W; ++x) f.at(x, y) = b.at(x, y) ? 0.f : g * (yf[x] + yb[x] - f.at(x, y)); }
        for (int x = 0; x < W; ++x) { float acc = 0; for (int y = 0; y < H; ++y) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yf[y] = acc; }
            acc = 0; for (int y = H - 1; y >= 0; --y) { acc = b.at(x, y) ? 0.f : f.at(x, y) + a * acc; yb[y] = acc; }
            for (int y = 0; y < H; ++y) f.at(x, y) = b.at(x, y) ? 0.f : g * (yf[y] + yb[y] - f.at(x, y)); } } };

int main() {
    Rng rng(11); Grid<bool> b = make_town(rng);
    std::vector<int> d0 = bfs_dist(b, {100, 1}); Vec2i src{100, 60}; int best = -1;
    for (int y = 41; y < 80; ++y) for (int x = 61; x < 140; ++x) if (!b.at(x, y) && d0[y * W + x] > best) { best = d0[y * W + x]; src = {x, y}; }
    std::vector<int> parent; std::vector<int> dist = bfs_dist(b, src, &parent);
    std::printf("source (%d,%d)\n", src.x, src.y);
    // ---- local map print around source (15x9)
    for (int y = src.y - 4; y <= src.y + 4; ++y) { for (int x = src.x - 7; x <= src.x + 7; ++x) std::putchar(Vec2i{x, y} == src ? '@' : b.at(x, y) ? '#' : '.'); std::putchar('\n'); }
    // ---- IIR x3: where is the max? is the source a local max?
    { Iir ii(4.47f); ii.f.at(src) = kPlayerScent; for (int k = 0; k < 3; ++k) ii.pass(b);
      float mx = 0; Vec2i at{}; for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (ii.f.at(x, y) > mx) { mx = ii.f.at(x, y); at = {x, y}; }
      std::printf("IIR x3: value at source %.3g, field max %.3g at (%d,%d) geodesic %d from source\n", ii.f.at(src), mx, at.x, at.y, dist[at.y * W + at.x]);
      int localmax = 0; for (int y = 1; y < H - 1; ++y) for (int x = 1; x < W - 1; ++x) if (!b.at(x, y) && ii.f.at(x, y) > 0) { bool lm = true; for (const Vec2i d : kNeighbours8) { const Vec2i n = Vec2i{x, y} + d; if (!b.at(n) && ii.f.at(n) > ii.f.at(x, y)) lm = false; } localmax += lm; }
      std::printf("IIR x3: %d local maxima among nonzero cells (a climber freezes on each)\n", localmax); }
    // ---- dense sweep scaling with map size
    for (const int side : {200, 512, 1024}) { ScentField f(side, side); Grid<bool> bb(side, side, false); f.deposit({side / 2, side / 2}, 500); auto t0 = Clock::now(); for (int i = 0; i < 20; ++i) f.step(&bb); std::printf("dense sweep %dx%d: %.2f ms/update\n", side, side, ms_since(t0) / 20); }
    // ---- wave with bounded reach: standing source, cost vs reach
    for (const int reach : {30, 60, 120}) { Wave w(8, 8, 1); size_t mx = 0; for (int t = 0; t < 400; ++t) { w.deposit(src, reach * 8); mx = std::max(mx, w.update(b)); }
        auto t0 = Clock::now(); for (int t = 0; t < 200; ++t) { w.deposit(src, reach * 8); w.update(b); }
        int nz = 0; for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (!b.at(x, y)) nz += w.read({x, y}) > 0;
        std::printf("wave standing, reach %3d cells: cells with scent %5d, active/update max %5zu, steady %.3f ms/update\n", reach, nz, mx, ms_since(t0) / 200); }
    // ---- trail scenario: player walks from a street cell into the deep room; a Dead follows the trail.
    // Path: BFS parent chain from a street cell (x=src.x-? pick (src.x, 2)) to src.
    { Vec2i start{src.x, 2}; std::vector<int> pdist = bfs_dist(b, start, &parent); std::vector<Vec2i> path; for (int c = src.y * W + src.x; c >= 0 && Vec2i{c % W, c / W} != start; c = parent[c]) path.push_back({c % W, c / W}); std::reverse(path.begin(), path.end());
      std::printf("trail: %zu steps from (%d,%d) to the room\n", path.size(), start.x, start.y);
      for (const int lag : {1, 5, 15, 30}) {
        // current field: player walks one cell per update depositing 500, then waits 200 updates. Dead starts at `start` `lag` updates after the player.
        ScentField f(W, H); Dead unit{.pos = start}; Vec2i player = start; size_t k = 0; int t = 0; int arrive = -1; int moves = 0;
        for (; t < int(path.size()) + 300; ++t) { if (k < path.size()) player = path[k++]; f.deposit(player, kPlayerScent); f.step(&b);
            if (t >= lag) { unit.cooldown_s = 0; moves += step_dead(unit, f, b, 6); if (arrive < 0 && std::max(std::abs(unit.pos.x - player.x), std::abs(unit.pos.y - player.y)) <= 1) arrive = t; } }
        std::vector<int> dp = bfs_dist(b, player);
        if (lag == 1) { std::printf("  field along the trail at the end (path index: value), Dead stopped at index "); for (size_t i = 0; i < path.size(); ++i) if (path[i] == unit.pos) std::printf("%zu", i); std::printf("\n   "); for (size_t i = 18; i < 44; ++i) std::printf("%zu:%.2g ", i, f.sample(path[i])); std::printf("\n"); }
        std::printf("  diffusion, Dead lags %2d updates: moved %3d times, ends %3d cells (geodesic) from player at (%d,%d); adjacent at update %d\n", lag, moves, dp[unit.pos.y * W + unit.pos.x], unit.pos.x, unit.pos.y, arrive);
        Wave w(8, 8, 1); Dead u2{.pos = start}; player = start; k = 0; arrive = -1; moves = 0;
        for (t = 0; t < int(path.size()) + 300; ++t) { if (k < path.size()) player = path[k++]; w.deposit(player, 120 * 8); w.update(b);
            if (t >= lag) { float bestv = w.read(u2.pos); Vec2i nxt = u2.pos; for (const Vec2i d : kNeighbours8) { const Vec2i n = u2.pos + d; if (!b.at(n) && w.read(n) > bestv) { bestv = w.read(n); nxt = n; } } moves += nxt != u2.pos; u2.pos = nxt; if (arrive < 0 && std::max(std::abs(u2.pos.x - player.x), std::abs(u2.pos.y - player.y)) <= 1) arrive = t; } }
        std::printf("  wave (60 cells), Dead lags %2d updates: moved %3d times, ends %3d cells from player at (%d,%d); adjacent at update %d\n", lag, moves, dp[u2.pos.y * W + u2.pos.x], u2.pos.x, u2.pos.y, arrive);
      } }
    return 0;
}
