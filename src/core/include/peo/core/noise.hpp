#pragma once

#include "peo/core/types.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace peo::core {

/// Integer value noise (PEO-095): every value is Q16, kNoiseOne meaning 1.0, so the same
/// seed gives the same bits on every platform (no float). Lattice corners are a hash of
/// (seed, lattice x, lattice y), so a cell's value never depends on what else was asked.
inline constexpr std::int32_t kNoiseOne = 1 << 16;
inline constexpr int kMaxWavelengthLog2 = 30;
/// Octaves a stack may hold: more than eight halvings add nothing a 100 m cell can show,
/// and a scan keeps one lattice square per octave.
inline constexpr int kMaxOctaves = 8;

/// Value noise with a lattice every 2^wavelength_log2 cells (0 to 30): the corner values,
/// in [-kNoiseOne, kNoiseOne), blended by an integer smoothstep in x then y. The lattice
/// index is floor division (an arithmetic shift), so the field is continuous across 0.
/// Worst case inside: the Q16 fraction is below 2^16, and every product is int64.
[[nodiscard]] std::int32_t value_noise(Seed seed, std::int64_t x, std::int64_t y,
                                       int wavelength_log2) noexcept;

/// A stack of octaves: the first at 2^first_wavelength_log2 cells, each next one at half
/// the wavelength with its amplitude times gain_q16 (Q16), `count` in all. count must not
/// exceed first_wavelength_log2 + 1 (the last wavelength is at least one cell) or kMaxOctaves.
struct Octaves {
    int first_wavelength_log2 = 8;
    int count = 3;
    std::int32_t gain_q16 = kNoiseOne / 2;
};
[[nodiscard]] constexpr bool valid(const Octaves& o) noexcept {
    return o.first_wavelength_log2 >= 0 && o.first_wavelength_log2 <= kMaxWavelengthLog2 && o.count >= 1 &&
           o.count <= o.first_wavelength_log2 + 1 && o.count <= kMaxOctaves && o.gain_q16 > 0 &&
           o.gain_q16 <= kNoiseOne;
}

/// Fractal sum of value noise: each octave its own seed (a hash of seed and index),
/// normalised by the amplitudes' sum, so it stays in [-kNoiseOne, kNoiseOne].
[[nodiscard]] std::int32_t fbm(Seed seed, std::int64_t x, std::int64_t y, const Octaves& octaves) noexcept;

/// Ridges for mountain ranges: kNoiseOne - 2|fbm|, highest where fbm crosses zero, in
/// [-kNoiseOne, kNoiseOne].
[[nodiscard]] std::int32_t ridged(Seed seed, std::int64_t x, std::int64_t y, const Octaves& octaves) noexcept;

/// The same values as value_noise, for scanning a field cell by cell: it keeps the last
/// lattice square's corners, so a row of cells hashes once per lattice square instead of
/// once per cell. value_noise is a fresh scan of one cell, so the two cannot differ.
class NoiseScan {
public:
    [[nodiscard]] std::int32_t value(Seed seed, std::int64_t x, std::int64_t y, int wavelength_log2) noexcept;

private:
    Seed seed_ = 0;
    std::int64_t lx_ = 0;
    std::int64_t ly_ = 0;
    int wavelength_log2_ = -1; // none kept yet
    std::array<std::int64_t, 4> corners_{};
};

/// fbm and ridged for scanning: one NoiseScan per octave and the octave seeds derived
/// once. The same values as the free functions.
class FbmScan {
public:
    [[nodiscard]] std::int32_t fbm(Seed seed, std::int64_t x, std::int64_t y,
                                   const Octaves& octaves) noexcept;
    [[nodiscard]] std::int32_t ridged(Seed seed, std::int64_t x, std::int64_t y,
                                      const Octaves& octaves) noexcept;

private:
    Seed seed_ = 0;
    int seeded_ = 0; // octave seeds derived for seed_
    std::array<Seed, kMaxOctaves> octave_seeds_{};
    std::array<NoiseScan, kMaxOctaves> octaves_{};
};

/// value_noise over a run of cells [x0, x0 + width) of row after row (PEO-095): within
/// one lattice row the two horizontal blends depend only on x, so they are kept for the
/// whole run and each cell costs one vertical blend. The same arithmetic on the same
/// values as value_noise, so the same bits. It allocates in reset(), once per region.
class NoiseRow {
public:
    void reset(Seed seed, int wavelength_log2, std::int64_t x0, int width);
    /// The value at (x0 + i, y).
    [[nodiscard]] std::int32_t value(int i, std::int64_t y) noexcept;

private:
    Seed seed_ = 0;
    int wavelength_log2_ = 0;
    std::int64_t x0_ = 0;
    std::int64_t ly_ = 0;
    bool blended_ = false;
    std::vector<std::int64_t> sx_;
    std::vector<std::int64_t> top_;
    std::vector<std::int64_t> bottom_;
    std::int64_t y_ = 0; // the row sy_ is for
    bool row_known_ = false;
    std::int64_t sy_ = 0;
};

/// fbm and ridged over a run of cells, one NoiseRow per octave: the same values as the
/// free functions.
class FbmRow {
public:
    void reset(Seed seed, const Octaves& octaves, std::int64_t x0, int width);
    [[nodiscard]] std::int32_t fbm(int i, std::int64_t y) noexcept;
    [[nodiscard]] std::int32_t ridged(int i, std::int64_t y) noexcept;

private:
    Octaves octaves_{};
    std::array<NoiseRow, kMaxOctaves> rows_{};
    std::array<std::int64_t, kMaxOctaves> amplitude_{}; // each octave's weight, and their sum
    std::int64_t amplitudes_ = 0;
};

} // namespace peo::core
