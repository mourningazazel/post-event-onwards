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

    TEST_CASE("worlds on the GPU field equal worlds on the CPU") {
        // World goldens with the GPU backend: one World steps on the CPU, one speculates
        // and commits with every calm update on the GPU; equal after every action.
        if (device() == nullptr) {
            MESSAGE("no Vulkan device: the GPU tests are skipped");
            return;
        }
        constexpr int kActions = 200;
        constexpr Seconds kDurations[] = {1, 3, 6, 12};
        peo::gpu::GpuFieldBackend gpu(device(), 0);
        const WorldParams params{.initial_dead = 300, .stage_width = 160, .stage_height = 90};
        World cpu(17, params);
        World on_gpu(17, params);
        on_gpu.set_field_backend(&gpu);
        Speculation spec;
        Rng pick(17);
        for (int i = 0; i < kActions; ++i) {
            const Seconds secs = kDurations[pick.range(0, 3)];
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
        MESSAGE("GPU ran " << gpu.runs() << " of " << on_gpu.updates() << " updates");
    }
}
