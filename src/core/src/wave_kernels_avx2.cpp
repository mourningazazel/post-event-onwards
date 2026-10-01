// The AVX2 copy of the scent wave's row kernels (PEO-085). GCC and Clang build only
// these two functions for AVX2 (the target attribute); MSVC has no such attribute, so
// CMake builds this whole file with /arch:AVX2. ScentWave calls them only after a CPU
// check, so nothing else here may run: see wave_kernels.hpp on why it calls no library
// code.

#include "wave_kernels.hpp"

#if PEO_WAVE_HAS_AVX2

#if defined(__GNUC__) || defined(__clang__)
#define PEO_WAVE_AVX2_TARGET __attribute__((target("avx2")))
#else
#define PEO_WAVE_AVX2_TARGET
#endif

namespace peo::core::wave_kernel {

PEO_WAVE_AVX2_TARGET bool pull_row_avx2(const std::int32_t* a, std::int32_t* b, const std::uint8_t* open,
                                        const std::uint8_t* diag, std::ptrdiff_t w, std::ptrdiff_t begin,
                                        std::ptrdiff_t end, std::int32_t cost, std::int32_t line) noexcept {
    return pull_row(a, b, open, diag, w, begin, end, cost, line);
}

PEO_WAVE_AVX2_TARGET void flow_row_avx2(const std::int32_t* a, std::uint8_t* flow, const std::uint8_t* open,
                                        const std::uint8_t* diag, std::ptrdiff_t w, std::ptrdiff_t begin,
                                        std::ptrdiff_t end) noexcept {
    flow_row(a, flow, open, diag, w, begin, end);
}

} // namespace peo::core::wave_kernel

#endif
