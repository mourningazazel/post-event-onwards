#pragma once

// The scent wave's row kernels (PEO-078, PEO-079): one source, built plain in
// scent_wave.cpp and for AVX2 in wave_kernels_avx2.cpp on x86-64, and picked once per
// process by a CPU check (PEO-085). Both copies give the same bits: the kernels are
// integer-only.
//
// Everything here has internal linkage and calls no library code. MSVC builds the AVX2
// file with /arch:AVX2, which applies to every function that file instantiates; an
// inline library function (std::max, std::array's operator[]) built there could be the
// copy the linker keeps for the whole program, and fault on a CPU without AVX2.

#include "peo/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>

#if defined(__x86_64__) || defined(_M_X64)
#define PEO_WAVE_HAS_AVX2 1
#else
#define PEO_WAVE_HAS_AVX2 0
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define PEO_WAVE_INLINE static __forceinline
#else
// Inlined even unoptimised, so the AVX2 wrappers compile the body for AVX2.
#define PEO_WAVE_INLINE static inline __attribute__((always_inline))
#endif

namespace peo::core::wave_kernel {

/// Stored value of a cell nothing has reached: far below any age line, and far enough
/// above the type's minimum that subtracting a distance cost cannot overflow.
constexpr std::int32_t kUnreached = std::numeric_limits<std::int32_t>::min() / 2;

/// kNeighbours8 alternates orthogonal and diagonal (N, NE, E, SE, S, SW, W, NW): the
/// diagonal 2k+1 lies between orthogonals k and k+1 of kNeighbours4. Diagonal mask bit
/// k is kNeighbours8[2k+1]: NE, SE, SW, NW.
constexpr std::size_t kOrthogonals = 4;
constexpr std::uint8_t kNE = 1;
constexpr std::uint8_t kSE = 2;
constexpr std::uint8_t kSW = 4;
constexpr std::uint8_t kNW = 8;
/// A direction byte meaning "no neighbour is stronger" (indices 0-7 are kNeighbours8's).
constexpr std::uint8_t kNoFlow = 0xFF;
constexpr std::size_t kDirections = std::size(kNeighbours8);

PEO_WAVE_INLINE std::int32_t max32(std::int32_t x, std::int32_t y) noexcept {
    return x > y ? x : y;
}

/// The pull for one row segment [begin, end) of interior cells (never on the map's
/// border, so all eight neighbours exist): branch-free so it vectorises. Returns
/// whether any cell changed.
PEO_WAVE_INLINE bool pull_row(const std::int32_t* __restrict a, std::int32_t* __restrict b,
                              const std::uint8_t* __restrict open, const std::uint8_t* __restrict diag,
                              std::ptrdiff_t w, std::ptrdiff_t begin, std::ptrdiff_t end, std::int32_t cost,
                              std::int32_t line) noexcept {
    int changed = 0;
    for (std::ptrdiff_t i = begin; i < end; ++i) {
        // Every load is unconditional (interior cells have all eight neighbours) and the
        // masks only select, so the loop has no branches and vectorises.
        const std::int32_t m = diag[i];
        const std::int32_t ne = a[i - w + 1];
        const std::int32_t se = a[i + w + 1];
        const std::int32_t sw = a[i + w - 1];
        const std::int32_t nw = a[i - w - 1];
        std::int32_t best = max32(max32(a[i - w], a[i + 1]), max32(a[i + w], a[i - 1]));
        best = max32(best, (m & kNE) != 0 ? ne : kUnreached);
        best = max32(best, (m & kSE) != 0 ? se : kUnreached);
        best = max32(best, (m & kSW) != 0 ? sw : kUnreached);
        best = max32(best, (m & kNW) != 0 ? nw : kUnreached);
        const std::int32_t candidate = best - cost;
        const std::int32_t old = a[i];
        const bool take = (open[i] != 0) & (candidate > line) & (candidate > old);
        const std::int32_t v = take ? candidate : old;
        b[i] = v;
        changed |= static_cast<int>(take);
    }
    return changed != 0;
}

/// The direction bytes for one row segment [begin, end) of interior cells (PEO-079): the
/// first neighbour in kNeighbours8 order holding the strict maximum stored value above
/// the cell's own, among those a step may reach (open; a diagonal also needs its corner
/// mask bit). Stored values keep the order sample() gives everything above the age line,
/// so with flow_target()'s check of the target against the line this is
/// strongest_neighbour. Branch-free, as pull_row: loads are unconditional, the masks are
/// integers that only select (a bool & bool here keeps GCC from vectorising).
PEO_WAVE_INLINE void flow_row(const std::int32_t* __restrict a, std::uint8_t* __restrict flow,
                              const std::uint8_t* __restrict open, const std::uint8_t* __restrict diag,
                              std::ptrdiff_t w, std::ptrdiff_t begin, std::ptrdiff_t end) noexcept {
    std::ptrdiff_t offset[kDirections];
    for (std::size_t d = 0; d < kDirections; ++d) {
        offset[d] = kNeighbours8[d].y * w + kNeighbours8[d].x;
    }
    for (std::ptrdiff_t i = begin; i < end; ++i) {
        const std::int32_t m = diag[i];
        std::int32_t best = a[i];
        std::int32_t to = kNoFlow;
        for (std::size_t d = 0; d < kDirections; ++d) {
            const std::ptrdiff_t n = i + offset[d];
            const std::int32_t stored = a[n];
            const std::int32_t corner = d % 2 == 0 ? 1 : m >> (d / 2); // diagonal d is mask bit d/2
            const std::int32_t reach = static_cast<std::int32_t>(open[n]) & corner & 1;
            const std::int32_t v = reach != 0 ? stored : kUnreached;
            to = v > best ? static_cast<std::int32_t>(d) : to;
            best = max32(best, v);
        }
        flow[i] = static_cast<std::uint8_t>(to);
    }
}

#if PEO_WAVE_HAS_AVX2
/// The same kernels built for AVX2 (wave_kernels_avx2.cpp). Call only when the CPU has it.
bool pull_row_avx2(const std::int32_t* a, std::int32_t* b, const std::uint8_t* open, const std::uint8_t* diag,
                   std::ptrdiff_t w, std::ptrdiff_t begin, std::ptrdiff_t end, std::int32_t cost,
                   std::int32_t line) noexcept;
void flow_row_avx2(const std::int32_t* a, std::uint8_t* flow, const std::uint8_t* open,
                   const std::uint8_t* diag, std::ptrdiff_t w, std::ptrdiff_t begin,
                   std::ptrdiff_t end) noexcept;
#endif

} // namespace peo::core::wave_kernel
