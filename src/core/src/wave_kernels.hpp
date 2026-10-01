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
#include "peo/core/wind.hpp"

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

/// One round's wind for pull_row_wind, per neighbour offset k (kNeighbours8[k] from the
/// receiving cell; the step runs the other way, from that neighbour into the receiver).
/// Plain arrays: see the note on library code above.
struct WindRound {
    std::int32_t magnitude[kDirections]; ///< |step| of that direction of travel
    std::int32_t sign[kDirections];      ///< +1 cheaper (downwind), -1 dearer, 0 no wind that way
    std::int32_t allowed[kDirections];   ///< 1 when this round carries scent that way
    std::int32_t need_open;              ///< 1 in a gust round: only an outdoor sender offers
};

/// A step's cost under the wind (PEO-048): the distance cost less the direction's step
/// scaled by the sender's openness / 255, truncated toward zero, never below 1.
PEO_WAVE_INLINE std::int32_t wind_cost(std::int32_t cost, std::int32_t sign, std::int32_t magnitude,
                                       std::int32_t openness) noexcept {
    const auto scaled = static_cast<std::int32_t>(static_cast<std::uint32_t>(magnitude * openness) /
                                                  static_cast<std::uint32_t>(kOpennessOutdoors));
    return max32(1, cost - sign * scaled);
}

/// One neighbour's offer under the wind: its value less its step's cost, or kUnreached
/// where this round takes nothing from it (a corner, a direction the round does not
/// carry, or in a gust round an indoor sender).
PEO_WAVE_INLINE std::int32_t wind_offer(std::int32_t stored, std::int32_t op, std::int32_t corner,
                                        std::int32_t allowed, std::int32_t any_sender, std::int32_t cost,
                                        std::int32_t sign, std::int32_t magnitude) noexcept {
    const std::int32_t ok = corner & allowed & (any_sender | (op != 0 ? 1 : 0));
    const std::int32_t offer = stored - wind_cost(cost, sign, magnitude, op);
    return ok != 0 ? offer : kUnreached;
}

/// pull_row under a wind (PEO-048): each neighbour's offer is its value less its own
/// step's cost, and a gust round takes offers only along its allowed directions and
/// only from outdoor senders. Branch-free as pull_row, and the eight directions are
/// written out (kNeighbours8 order: N, NE, E, SE, S, SW, W, NW): as a loop, GCC leaves
/// this body too large to unroll and so never vectorises the row.
PEO_WAVE_INLINE bool pull_row_wind(const std::int32_t* __restrict a, std::int32_t* __restrict b,
                                   const std::uint8_t* __restrict open, const std::uint8_t* __restrict diag,
                                   const std::uint8_t* __restrict openness, std::ptrdiff_t w,
                                   std::ptrdiff_t begin, std::ptrdiff_t end, std::int32_t cost,
                                   std::int32_t line, const WindRound* __restrict wind) noexcept {
    const std::int32_t m0 = wind->magnitude[0], m1 = wind->magnitude[1], m2 = wind->magnitude[2],
                       m3 = wind->magnitude[3], m4 = wind->magnitude[4], m5 = wind->magnitude[5],
                       m6 = wind->magnitude[6], m7 = wind->magnitude[7];
    const std::int32_t s0 = wind->sign[0], s1 = wind->sign[1], s2 = wind->sign[2], s3 = wind->sign[3],
                       s4 = wind->sign[4], s5 = wind->sign[5], s6 = wind->sign[6], s7 = wind->sign[7];
    const std::int32_t l0 = wind->allowed[0], l1 = wind->allowed[1], l2 = wind->allowed[2],
                       l3 = wind->allowed[3], l4 = wind->allowed[4], l5 = wind->allowed[5],
                       l6 = wind->allowed[6], l7 = wind->allowed[7];
    const std::int32_t any = wind->need_open == 0 ? 1 : 0;
    int changed = 0;
    for (std::ptrdiff_t i = begin; i < end; ++i) {
        const std::int32_t m = diag[i];
        const std::ptrdiff_t n = i - w;
        const std::ptrdiff_t ne = i - w + 1;
        const std::ptrdiff_t e = i + 1;
        const std::ptrdiff_t se = i + w + 1;
        const std::ptrdiff_t s = i + w;
        const std::ptrdiff_t sw = i + w - 1;
        const std::ptrdiff_t west = i - 1;
        const std::ptrdiff_t nw = i - w - 1;
        std::int32_t best = wind_offer(a[n], openness[n], 1, l0, any, cost, s0, m0);
        best = max32(best, wind_offer(a[ne], openness[ne], m & kNE, l1, any, cost, s1, m1));
        best = max32(best, wind_offer(a[e], openness[e], 1, l2, any, cost, s2, m2));
        best = max32(best, wind_offer(a[se], openness[se], (m & kSE) >> 1, l3, any, cost, s3, m3));
        best = max32(best, wind_offer(a[s], openness[s], 1, l4, any, cost, s4, m4));
        best = max32(best, wind_offer(a[sw], openness[sw], (m & kSW) >> 2, l5, any, cost, s5, m5));
        best = max32(best, wind_offer(a[west], openness[west], 1, l6, any, cost, s6, m6));
        best = max32(best, wind_offer(a[nw], openness[nw], (m & kNW) >> 3, l7, any, cost, s7, m7));
        const std::int32_t old = a[i];
        const bool take = (open[i] != 0) & (best > line) & (best > old);
        b[i] = take ? best : old;
        changed |= static_cast<int>(take);
    }
    return changed != 0;
}

#if PEO_WAVE_HAS_AVX2
/// The same kernels built for AVX2 (wave_kernels_avx2.cpp). Call only when the CPU has it.
bool pull_row_avx2(const std::int32_t* a, std::int32_t* b, const std::uint8_t* open, const std::uint8_t* diag,
                   std::ptrdiff_t w, std::ptrdiff_t begin, std::ptrdiff_t end, std::int32_t cost,
                   std::int32_t line) noexcept;
void flow_row_avx2(const std::int32_t* a, std::uint8_t* flow, const std::uint8_t* open,
                   const std::uint8_t* diag, std::ptrdiff_t w, std::ptrdiff_t begin,
                   std::ptrdiff_t end) noexcept;
bool pull_row_wind_avx2(const std::int32_t* a, std::int32_t* b, const std::uint8_t* open,
                        const std::uint8_t* diag, const std::uint8_t* openness, std::ptrdiff_t w,
                        std::ptrdiff_t begin, std::ptrdiff_t end, std::int32_t cost, std::int32_t line,
                        const WindRound* wind) noexcept;
#endif

} // namespace peo::core::wave_kernel
