#include "gpu_field.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <utility>

#include "wave_flow_spv.hpp"
#include "wave_fused_spv.hpp"
#include "wave_pull_spv.hpp"

namespace peo::gpu {

namespace {
static_assert(std::endian::native == std::endian::little, "direction words unpack as little-endian bytes");
/// Invocations per workgroup: local_size_x in both shaders.
constexpr Uint32 kGroup = 64;
/// Cells packed into one word of direction bytes.
constexpr std::size_t kCellsPerWord = 4;
constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
/// Mask bits per cell: 1 open, the corner mask above it (wave_pull.comp), and from bit 5
/// which of the eight neighbours a step may reach (wave_flow.comp).
constexpr std::uint32_t kOpenBit = 1;
constexpr int kDiagShift = 1;
constexpr int kReachShift = 5;
constexpr int kOpennessShift = 13; // and from bit 13 its openness to the wind (wave_common.glsl)
/// The fused shader's geometry (wave_fused.comp): a block of kFusedBlockW x kFusedBlockH
/// cells inside one pull tile, a halo of at most kFusedMaxHalo rounds, kFusedThreads lanes.
constexpr int kFusedBlockW = PEO_GPU_FUSED_BLOCK_W;
constexpr int kFusedBlockH = 8;
constexpr int kFusedMaxHalo = 6;
constexpr Uint32 kFusedThreads = 256;
/// A full round carries every neighbour slot.
constexpr std::uint32_t kAllSlots = 0xFF;
constexpr std::array<std::array<int, 2>, 8> kNeighbours{
    {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}}; // kNeighbours8

/// wave_pull.comp's uniform block (std140: eight 32-bit fields, two ivec4, four more).
struct PullParams {
    std::int32_t width;
    std::int32_t height;
    std::int32_t tiles_x;
    std::int32_t tile_width;
    std::int32_t tile_height;
    std::int32_t cost;
    std::int32_t line;
    std::uint32_t round_plus_one;
    std::array<std::int32_t, 8> steps; // step_lo, step_hi
    std::uint32_t allowed;
    std::uint32_t need_open;
    std::uint32_t windy;
    std::uint32_t pad;
};
/// wave_fused.comp's (std140, laid out as PullParams).
struct FusedParams {
    std::int32_t width;
    std::int32_t height;
    std::int32_t tiles_x;
    std::int32_t tile_width;
    std::int32_t tile_height;
    std::int32_t cost;
    std::int32_t line;
    std::int32_t halo;
    std::array<std::int32_t, 8> steps;
    std::int32_t full_rounds;
    std::uint32_t gust_from;
    std::uint32_t windy;
    std::uint32_t pad;
};
static_assert(sizeof(PullParams) == 80 && sizeof(FusedParams) == 80, "std140 blocks");
/// wave_flow.comp's, padded to a std140 block's 16 bytes.
struct FlowParams {
    std::int32_t width;
    std::int32_t height;
    std::int32_t pad0;
    std::int32_t pad1;
};

SDL_GPUComputePipeline* make_pipeline(SDL_GPUDevice* device, const unsigned char* code, std::size_t size,
                                      Uint32 readonly, Uint32 readwrite, Uint32 threads = kGroup) {
    SDL_GPUComputePipelineCreateInfo info{};
    info.code = code;
    info.code_size = size;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.num_readonly_storage_buffers = readonly;
    info.num_readwrite_storage_buffers = readwrite;
    info.num_uniform_buffers = 1;
    info.threadcount_x = threads;
    info.threadcount_y = 1;
    info.threadcount_z = 1;
    return SDL_CreateGPUComputePipeline(device, &info);
}

/// A read-write binding: value-initialised, so SDL's padding fields are zero.
SDL_GPUStorageBufferReadWriteBinding writes_to(SDL_GPUBuffer* buffer) {
    SDL_GPUStorageBufferReadWriteBinding binding{};
    binding.buffer = buffer;
    binding.cycle = false;
    return binding;
}

Uint32 groups(std::size_t items) {
    return static_cast<Uint32>((items + kGroup - 1) / kGroup);
}
} // namespace

SDL_GPUDevice* create_compute_device() {
    return SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
}

GpuFieldBackend::GpuFieldBackend(SDL_GPUDevice* device, std::size_t min_cells, std::size_t windy_min_cells)
    : device_(device), min_cells_(min_cells), windy_min_cells_(windy_min_cells) {
    if (device_ == nullptr) {
        return;
    }
    pull_ = make_pipeline(device_, k_wave_pull_spv, sizeof k_wave_pull_spv, 2, 2);
    flow_ = make_pipeline(device_, k_wave_flow_spv, sizeof k_wave_flow_spv, 2, 1);
    fused_ = make_pipeline(device_, k_wave_fused_spv, sizeof k_wave_fused_spv, 2, 2, kFusedThreads);
}

GpuFieldBackend::~GpuFieldBackend() {
    if (device_ == nullptr) {
        return;
    }
    release_buffers();
    if (pull_ != nullptr) {
        SDL_ReleaseGPUComputePipeline(device_, pull_);
    }
    if (flow_ != nullptr) {
        SDL_ReleaseGPUComputePipeline(device_, flow_);
    }
    if (fused_ != nullptr) {
        SDL_ReleaseGPUComputePipeline(device_, fused_);
    }
}

void GpuFieldBackend::release_buffers() noexcept {
    for (SDL_GPUBuffer** b : {&masks_, &a_, &b_, &flags_, &flow_words_}) {
        if (*b != nullptr) {
            SDL_ReleaseGPUBuffer(device_, *b);
            *b = nullptr;
        }
    }
    for (SDL_GPUTransferBuffer** t : {&up_, &down_}) {
        if (*t != nullptr) {
            SDL_ReleaseGPUTransferBuffer(device_, *t);
            *t = nullptr;
        }
    }
    width_ = 0;
    height_ = 0;
    tiles_ = 0;
    masks_sent_ = false;
}

bool GpuFieldBackend::fit(const core::FieldRounds& r) {
    const int tiles = r.tiles_x * r.tiles_y;
    if (a_ != nullptr && r.width == width_ && r.height == height_ && tiles == tiles_) {
        return true;
    }
    release_buffers();
    const std::size_t cells = static_cast<std::size_t>(r.width) * static_cast<std::size_t>(r.height);
    const std::size_t words = (cells + kCellsPerWord - 1) / kCellsPerWord;
    const auto cell_bytes = static_cast<Uint32>(cells * kWordBytes);
    const auto tile_bytes = static_cast<Uint32>(static_cast<std::size_t>(tiles) * kWordBytes);
    const auto word_bytes = static_cast<Uint32>(words * kWordBytes);
    const auto buffer = [&](SDL_GPUBufferUsageFlags usage, Uint32 size) {
        SDL_GPUBufferCreateInfo info{};
        info.usage = usage;
        info.size = size;
        return SDL_CreateGPUBuffer(device_, &info);
    };
    const SDL_GPUBufferUsageFlags rw =
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    masks_ = buffer(SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, cell_bytes);
    a_ = buffer(rw, cell_bytes);
    b_ = buffer(rw, cell_bytes);
    flags_ = buffer(rw, tile_bytes);
    flow_words_ = buffer(rw, word_bytes);
    SDL_GPUTransferBufferCreateInfo up{};
    up.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    up.size = 2 * cell_bytes + tile_bytes; // values, zero flags, masks
    up_ = SDL_CreateGPUTransferBuffer(device_, &up);
    SDL_GPUTransferBufferCreateInfo down{};
    down.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    down.size = cell_bytes + tile_bytes + word_bytes; // values, flags, direction words
    down_ = SDL_CreateGPUTransferBuffer(device_, &down);
    if (masks_ == nullptr || a_ == nullptr || b_ == nullptr || flags_ == nullptr || flow_words_ == nullptr ||
        up_ == nullptr || down_ == nullptr) {
        release_buffers();
        return false;
    }
    width_ = r.width;
    height_ = r.height;
    tiles_ = tiles;
    return true;
}

bool GpuFieldBackend::run(const core::FieldRounds& r) {
    const std::lock_guard lock(mutex_);
    const std::size_t cells = static_cast<std::size_t>(r.width) * static_cast<std::size_t>(r.height);
    // A pull workgroup is one row's kGroup cells inside one tile (wave_pull.comp).
    if (!ready() || cells < (r.windy ? windy_min_cells_ : min_cells_) || cells == 0 || r.rounds < 1 ||
        r.tile_width % static_cast<int>(kGroup) != 0 || !fit(r)) {
        return false;
    }
    const auto tiles = static_cast<std::size_t>(tiles_);
    const std::size_t words = (cells + kCellsPerWord - 1) / kCellsPerWord;
    const std::size_t cell_bytes = cells * kWordBytes;
    const std::size_t tile_bytes = tiles * kWordBytes;
    const std::size_t word_bytes = words * kWordBytes;
    const bool with_openness = r.windy && r.openness != nullptr;
    const bool send_masks =
        !masks_sent_ || r.stage == 0 || r.stage != stage_ || (with_openness && !masks_openness_);

    // Upload: the values, zeroed flags, and the masks when the stage is new.
    auto* up = static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device_, up_, true));
    if (up == nullptr) {
        return false;
    }
    std::memcpy(up, r.values, cell_bytes);
    std::memset(up + cell_bytes, 0, tile_bytes);
    if (send_masks) {
        auto* masks = reinterpret_cast<std::uint32_t*>(up + cell_bytes + tile_bytes);
        for (int y = 0; y < r.height; ++y) {
            for (int x = 0; x < r.width; ++x) {
                const auto i = static_cast<std::size_t>(y) * static_cast<std::size_t>(r.width) +
                               static_cast<std::size_t>(x);
                // The direction bytes' rule (ScentWave's flow_row): on the map, open, and
                // a diagonal only through this cell's corner mask.
                std::uint32_t reach = 0;
                for (std::size_t d = 0; d < kNeighbours.size(); ++d) {
                    const int nx = x + kNeighbours[d][0];
                    const int ny = y + kNeighbours[d][1];
                    const bool corner = d % 2 == 0 || ((r.diag[i] >> (d / 2)) & 1U) != 0;
                    if (nx >= 0 && ny >= 0 && nx < r.width && ny < r.height && corner &&
                        r.open[static_cast<std::size_t>(ny) * static_cast<std::size_t>(r.width) +
                               static_cast<std::size_t>(nx)] != 0) {
                        reach |= 1U << d;
                    }
                }
                const std::uint32_t openness = with_openness ? r.openness[i] : 0U;
                masks[i] = (r.open[i] != 0 ? kOpenBit : 0U) | (std::uint32_t{r.diag[i]} << kDiagShift) |
                           (reach << kReachShift) | (openness << kOpennessShift);
            }
        }
    }
    SDL_UnmapGPUTransferBuffer(device_, up_);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
    if (cmd == nullptr) {
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const auto upload = [&](std::size_t offset, SDL_GPUBuffer* to, std::size_t size) {
        SDL_GPUTransferBufferLocation from{.transfer_buffer = up_, .offset = static_cast<Uint32>(offset)};
        SDL_GPUBufferRegion region{.buffer = to, .offset = 0, .size = static_cast<Uint32>(size)};
        SDL_UploadToGPUBuffer(copy, &from, &region, false);
    };
    upload(0, a_, cell_bytes);
    upload(cell_bytes, flags_, tile_bytes);
    if (send_masks) {
        upload(cell_bytes + tile_bytes, masks_, cell_bytes);
    }
    SDL_EndGPUCopyPass(copy);

    // The rounds: the full ones, then under a wind the gust ones. Each reads one buffer
    // and writes the other (ADR-0014's rule); a tile's flag ends as the last round it
    // changed in.
    const int gusts = r.windy ? r.gust_rounds : 0;
    const int total = r.rounds + gusts;
    SDL_GPUBuffer* in = a_;
    SDL_GPUBuffer* out = b_;
    const bool fused = use_fused_ && fused_ != nullptr && total <= kFusedMaxHalo &&
                       r.tile_width % kFusedBlockW == 0 && r.tile_height == kFusedBlockH;
    if (fused) {
        FusedParams params{.width = r.width,
                           .height = r.height,
                           .tiles_x = r.tiles_x,
                           .tile_width = r.tile_width,
                           .tile_height = r.tile_height,
                           .cost = r.distance_cost,
                           .line = r.age_line,
                           .halo = total,
                           .steps = r.wind_step,
                           .full_rounds = r.rounds,
                           .gust_from = r.gust_from,
                           .windy = r.windy ? 1U : 0U,
                           .pad = 0};
        SDL_PushGPUComputeUniformData(cmd, 0, &params, sizeof params);
        const std::array<SDL_GPUStorageBufferReadWriteBinding, 2> writes{writes_to(out), writes_to(flags_)};
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cmd, nullptr, 0, writes.data(), 2);
        SDL_BindGPUComputePipeline(pass, fused_);
        const std::array<SDL_GPUBuffer*, 2> reads{masks_, in};
        SDL_BindGPUComputeStorageBuffers(pass, 0, reads.data(), 2);
        SDL_DispatchGPUCompute(pass, static_cast<Uint32>((r.width + kFusedBlockW - 1) / kFusedBlockW),
                               static_cast<Uint32>((r.height + kFusedBlockH - 1) / kFusedBlockH), 1);
        SDL_EndGPUComputePass(pass);
        std::swap(in, out);
    }
    for (int k = 0; !fused && k < total; ++k) {
        const bool gust = k >= r.rounds;
        PullParams params{.width = r.width,
                          .height = r.height,
                          .tiles_x = r.tiles_x,
                          .tile_width = r.tile_width,
                          .tile_height = r.tile_height,
                          .cost = r.distance_cost,
                          .line = r.age_line,
                          .round_plus_one = static_cast<std::uint32_t>(k + 1),
                          .steps = r.wind_step,
                          .allowed = gust ? r.gust_from : kAllSlots,
                          .need_open = gust ? 1U : 0U,
                          .windy = r.windy ? 1U : 0U,
                          .pad = 0};
        SDL_PushGPUComputeUniformData(cmd, 0, &params, sizeof params);
        const std::array<SDL_GPUStorageBufferReadWriteBinding, 2> writes{writes_to(out), writes_to(flags_)};
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cmd, nullptr, 0, writes.data(), 2);
        SDL_BindGPUComputePipeline(pass, pull_);
        const std::array<SDL_GPUBuffer*, 2> reads{masks_, in};
        SDL_BindGPUComputeStorageBuffers(pass, 0, reads.data(), 2);
        SDL_DispatchGPUCompute(pass, groups(static_cast<std::size_t>(r.width)), static_cast<Uint32>(r.height),
                               1);
        SDL_EndGPUComputePass(pass);
        std::swap(in, out);
    }
    // The direction bytes of the final values.
    const FlowParams flow{.width = r.width, .height = r.height, .pad0 = 0, .pad1 = 0};
    SDL_PushGPUComputeUniformData(cmd, 0, &flow, sizeof flow);
    const SDL_GPUStorageBufferReadWriteBinding flow_write = writes_to(flow_words_);
    SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cmd, nullptr, 0, &flow_write, 1);
    SDL_BindGPUComputePipeline(pass, flow_);
    const std::array<SDL_GPUBuffer*, 2> reads{masks_, in};
    SDL_BindGPUComputeStorageBuffers(pass, 0, reads.data(), 2);
    SDL_DispatchGPUCompute(pass, groups(words), 1, 1);
    SDL_EndGPUComputePass(pass);

    // Read back the values, the flags and the bytes in one copy pass.
    copy = SDL_BeginGPUCopyPass(cmd);
    const auto download = [&](SDL_GPUBuffer* from, std::size_t size, std::size_t offset) {
        SDL_GPUBufferRegion region{.buffer = from, .offset = 0, .size = static_cast<Uint32>(size)};
        SDL_GPUTransferBufferLocation to{.transfer_buffer = down_, .offset = static_cast<Uint32>(offset)};
        SDL_DownloadFromGPUBuffer(copy, &region, &to);
    };
    download(in, cell_bytes, 0);
    download(flags_, tile_bytes, cell_bytes);
    download(flow_words_, word_bytes, cell_bytes + tile_bytes);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        return false;
    }
    const bool done = SDL_WaitForGPUFences(device_, true, &fence, 1);
    SDL_ReleaseGPUFence(device_, fence);
    if (!done) {
        return false;
    }
    stage_ = r.stage;
    if (send_masks) {
        masks_sent_ = true;
        masks_openness_ = with_openness;
    }

    const auto* down = static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device_, down_, false));
    if (down == nullptr) {
        return false;
    }
    std::memcpy(r.values, down, cell_bytes);
    const auto* flags = reinterpret_cast<const std::uint32_t*>(down + cell_bytes);
    // What the next update pulls: changed in the last full round or any gust round.
    const auto last = static_cast<std::uint32_t>(r.rounds);
    for (std::size_t t = 0; t < tiles; ++t) {
        r.last_changed[t] = flags[t] >= last ? 1 : 0;
        r.any_changed[t] = flags[t] != 0 ? 1 : 0;
    }
    std::memcpy(r.flow, down + cell_bytes + tile_bytes, cells); // bytes, little-endian words
    SDL_UnmapGPUTransferBuffer(device_, down_);
    ++runs_;
    fused_runs_ += fused ? 1 : 0;
    return true;
}

} // namespace peo::gpu
