// Probe 3: wind in the geodesic wave field; 512x512 scaling with 50k Dead.
#include "peo/core/dead.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <vector>
using namespace peo::core;
using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

// Wave with wind. openness[cell] in 0..1 scales the wind (0 indoors). Cost per
// step = C - w*dot(wind, dir)*openness (downwind cheaper, upwind dearer), and
// within one update a cell reached this update may pass scent on again only
// downwind, up to `gust` rounds, so the front runs ahead of a walker downwind
// and one cell per update otherwise.
struct WindWave {
    int W, H; std::vector<int32_t> val; std::vector<int> active, next; std::vector<uint32_t> stamp; std::vector<uint8_t> open; std::vector<float> openness;
    uint32_t turn = 0; int K, C, gust; float wx, wy, w;
    WindWave(int w_, int h_, int k, int c, int g, float windx, float windy, float strength)
        : W(w_), H(h_), val(w_ * h_, INT32_MIN / 2), stamp(w_ * h_, 0), open(w_ * h_, 1), openness(w_ * h_, 1.f), K(k), C(c), gust(g), wx(windx), wy(windy), w(strength) {}
    void deposit(int x, int y, int s) { const int i = y * W + x; const int32_t v = int32_t(turn) * K + s; if (v > val[i]) { val[i] = v; if (stamp[i] != turn + 1) { stamp[i] = turn + 1; active.push_back(i); } } }
    size_t update() { ++turn; size_t work = 0;
        for (int r = 0; r < gust; ++r) { next.clear();
            for (const int u : active) { ++work; const int ux = u % W, uy = u / W; const float op = openness[u];
                for (const Vec2i d : kNeighbours8) {
                    const float down = (d.x * wx + d.y * wy) * op; // >0 downwind
                    if (r > 0 && down <= 0) continue;               // gust rounds only run downwind
                    const int cost = std::max(1, int(C - w * down));
                    const int nx = ux + d.x, ny = uy + d.y; if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue; const int n = ny * W + nx; if (!open[n]) continue;
                    const int32_t cand = val[u] - cost; if (cand <= int32_t(turn) * K) continue;
                    if (cand > val[n]) { val[n] = cand; if (stamp[n] != turn + 1) { stamp[n] = turn + 1; next.push_back(n); } } } }
            std::swap(active, next); }
        return work; }
    int read(int x, int y) const { const int32_t v = val[y * W + x] - int32_t(turn) * K; return v > 0 ? v : 0; }
};

int main() {
    // ---- wind demo: 200x120 open, player walks east 1 cell/update, wind from the west.
    for (const float wind : {0.f, 0.5f, 1.f}) for (const int gust : {1, 3}) {
        if (wind == 0.f && gust == 3) continue;
        WindWave f(200, 120, 8, 8, gust, 1.f, 0.f, wind * 8.f);
        int px = 20; const int py = 60;
        size_t mx = 0;
        for (int t = 0; t < 120; ++t) { f.deposit(px, py, 60 * 16); mx = std::max(mx, f.update()); if (px < 190) ++px; }
        int ahead = 0; while (px + ahead + 1 < 200 && f.read(px + ahead + 1, py) > 0) ++ahead;
        int behind = 0; while (px - behind - 1 >= 0 && f.read(px - behind - 1, py) > 0) ++behind;
        std::printf("wind %.1f gust %d: scent reaches %3d cells ahead of the walker, %3d behind; max active cells/update %zu\n", wind, gust, ahead, behind, mx);
    }
    // indoors: same wind, openness 0 inside a box around the walker's path
    { WindWave f(200, 120, 8, 8, 3, 1.f, 0.f, 8.f); for (int y = 50; y < 70; ++y) for (int x = 0; x < 200; ++x) f.openness[y * 200 + x] = 0.f;
      int px = 20; for (int t = 0; t < 120; ++t) { f.deposit(px, 60, 60 * 16); f.update(); if (px < 190) ++px; }
      int ahead = 0; while (f.read(px + ahead + 1, 60) > 0) ++ahead; std::printf("same wind, openness 0 (indoors): %d cells ahead\n", ahead); }

    // ---- 512x512 scaling: dense sweep vs wave; 50,000 Dead
    { const int S = 512; Grid<bool> b(S, S, false); for (int i = 0; i < S; ++i) { b.at(i, 0) = b.at(0, i) = b.at(S - 1, i) = b.at(i, S - 1) = true; }
      Rng rng(5); for (int y = 1; y < S - 1; ++y) for (int x = 1; x < S - 1; ++x) b.at(x, y) = rng.chance(0.2f);
      ScentField f(S, S); f.deposit({256, 256}, 500);
      auto t0 = Clock::now(); for (int i = 0; i < 20; ++i) f.step(&b); std::printf("512x512 20%% walls: dense sweep %.2f ms/update\n", ms_since(t0) / 20);
      for (const int reach : {60, 120, 250}) { WindWave w(S, S, 8, 8, 1, 0, 0, 0); for (int i = 0; i < S * S; ++i) w.open[i] = !b.data()[i];
        for (int t = 0; t < 400; ++t) { w.deposit(256, 256, reach * 16); w.update(); }
        t0 = Clock::now(); size_t mx = 0; for (int t = 0; t < 100; ++t) { w.deposit(256, 256, reach * 16); mx = std::max(mx, w.update()); }
        std::printf("512x512 wave, reach %3d cells: %.3f ms/update, %zu cells touched\n", reach, ms_since(t0) / 100, mx); }
      std::vector<Dead> horde; while (horde.size() < 50000) { Vec2i p{rng.range(1, S - 2), rng.range(1, S - 2)}; if (!b.at(p)) horde.push_back({.pos = p}); }
      for (int i = 0; i < 200; ++i) { f.deposit({256, 256}, 500); f.step(&b); }
      t0 = Clock::now(); for (int i = 0; i < 20; ++i) { for (Dead& d : horde) d.cooldown_s = 0; step_horde(horde, f, b, 6); }
      std::printf("50,000 Dead strongest_neighbour on 512x512: %.2f ms/update\n", ms_since(t0) / 20); }
    return 0;
}
