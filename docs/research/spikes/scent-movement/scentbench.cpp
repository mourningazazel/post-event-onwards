// Throwaway benchmark: blind scent-following mobs, sequential occupancy-aware moves.
// Compares (A) all mobs in shuffled order, (B) frontier-only in shuffled order with wake-ups,
// (C) frontier ordered by scent strength (front of horde first) with wake-ups.
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <algorithm>
#include <deque>

static constexpr int W = 1024, H = 1024;
struct Rng { uint64_t s; uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return uint32_t(s >> 32); } };

static const int DX[8] = {1,-1,0,0,1,1,-1,-1};
static const int DY[8] = {0,0,1,-1,1,-1,1,-1};

struct World {
    std::vector<uint8_t> wall;
    std::vector<uint16_t> scent;
    std::vector<uint32_t> occ;  // mob index + 1, 0 = empty
    std::vector<int> mx, my;
    std::vector<uint32_t> stamp; // turn stamp: moved/queued this turn
    uint16_t weightLut[512];     // scent delta bucket -> weight (per-mob-type table in the real thing)
};

static inline int idx(int x, int y) { return y * W + x; }

void buildScent(World& w, int px, int py) {
    // BFS distance from the "player" -> scent = 65535 - 24*dist (stand-in for a diffused scent field)
    std::vector<int> dist(W * H, -1);
    std::deque<int> q; q.push_back(idx(px, py)); dist[idx(px, py)] = 0;
    while (!q.empty()) {
        int c = q.front(); q.pop_front(); int x = c % W, y = c / W;
        for (int d = 0; d < 4; ++d) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int n = idx(nx, ny);
            if (w.wall[n] || dist[n] >= 0) continue;
            dist[n] = dist[c] + 1; q.push_back(n);
        }
    }
    for (int i = 0; i < W * H; ++i) w.scent[i] = dist[i] < 0 ? 0 : uint16_t(std::max(0, 65535 - dist[i] * 24));
}

// Try to move mob m. Returns true if it moved. Weighted random pick over free neighbours + "stay".
static inline bool stepMob(World& w, int m, Rng& rng, int* vacatedOut) {
    int x = w.mx[m], y = w.my[m];
    int here = w.scent[idx(x, y)];
    uint32_t cum[9]; int cand[9]; int n = 0; uint32_t total = 0;
    total += 4; cum[n] = total; cand[n++] = -1;  // small weight for staying put
    for (int d = 0; d < 8; ++d) {
        int nx = x + DX[d], ny = y + DY[d];
        if ((unsigned)nx >= W || (unsigned)ny >= H) continue;
        int c = idx(nx, ny);
        if (w.wall[c] || w.occ[c]) continue;
        int delta = (int(w.scent[c]) - here) >> 7;  // -512..512 -> bucket
        delta = std::clamp(delta + 256, 0, 511);
        total += w.weightLut[delta]; cum[n] = total; cand[n++] = c;
    }
    if (n == 1) return false;
    uint32_t r = uint32_t((uint64_t(rng.next()) * total) >> 32);
    int k = 0; while (cum[k] <= r) ++k;
    if (cand[k] < 0) return false;
    int from = idx(x, y);
    w.occ[from] = 0; w.occ[cand[k]] = m + 1;
    w.mx[m] = cand[k] % W; w.my[m] = cand[k] / W;
    *vacatedOut = from;
    return true;
}

static inline bool hasFree(const World& w, int m) {
    int x = w.mx[m], y = w.my[m];
    for (int d = 0; d < 8; ++d) {
        int nx = x + DX[d], ny = y + DY[d];
        if ((unsigned)nx >= W || (unsigned)ny >= H) continue;
        int c = idx(nx, ny);
        if (!w.wall[c] && !w.occ[c]) return true;
    }
    return false;
}

struct Stats { double ms; long touched, moved; };

Stats turnA(World& w, Rng& rng, std::vector<int>& order) {
    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = order.size(); i > 1; --i) std::swap(order[i - 1], order[rng.next() % i]);
    long moved = 0; int vac;
    for (int m : order) moved += stepMob(w, m, rng, &vac);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return {ms, (long)order.size(), moved};
}

// Frontier with wake-ups. ordered=true processes by scent (high first) via 256 buckets.
Stats turnBC(World& w, Rng& rng, uint32_t turn, bool ordered, std::vector<std::vector<int>>& buckets) {
    auto t0 = std::chrono::steady_clock::now();
    int N = (int)w.mx.size(); long touched = 0, moved = 0;
    for (auto& b : buckets) b.clear();
    for (int m = 0; m < N; ++m)
        if (hasFree(w, m)) { w.stamp[m] = turn; buckets[ordered ? (w.scent[idx(w.mx[m], w.my[m])] >> 8) : 0].push_back(m); }
    if (!ordered) { auto& b = buckets[0]; for (size_t i = b.size(); i > 1; --i) std::swap(b[i - 1], b[rng.next() % i]); }
    std::vector<int> woken; woken.reserve(1024);
    auto runOne = [&](int m) {
        ++touched; int vac;
        if (!stepMob(w, m, rng, &vac)) return;
        ++moved;
        int vx = vac % W, vy = vac / W;  // wake unmoved neighbours of the vacated cell
        for (int d = 0; d < 8; ++d) {
            int nx = vx + DX[d], ny = vy + DY[d];
            if ((unsigned)nx >= W || (unsigned)ny >= H) continue;
            uint32_t o = w.occ[idx(nx, ny)];
            if (o && w.stamp[o - 1] != turn) { w.stamp[o - 1] = turn; woken.push_back(int(o - 1)); }
        }
    };
    for (int bi = 255; bi >= 0; --bi) {
        for (size_t i = 0; i < buckets[bi].size(); ++i) {
            runOne(buckets[bi][i]);
            while (!woken.empty()) { int m = woken.back(); woken.pop_back(); runOne(m); }
        }
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return {ms, touched, moved};
}


// (D) Persistent frontier maintained incrementally across turns; scent-ordered; wake-ups.
struct Frontier { std::vector<int> list; std::vector<int> pos; };
static inline void recheck(World& w, Frontier& f, int m) {
    bool fr = hasFree(w, m);
    if (fr && f.pos[m] < 0) { f.pos[m] = (int)f.list.size(); f.list.push_back(m); }
    else if (!fr && f.pos[m] >= 0) { int last = f.list.back(); f.list[f.pos[m]] = last; f.pos[last] = f.pos[m]; f.list.pop_back(); f.pos[m] = -1; }
}
static inline void recheckAround(World& w, Frontier& f, int cell) {
    int x = cell % W, y = cell / W;
    for (int d = 0; d < 8; ++d) {
        int nx = x + DX[d], ny = y + DY[d];
        if ((unsigned)nx >= W || (unsigned)ny >= H) continue;
        uint32_t o = w.occ[idx(nx, ny)]; if (o) recheck(w, f, int(o - 1));
    }
}
Stats turnD(World& w, Rng& rng, uint32_t turn, Frontier& f, std::vector<std::vector<int>>& buckets) {
    auto t0 = std::chrono::steady_clock::now();
    long touched = 0, moved = 0;
    for (auto& b : buckets) b.clear();
    for (int m : f.list) { w.stamp[m] = turn; buckets[w.scent[idx(w.mx[m], w.my[m])] >> 8].push_back(m); }
    std::vector<int> woken; woken.reserve(1024);
    auto runOne = [&](int m) {
        ++touched; int vac;
        if (!stepMob(w, m, rng, &vac)) return;
        ++moved;
        int now = idx(w.mx[m], w.my[m]);
        recheck(w, f, m); recheckAround(w, f, vac); recheckAround(w, f, now);
        int vx = vac % W, vy = vac / W;
        for (int d = 0; d < 8; ++d) {
            int nx = vx + DX[d], ny = vy + DY[d];
            if ((unsigned)nx >= W || (unsigned)ny >= H) continue;
            uint32_t o = w.occ[idx(nx, ny)];
            if (o && w.stamp[o - 1] != turn) { w.stamp[o - 1] = turn; woken.push_back(int(o - 1)); }
        }
    };
    for (int bi = 255; bi >= 0; --bi)
        for (size_t i = 0; i < buckets[bi].size(); ++i) {
            runOne(buckets[bi][i]);
            while (!woken.empty()) { int m = woken.back(); woken.pop_back(); runOne(m); }
        }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return {ms, touched, moved};
}


// (E) All mobs, but in spatial order (occupancy-grid scan), sweep direction alternates per turn.
Stats turnE(World& w, Rng& rng, uint32_t turn, std::vector<int>& order) {
    auto t0 = std::chrono::steady_clock::now();
    order.clear();
    if (turn & 1) { for (int c = 0; c < W * H; ++c) if (w.occ[c]) order.push_back(int(w.occ[c] - 1)); }
    else          { for (int c = W * H - 1; c >= 0; --c) if (w.occ[c]) order.push_back(int(w.occ[c] - 1)); }
    long moved = 0; int vac;
    for (int m : order) moved += stepMob(w, m, rng, &vac);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return {ms, (long)order.size(), moved};
}

void setup(World& w, int nMobs, uint64_t seed) {
    Rng rng{seed};
    w.wall.assign(W * H, 0); w.scent.assign(W * H, 0); w.occ.assign(W * H, 0);
    for (int i = 0; i < W * H; ++i) w.wall[i] = (rng.next() % 100) < 8;  // 8% scattered rubble
    int px = W / 2, py = H / 2; w.wall[idx(px, py)] = 0;
    buildScent(w, px, py);
    for (int i = 0; i < 512; ++i) { int d = i - 256; w.weightLut[i] = uint16_t(d <= 0 ? 1 : std::min(4000, 1 + d * d / 8)); }
    w.mx.clear(); w.my.clear();
    // Dense hordes: 16 blobs around the map edge region, filled until nMobs placed.
    int blobs = 16, perBlob = nMobs / blobs;
    for (int b = 0; b < blobs; ++b) {
        int cx = 100 + (rng.next() % (W - 200)), cy = 100 + (rng.next() % (H - 200));
        int placed = 0;
        for (int r = 0; placed < perBlob && r < 400; ++r)
            for (int dy = -r; dy <= r && placed < perBlob; ++dy)
                for (int dx = -r; dx <= r && placed < perBlob; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = cx + dx, y = cy + dy;
                    if ((unsigned)x >= W || (unsigned)y >= H) continue;
                    int c = idx(x, y);
                    if (w.wall[c] || w.occ[c]) continue;
                    w.occ[c] = uint32_t(w.mx.size() + 1); w.mx.push_back(x); w.my.push_back(y); ++placed;
                }
    }
    w.stamp.assign(w.mx.size(), 0);
}

int main() {
    for (int n : {10000, 100000, 300000}) {
        for (int mode = 0; mode < 5; ++mode) {
            World w; setup(w, n, 1234);
            Rng rng{99};
            std::vector<int> order(w.mx.size()); for (size_t i = 0; i < order.size(); ++i) order[i] = int(i);
            std::vector<std::vector<int>> buckets(256);
            Frontier fr; fr.pos.assign(w.mx.size(), -1); if (mode == 3) for (int m = 0; m < (int)w.mx.size(); ++m) recheck(w, fr, m);
            double tot = 0, worst = 0; long touched = 0, moved = 0; int turns = 50;
            for (int t = 1; t <= turns; ++t) {
                Stats s = mode == 4 ? turnE(w, rng, uint32_t(t), order) : mode == 0 ? turnA(w, rng, order) : mode == 3 ? turnD(w, rng, uint32_t(t), fr, buckets) : turnBC(w, rng, uint32_t(t), mode == 2, buckets);
                tot += s.ms; worst = std::max(worst, s.ms); touched += s.touched; moved += s.moved;
            }
            const char* name[] = {"A all/shuffled   ", "B frontier/shuffle", "C frontier/scent  ", "D persist/scent   ", "E all/spatial     "};
            printf("mobs=%6zu %s avg %.3f ms/turn  worst %.3f  stepped %5.1f%%  moved %5.1f%%\n",
                   w.mx.size(), name[mode], tot / turns, worst,
                   100.0 * touched / (turns * double(w.mx.size())), 100.0 * moved / (turns * double(w.mx.size())));
        }
    }
}
