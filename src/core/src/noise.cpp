#include "peo/core/noise.hpp"

#include "peo/core/rng.hpp"

#include <array>
#include <cstdlib>

namespace peo::core {

namespace {
constexpr int kQ16 = 16;
/// The top 17 bits of a hash, as a corner value in [-kNoiseOne, kNoiseOne).
constexpr int kCornerShift = 64 - (kQ16 + 1);
/// Mixed into each octave's seed so octaves are independent fields.
constexpr std::uint64_t kOctaveSalt = 0x0C7A11ULL;

std::int64_t corner(Seed seed, std::int64_t cx, std::int64_t cy) noexcept {
    const std::uint64_t h = hash_u64(seed, static_cast<std::uint64_t>(cx), static_cast<std::uint64_t>(cy));
    return static_cast<std::int64_t>(h >> kCornerShift) - kNoiseOne;
}

/// 3t^2 - 2t^3 for t in Q16 [0, kNoiseOne]: zero slope at both ends, so the blend has no
/// crease at a lattice line.
std::int64_t smoothstep(std::int64_t t) noexcept {
    const std::int64_t t2 = (t * t) >> kQ16;
    return (t2 * (3 * std::int64_t{kNoiseOne} - 2 * t)) >> kQ16;
}

std::int64_t lerp(std::int64_t a, std::int64_t b, std::int64_t s) noexcept {
    return a + (((b - a) * s) >> kQ16); // |b - a| < 2^18, s <= 2^16: the product fits easily
}

Seed octave_seed(Seed seed, int i) noexcept {
    return hash_u64(seed, kOctaveSalt, static_cast<std::uint64_t>(i));
}
} // namespace

std::int32_t NoiseScan::value(Seed seed, std::int64_t x, std::int64_t y, int wavelength_log2) noexcept {
    const std::int64_t lx = x >> wavelength_log2; // floor division, defined for negatives (C++20)
    const std::int64_t ly = y >> wavelength_log2;
    if (wavelength_log2 != wavelength_log2_ || seed != seed_ || lx != lx_ || ly != ly_) {
        seed_ = seed;
        wavelength_log2_ = wavelength_log2;
        lx_ = lx;
        ly_ = ly;
        corners_ = {corner(seed, lx, ly), corner(seed, lx + 1, ly), corner(seed, lx, ly + 1),
                    corner(seed, lx + 1, ly + 1)};
    }
    const std::int64_t fx = x - (lx << wavelength_log2);
    const std::int64_t fy = y - (ly << wavelength_log2);
    const std::int64_t sx = smoothstep((fx << kQ16) >> wavelength_log2);
    const std::int64_t sy = smoothstep((fy << kQ16) >> wavelength_log2);
    const std::int64_t top = lerp(corners_[0], corners_[1], sx);
    const std::int64_t bottom = lerp(corners_[2], corners_[3], sx);
    return static_cast<std::int32_t>(lerp(top, bottom, sy));
}

std::int32_t value_noise(Seed seed, std::int64_t x, std::int64_t y, int wavelength_log2) noexcept {
    return NoiseScan{}.value(seed, x, y, wavelength_log2);
}

std::int32_t FbmScan::fbm(Seed seed, std::int64_t x, std::int64_t y, const Octaves& octaves) noexcept {
    if (seed != seed_ || octaves.count > seeded_) { // derive only the seeds this stack uses
        if (seed != seed_) {
            seeded_ = 0;
            seed_ = seed;
        }
        for (int i = seeded_; i < octaves.count; ++i) {
            octave_seeds_[static_cast<std::size_t>(i)] = octave_seed(seed, i);
        }
        seeded_ = octaves.count;
    }
    std::int64_t sum = 0;
    std::int64_t amplitudes = 0;
    std::int64_t amplitude = kNoiseOne;
    for (int i = 0; i < octaves.count; ++i) {
        const auto k = static_cast<std::size_t>(i);
        sum += amplitude *
               octaves_[k].value(octave_seeds_[k], x, y, octaves.first_wavelength_log2 - i); // < 2^33
        amplitudes += amplitude;
        amplitude = (amplitude * octaves.gain_q16) >> kQ16;
    }
    return static_cast<std::int32_t>(sum / amplitudes);
}

std::int32_t FbmScan::ridged(Seed seed, std::int64_t x, std::int64_t y, const Octaves& octaves) noexcept {
    return kNoiseOne - 2 * std::abs(fbm(seed, x, y, octaves));
}

void NoiseRow::reset(Seed seed, int wavelength_log2, std::int64_t x0, int width) {
    seed_ = seed;
    wavelength_log2_ = wavelength_log2;
    x0_ = x0;
    blended_ = false;
    row_known_ = false;
    const auto n = static_cast<std::size_t>(width);
    sx_.resize(n);
    top_.resize(n);
    bottom_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::int64_t x = x0 + static_cast<std::int64_t>(i);
        const std::int64_t fx = x - ((x >> wavelength_log2) << wavelength_log2);
        sx_[i] = smoothstep((fx << kQ16) >> wavelength_log2);
    }
}

std::int32_t NoiseRow::value(int i, std::int64_t y) noexcept {
    const std::int64_t ly = y >> wavelength_log2_;
    if (!blended_ || ly != ly_) {
        // A new lattice row: the run's horizontal blends, the corners read once a square.
        ly_ = ly;
        blended_ = true;
        std::int64_t lx_kept = 0;
        bool have = false;
        std::array<std::int64_t, 4> c{};
        for (std::size_t k = 0; k < sx_.size(); ++k) {
            const std::int64_t lx = (x0_ + static_cast<std::int64_t>(k)) >> wavelength_log2_;
            if (!have || lx != lx_kept) {
                c = {corner(seed_, lx, ly), corner(seed_, lx + 1, ly), corner(seed_, lx, ly + 1),
                     corner(seed_, lx + 1, ly + 1)};
                lx_kept = lx;
                have = true;
            }
            top_[k] = lerp(c[0], c[1], sx_[k]);
            bottom_[k] = lerp(c[2], c[3], sx_[k]);
        }
    }
    if (!row_known_ || y != y_) { // the vertical blend is the same along a row
        y_ = y;
        row_known_ = true;
        const std::int64_t fy = y - (ly << wavelength_log2_);
        sy_ = smoothstep((fy << kQ16) >> wavelength_log2_);
    }
    const auto k = static_cast<std::size_t>(i);
    return static_cast<std::int32_t>(lerp(top_[k], bottom_[k], sy_));
}

void FbmRow::reset(Seed seed, const Octaves& octaves, std::int64_t x0, int width) {
    octaves_ = octaves;
    // The octaves' weights as fbm computes them, once.
    std::int64_t amplitude = kNoiseOne;
    amplitudes_ = 0;
    for (int i = 0; i < octaves.count; ++i) {
        const auto k = static_cast<std::size_t>(i);
        rows_[k].reset(octave_seed(seed, i), octaves.first_wavelength_log2 - i, x0, width);
        amplitude_[k] = amplitude;
        amplitudes_ += amplitude;
        amplitude = (amplitude * octaves.gain_q16) >> kQ16;
    }
}

std::int32_t FbmRow::fbm(int i, std::int64_t y) noexcept {
    std::int64_t sum = 0;
    for (int o = 0; o < octaves_.count; ++o) {
        const auto k = static_cast<std::size_t>(o);
        sum += amplitude_[k] * rows_[k].value(i, y);
    }
    return static_cast<std::int32_t>(sum / amplitudes_);
}

std::int32_t FbmRow::ridged(int i, std::int64_t y) noexcept {
    return kNoiseOne - 2 * std::abs(fbm(i, y));
}

std::int32_t fbm(Seed seed, std::int64_t x, std::int64_t y, const Octaves& octaves) noexcept {
    // FbmScan's sum for one cell, without its per-octave caches (a point has nothing to keep).
    std::int64_t sum = 0;
    std::int64_t amplitudes = 0;
    std::int64_t amplitude = kNoiseOne;
    for (int i = 0; i < octaves.count; ++i) {
        sum += amplitude * value_noise(octave_seed(seed, i), x, y, octaves.first_wavelength_log2 - i);
        amplitudes += amplitude;
        amplitude = (amplitude * octaves.gain_q16) >> kQ16;
    }
    return static_cast<std::int32_t>(sum / amplitudes);
}

std::int32_t ridged(Seed seed, std::int64_t x, std::int64_t y, const Octaves& octaves) noexcept {
    return kNoiseOne - 2 * std::abs(fbm(seed, x, y, octaves));
}

} // namespace peo::core
