// PEO-081: the GPU field backend against ScentWave's CPU pull, the reference: values,
// per-tile flags and direction bytes after every update, and whole worlds. Skipped with
// a MESSAGE when the machine has no Vulkan device (the lavapipe CI job always has one).

#include "peo/core/rng.hpp"
#include "peo/core/scent_wave.hpp"
#include "peo/core/world.hpp"

#include <SDL3/SDL.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "gpu_field.hpp"

using namespace peo::core;

namespace {

/// One compute device for the whole binary, or null.
SDL_GPUDevice* device() {
    static SDL_GPUDevice* d = [] {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            return static_cast<SDL_GPUDevice*>(nullptr);
        }
        return peo::gpu::create_compute_device();
    }();
    return d;
}

/// A stage with one wall in `wall_one_in` and a border wall.
Grid<bool> walled(int w, int h, int wall_one_in, Seed seed) {
    Rng rng(seed);
    Grid<bool> b(w, h, false);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            b.at(x, y) = x == 0 || y == 0 || x == w - 1 || y == h - 1 || rng.range(1, wall_one_in) == 1;
        }
    }
    return b;
}

} // namespace

TEST_SUITE("gpu") {
    TEST_CASE("the GPU field equals the CPU pull on every update") {
        // Random walled stages (sizes off the tile grid), emitters in most pull tiles, a
        // walker, 120 updates, speed 1 and 2; after each update the values, the tiles
        // changed in the last round, the tiles written for a partner and every direction
        // byte must match the CPU's.
        if (device() == nullptr) {
            MESSAGE("no Vulkan device: the GPU tests are skipped");
            return;
        }
        constexpr int kUpdates = 120;
        constexpr int kWallOneIn = 7;
        constexpr int kEmitterPitch = 23; // one in about every pull tile
        peo::gpu::GpuFieldBackend gpu(device(), 0);
        REQUIRE(gpu.ready());
        struct Case {
            int width;
            int height;
            int speed;
            Seed seed;
        };
        for (const Case c : {Case{300, 130, 1, 3}, Case{129, 67, 2, 4}, Case{511, 40, 1, 5}}) {
            CAPTURE(c.width);
            CAPTURE(c.speed);
            const Grid<bool> b = walled(c.width, c.height, kWallOneIn, c.seed);
            std::vector<Vec2i> emitters;
            for (int y = 2; y < c.height - 2; y += kEmitterPitch / 3) {
                for (int x = 2; x < c.width - 2; x += kEmitterPitch) {
                    if (!b.at(x, y)) {
                        emitters.push_back({x, y});
                    }
                }
            }
            const WaveParams p{.speed = c.speed};
            ScentWave cpu(c.width, c.height, p);
            ScentWave on_gpu(c.width, c.height, p);
            cpu.set_token(1);
            on_gpu.set_token(1);
            on_gpu.set_field_backend(&gpu);
            Rng walk(c.seed);
            Vec2i walker{c.width / 2, c.height / 2};
            for (int u = 0; u < kUpdates; ++u) {
                const Vec2i next = walker + kNeighbours8[static_cast<std::size_t>(walk.range(0, 7))];
                if (next.x > 0 && next.y > 0 && next.x < c.width - 1 && next.y < c.height - 1 &&
                    !b.at(next)) {
                    walker = next;
                }
                for (ScentWave* w : {&cpu, &on_gpu}) {
                    // Emitters fall silent in turn, so the field also ages out.
                    for (std::size_t e = static_cast<std::size_t>(u) % 7; e < emitters.size(); e += 2) {
                        w->deposit(emitters[e], p.strength);
                    }
                    w->deposit(walker, p.strength);
                    w->update(b);
                }
                CAPTURE(u);
                REQUIRE(on_gpu.values() == cpu.values());
                REQUIRE(on_gpu.changed_tiles() == cpu.changed_tiles());
                REQUIRE(on_gpu.written_tiles() == cpu.written_tiles());
                REQUIRE(on_gpu.flow_bytes() == cpu.flow_bytes());
            }
        }
        CHECK(gpu.runs() == 3 * static_cast<std::size_t>(kUpdates)); // every update ran there
    }

    TEST_CASE("under wind and gusts both GPU paths equal the CPU on every update") {
        // PEO-087: windy waves on walled stages with indoor, outdoor and part-open cells;
        // winds along an axis and on diagonals, up to full, gust 0 to 3, speed 1 and 2, and
        // the wind changing half-way. The fused path (one dispatch per update) and the
        // per-round path must each give the CPU's values, tile flags and bytes every update.
        if (device() == nullptr) {
            MESSAGE("no Vulkan device: the GPU tests are skipped");
            return;
        }
        constexpr int kUpdates = 80;
        constexpr int kWallOneIn = 8;
        peo::gpu::GpuFieldBackend fused(device(), 0);
        peo::gpu::GpuFieldBackend per_round(device(), 0);
        fused.set_fused(true);
        per_round.set_fused(false);
        REQUIRE(fused.ready());
        struct Case {
            int width;
            int height;
            int speed;
            int gust;
            Wind first;
            Wind second;
            Seed seed;
        };
        const Case cases[] = {
            {300,
             70,
             1,
             2,
             {.toward_degrees = 0, .intensity = kWindFull},
             {.toward_degrees = 180, .intensity = 4},
             21},
            {257,
             61,
             1,
             3,
             {.toward_degrees = 45, .intensity = kWindFull},
             {.toward_degrees = 300, .intensity = 6},
             22},
            {200,
             90,
             2,
             1,
             {.toward_degrees = 135, .intensity = 5},
             {.toward_degrees = 90, .intensity = kWindFull},
             23},
            {131,
             45,
             1,
             0,
             {.toward_degrees = 225, .intensity = kWindFull},
             {.toward_degrees = 10, .intensity = 2},
             24},
            {190,
             50,
             2,
             3,
             {.toward_degrees = 315, .intensity = 7},
             {.toward_degrees = 0, .intensity = 0},
             25},
        };
        for (const Case& c : cases) {
            CAPTURE(c.seed);
            const Grid<bool> b = walled(c.width, c.height, kWallOneIn, c.seed);
            Rng rng(c.seed);
            Grid<std::uint8_t> openness(c.width, c.height, kOpennessOutdoors);
            for (int y = 0; y < c.height; ++y) {
                for (int x = 0; x < c.width; ++x) {
                    const int pick = rng.range(0, 9);
                    if (x > c.width / 3 && x < c.width / 2) {
                        openness.at(x, y) = kOpennessIndoors; // a band indoors
                    } else if (pick == 0) {
                        openness.at(x, y) = static_cast<std::uint8_t>(rng.range(1, kOpennessOutdoors - 1));
                    }
                }
            }
            std::vector<Vec2i> emitters;
            for (int y = 2; y < c.height - 2; y += 6) {
                for (int x = 2; x < c.width - 2; x += 17) {
                    if (!b.at(x, y)) {
                        emitters.push_back({x, y});
                    }
                }
            }
            const WaveParams p{.speed = c.speed, .gust = c.gust};
            ScentWave cpu(c.width, c.height, p);
            ScentWave on_fused(c.width, c.height, p);
            ScentWave on_rounds(c.width, c.height, p);
            on_fused.set_field_backend(&fused);
            on_rounds.set_field_backend(&per_round);
            for (int u = 0; u < kUpdates; ++u) {
                for (ScentWave* w : {&cpu, &on_fused, &on_rounds}) {
                    w->set_token(static_cast<std::uint64_t>(c.seed));
                    w->set_wind(u < kUpdates / 2 ? c.first : c.second);
                    for (std::size_t e = static_cast<std::size_t>(u) % 5; e < emitters.size(); e += 3) {
                        w->deposit(emitters[e], p.strength);
                    }
                    w->update(b, &openness);
                }
                CAPTURE(u);
                REQUIRE(on_fused.values() == cpu.values());
                REQUIRE(on_rounds.values() == cpu.values());
                REQUIRE(on_fused.changed_tiles() == cpu.changed_tiles());
                REQUIRE(on_rounds.changed_tiles() == cpu.changed_tiles());
                REQUIRE(on_fused.written_tiles() == cpu.written_tiles());
                REQUIRE(on_fused.flow_bytes() == cpu.flow_bytes());
                REQUIRE(on_rounds.flow_bytes() == cpu.flow_bytes());
            }
        }
        const auto all = std::size(cases) * static_cast<std::size_t>(kUpdates);
        CHECK(fused.fused_runs() == all);
        CHECK(per_round.runs() == all);
        CHECK(per_round.fused_runs() == 0);
    }

    TEST_CASE("worlds on the GPU field equal worlds on the CPU") {
        // World goldens with the GPU backend: one World steps on the CPU, one speculates
        // and commits with its updates on the GPU; equal after every action. Calm, then
        // under a stage wind with gusts (PEO-087).
        if (device() == nullptr) {
            MESSAGE("no Vulkan device: the GPU tests are skipped");
            return;
        }
        constexpr int kActions = 200;
        constexpr Substeps kDurations[] = {2, 6, 12, 24};
        for (const bool windy : {false, true}) {
            CAPTURE(windy);
            peo::gpu::GpuFieldBackend gpu(device(), 0);
            WorldParams params{.initial_dead = 300, .stage_width = 160, .stage_height = 90};
            if (windy) {
                params.wind_max = kWindFull;
                params.scent.gust = 2;
            }
            World cpu(17, params);
            World on_gpu(17, params);
            on_gpu.set_field_backend(&gpu);
            Speculation spec;
            Rng pick(17);
            for (int i = 0; i < kActions; ++i) {
                const Substeps secs = kDurations[pick.range(0, 3)];
                const Action act = pick.range(1, 4) == 1 ? Action::wait(secs)
                                                         : Action::step(kNeighbours4[pick.range(0, 3)], secs);
                cpu.step(act);
                on_gpu.speculate(spec);
                on_gpu.commit(spec, act);
                if (!World::equivalent(cpu, on_gpu)) {
                    FAIL("diverged at action " << i);
                }
            }
            CHECK(gpu.runs() >= static_cast<std::size_t>(on_gpu.updates()) / 2); // speculated ones
            if (windy) {
                CHECK(on_gpu.wind().intensity > 0); // the windy run really ran under a wind
            }
            MESSAGE("GPU ran " << gpu.runs() << " of " << on_gpu.updates() << " updates, wind intensity "
                               << on_gpu.wind().intensity);
        }
    }
}
