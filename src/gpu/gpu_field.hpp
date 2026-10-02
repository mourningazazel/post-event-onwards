#pragma once

// PEO-081: the calm scent update on the GPU (ADR-0014): SDL_GPU compute on Vulkan,
// shaders compiled to SPIR-V at build time and embedded. It matches ScentWave's CPU
// pull bit for bit (integer max and subtraction only), and hands back what the CPU
// keeps beside the values: per-tile changed flags and every cell's direction byte.

#include "peo/core/field_backend.hpp"

#include <SDL3/SDL_gpu.h>

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace peo::gpu {

/// A field backend on an SDL_GPU device (not owned: the frontend's, or a test's). One
/// dense dispatch per round over every cell (no active list yet: measure first), then
/// one for the direction bytes, then one readback, all in one submit. Buffers are sized
/// once per stage size and the masks uploaded once per stage; nothing is allocated per
/// update once warm. Fields under `min_cells` decline, as does every run when the
/// pipelines could not be made: the wave then runs on the CPU, with the same result.
class GpuFieldBackend final : public core::FieldBackend {
public:
    GpuFieldBackend(SDL_GPUDevice* device, std::size_t min_cells);
    ~GpuFieldBackend() override;
    GpuFieldBackend(const GpuFieldBackend&) = delete;
    GpuFieldBackend& operator=(const GpuFieldBackend&) = delete;

    /// Whether the compute pipelines exist, so runs can happen at all.
    [[nodiscard]] bool ready() const noexcept { return pull_ != nullptr && flow_ != nullptr; }
    [[nodiscard]] bool run(const core::FieldRounds& rounds) override;
    /// Updates this backend has run (not declined): tests check the GPU did the work.
    [[nodiscard]] std::size_t runs() const noexcept { return runs_; }

private:
    /// Size the buffers for this field (once per stage size); false if the device
    /// could not make them.
    [[nodiscard]] bool fit(const core::FieldRounds& rounds);
    void release_buffers() noexcept;

    SDL_GPUDevice* device_;
    std::size_t min_cells_;
    SDL_GPUComputePipeline* pull_ = nullptr;
    SDL_GPUComputePipeline* flow_ = nullptr;
    SDL_GPUBuffer* masks_ = nullptr;
    SDL_GPUBuffer* a_ = nullptr;
    SDL_GPUBuffer* b_ = nullptr;
    SDL_GPUBuffer* flags_ = nullptr;
    SDL_GPUBuffer* flow_words_ = nullptr;
    SDL_GPUTransferBuffer* up_ = nullptr;
    SDL_GPUTransferBuffer* down_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int tiles_ = 0;
    /// The stage whose masks are on the device; 0 (a wave with no token) re-sends them.
    std::uint64_t stage_ = 0;
    bool masks_sent_ = false;
    std::size_t runs_ = 0;
    /// The speculation worker and the main thread may both run updates.
    std::mutex mutex_;
};

/// A device for compute on Vulkan with SPIR-V (ADR-0001), or null when there is none.
/// SDL's video subsystem must be initialised.
[[nodiscard]] SDL_GPUDevice* create_compute_device();

} // namespace peo::gpu
