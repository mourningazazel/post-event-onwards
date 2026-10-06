// PEO-009: the swarm scenarios for the calm Dead's weighted draw (D-038 B), scent-mobs.md
// "Reproducible swarm tests". Each reports its metric with MESSAGE, so a knob change
// shows as a number.

#include "peo/core/dead.hpp"
#include "peo/core/desire.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/scent_wave.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace peo::core;

namespace {

constexpr std::uint64_t kSwarmSalt = 0x5A;

/// An open stage with a border wall.
Grid<bool> open_stage(int w, int h) {
    Grid<bool> b(w, h, false);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            b.at(x, y) = x == 0 || y == 0 || x == w - 1 || y == h - 1;
        }
    }
    return b;
}

int chebyshev(Vec2i a, Vec2i b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

/// A horde on a stage with standing emitters. Each update: the emitters deposit, the
/// field updates, the desire is built from it and the occupancy, then every unit draws
/// once, in index order against the moves already reserved (D-031 within one second),
/// and every move lands.
struct Swarm {
    Grid<bool> blocked;
    ScentWave wave;
    DesireField desire;
    Grid<std::uint8_t> occupied;
    std::vector<Dead> horde;
    std::vector<std::pair<Vec2i, std::int32_t>> emitters; // where, and how strong
    Slot updates = 0;

    explicit Swarm(Grid<bool> b)
        : blocked(std::move(b)), wave(blocked.width(), blocked.height()),
          desire(blocked.width(), blocked.height()), occupied(blocked.width(), blocked.height(), 0) {}

    void place(Vec2i at) {
        occupied.at(at) = 1;
        horde.push_back({.pos = at});
    }
    void warm(int n) {
        for (int i = 0; i < n; ++i) {
            for (const auto& [at, s] : emitters) {
                wave.deposit(at, s);
            }
            wave.update(blocked);
        }
    }
    /// One update; `seen(unit, chosen)` is told every draw (the choice, before
    /// occupancy) so a test can tally it.
    template <typename Seen> void step(Seen seen) {
        warm(1);
        desire.build(wave, blocked, occupied, DeadDrawParams{}, nullptr);
        Grid<bool> reserved(blocked.width(), blocked.height(), false);
        std::vector<std::pair<std::size_t, Vec2i>> moves;
        for (std::size_t i = 0; i < horde.size(); ++i) {
            const std::uint64_t word = draw_word(kSwarmSalt, updates, i);
            seen(horde[i], draw_choice(desire.weights(horde[i].pos), word));
            if (const auto to = decide_move(horde[i], desire, occupied, reserved, word)) {
                reserved.at(*to) = true;
                moves.emplace_back(i, *to);
            }
        }
        for (const auto& [i, to] : moves) {
            --occupied.at(horde[i].pos);
            horde[i].pos = to;
            ++occupied.at(to);
        }
        ++updates;
    }
    void step() {
        step([](const Dead&, std::size_t) {});
    }
};

} // namespace

TEST_SUITE("scenario: swarm") {
    TEST_CASE("sure-footed gradient: the Dead climb surer the stronger the scent") {
        // D-038 B: one standing emitter on an open stage, 2,000 Dead spread over it after
        // the field has filled its reach. Of the draws that pick a neighbour, the share
        // that climbs (a neighbour with more scent) is at least 90% within 10 cells of
        // the emitter, under 60% beyond 45, and rises band by band (5 cells) between.
        // (Draws, not landed moves: near the emitter most uphill tiles are taken, and
        // D-031 keeps a unit whose drawn tile is taken where it is.)
        constexpr int kSide = 141;
        constexpr Vec2i kEmitter{kSide / 2, kSide / 2};
        constexpr int kDead = 2000;
        constexpr int kWarm = kWaveReachCells + 5;
        constexpr int kUpdates = 12;
        constexpr int kBand = 5;
        constexpr int kNear = 10;
        constexpr int kFar = 45;
        constexpr int kBands = kWaveReachCells / kBand + 1;
        Swarm s(open_stage(kSide, kSide));
        s.emitters = {{kEmitter, s.wave.params().strength}};
        s.warm(kWarm);
        Rng rng(91);
        while (s.horde.size() < static_cast<std::size_t>(kDead)) {
            const Vec2i c{rng.range(1, kSide - 2), rng.range(1, kSide - 2)};
            if (s.occupied.at(c) == 0 && c != kEmitter) {
                s.place(c);
            }
        }
        std::array<int, kBands> draws{};
        std::array<int, kBands> climbs{};
        for (int u = 0; u < kUpdates; ++u) {
            s.step([&](const Dead& d, std::size_t choice) {
                const int dist = chebyshev(d.pos, kEmitter);
                if (choice == kStayChoice || dist > kWaveReachCells) {
                    return;
                }
                const auto band = static_cast<std::size_t>(dist / kBand);
                ++draws[band];
                climbs[band] += s.wave.sample(d.pos + kNeighbours8[choice]) > s.wave.sample(d.pos) ? 1 : 0;
            });
        }
        const auto share = [&](int from, int to) { // bands covering [from, to) cells
            int c = 0;
            int n = 0;
            for (int b = from / kBand; b < to / kBand && b < kBands; ++b) {
                c += climbs[static_cast<std::size_t>(b)];
                n += draws[static_cast<std::size_t>(b)];
            }
            return n == 0 ? 0.0 : static_cast<double>(c) / n;
        };
        const double near = share(0, kNear);
        const double far = share(kFar, kWaveReachCells + kBand);
        MESSAGE("climbing share: within " << kNear << " " << near << ", beyond " << kFar << " " << far);
        // Between 10 and 45 cells each band climbs less than the one inside it. (Within
        // 10 the emitter's own cell and the crowd on it, from which no draw climbs,
        // count too, so that share is checked only against its floor.)
        double last = 1.0;
        for (int b = kNear / kBand; b < kFar / kBand; ++b) {
            const double here = share(b * kBand, (b + 1) * kBand);
            MESSAGE("  band " << b * kBand << "-" << (b + 1) * kBand << ": " << here << " of "
                              << draws[static_cast<std::size_t>(b)] << " draws");
            CHECK(here < last);
            last = here;
        }
        CHECK(near >= 0.9);
        CHECK(far < 0.6);
        CHECK(far < last);
    }

    TEST_CASE("swarm-accumulation: the Dead gather where the scent is stronger") {
        // An even spread of Dead; weaker emitters across the stage and one full-strength
        // one with its region. The region's share of the horde grows at least 1.5x.
        constexpr int kW = 120;
        constexpr int kH = 80;
        constexpr int kDead = 1500;
        constexpr int kPitch = 24;
        constexpr Vec2i kStrong{90, 40};
        constexpr int kRegion = 12; // Chebyshev radius of the region round it
        constexpr int kUpdates = 40;
        Swarm s(open_stage(kW, kH));
        const std::int32_t full = s.wave.params().strength;
        for (int y = kPitch / 2; y < kH; y += kPitch) {
            for (int x = kPitch / 2; x < kW; x += kPitch) {
                if (chebyshev({x, y}, kStrong) > kRegion) {
                    s.emitters.push_back({{x, y}, full / 2});
                }
            }
        }
        s.emitters.push_back({kStrong, full});
        s.warm(kWaveReachCells);
        Rng rng(92);
        while (s.horde.size() < static_cast<std::size_t>(kDead)) {
            const Vec2i c{rng.range(1, kW - 2), rng.range(1, kH - 2)};
            if (s.occupied.at(c) == 0) {
                s.place(c);
            }
        }
        const auto in_region = [&] {
            return static_cast<double>(
                       std::count_if(s.horde.begin(), s.horde.end(),
                                     [&](const Dead& d) { return chebyshev(d.pos, kStrong) <= kRegion; })) /
                   kDead;
        };
        const double before = in_region();
        for (int u = 0; u < kUpdates; ++u) {
            s.step();
        }
        const double after = in_region();
        MESSAGE("swarm-accumulation: region share " << before << " -> " << after << " (" << after / before
                                                    << "x)");
        CHECK(after >= 1.5 * before);
    }

    TEST_CASE("fan-out: a tight cluster on flat scent spreads, never two to a tile") {
        // No scent at all: a packed 15x15 block of the Dead wanders. Its bounding area
        // grows at least 3x, and no tile ever holds two.
        constexpr int kSide = 101;
        constexpr int kBlock = 15;
        constexpr int kUpdates = 40;
        Swarm s(open_stage(kSide, kSide));
        const int x0 = kSide / 2 - kBlock / 2;
        for (int y = x0; y < x0 + kBlock; ++y) {
            for (int x = x0; x < x0 + kBlock; ++x) {
                s.place({x, y});
            }
        }
        const auto area = [&] {
            int minx = kSide, maxx = 0, miny = kSide, maxy = 0;
            for (const Dead& d : s.horde) {
                minx = std::min(minx, d.pos.x);
                maxx = std::max(maxx, d.pos.x);
                miny = std::min(miny, d.pos.y);
                maxy = std::max(maxy, d.pos.y);
            }
            return (maxx - minx + 1) * (maxy - miny + 1);
        };
        const int before = area();
        int doubled = 0;
        for (int u = 0; u < kUpdates; ++u) {
            s.step();
            doubled += static_cast<int>(
                std::count_if(s.occupied.begin(), s.occupied.end(), [](std::uint8_t n) { return n > 1; }));
        }
        const int after = area();
        MESSAGE("fan-out: bounding area " << before << " -> " << after << " ("
                                          << static_cast<double>(after) / before << "x)");
        CHECK(after >= 3 * before);
        CHECK(doubled == 0);
    }
}
