# ADR-0001: Target platforms and GPU backend

- Status: Accepted
- Date: 2026-09-27

## Context

The game must run on Linux and Windows. The primary development machine is an Apple M1 running
Asahi Linux. Its GPU is reachable only through Vulkan (the Mesa Honeykrisp driver).

SDL_GPU (SDL3) selects Vulkan, D3D12 or Metal per platform. Shader bytecode formats differ per
backend:

| Backend | Bytecode |
|---|---|
| Vulkan | SPIR-V |
| D3D12 | DXIL |
| Metal | MSL / metallib |

macOS supports **only** the Metal backend. Supporting it would need a second shader toolchain
(SDL_shadercross or SPIRV-Cross), a Metal-specific test matrix, and Apple code signing and
notarization.

## Decision

- Supported platforms: **Linux and Windows**, x86_64 and aarch64 where toolchains allow.
- GPU backend: **SDL_GPU on Vulkan** on both platforms.
- Shaders: **GLSL → SPIR-V**, compiled at build time.
- macOS is **out of scope**. The shader pipeline stays SPIR-V-based, so adding MSL output later
  through SPIRV-Cross remains possible.

## Consequences

- One shader language and one bytecode format. No runtime shader compiler.
- Windows machines without a working Vulkan driver are unsupported (rare on desktop GPUs since
  ~2016). A D3D12 fallback through SDL_shadercross can be added later without API changes.
- Hosted CI runners have no GPU. GPU tests use Mesa **lavapipe** (software Vulkan) on Linux CI,
  and CPU reference kernels everywhere else.
- Verified on 2026-09-27: SDL3 3.4.16 runs compute on Vulkan/Honeykrisp
  (`docs/research/spikes/gpu-compute-probe/`).
