#include "peo/core/geography.hpp"
#include "peo/core/noise.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <utility>

using namespace peo::core;

namespace {
constexpr Seed kSeed = 77;
constexpr int kHalf = 128; // samples run over [-kHalf, kHalf) on each axis
} // namespace

TEST_SUITE("noise") {
    TEST_CASE("value noise is a pure function of its inputs and stays in range") {
        constexpr int kWavelength = 5;
        int differ = 0;
        for (int y = -kHalf; y < kHalf; ++y) {
            for (int x = -kHalf; x < kHalf; ++x) {
                const std::int32_t v = value_noise(kSeed, x, y, kWavelength);
                if (v < -kNoiseOne || v > kNoiseOne || v != value_noise(kSeed, x, y, kWavelength)) {
                    FAIL("out of range or not repeatable at " << x << "," << y);
                }
                differ += v != value_noise(kSeed + 1, x, y, kWavelength) ? 1 : 0;
            }
        }
        CHECK(differ > (2 * kHalf) * (2 * kHalf) / 2); // another seed is another field
    }

    TEST_CASE("value noise meets its corners and changes smoothly, across zero too") {
        // At wavelength 0 every cell is a lattice point, so it reads the corner values
        // themselves; a lattice point at wavelength w is the corner (x >> w, y >> w).
        for (int w = 1; w <= 8; ++w) {
            CAPTURE(w);
            for (int ly = -4; ly <= 4; ++ly) {
                for (int lx = -4; lx <= 4; ++lx) {
                    CHECK(value_noise(kSeed, std::int64_t{lx} << w, std::int64_t{ly} << w, w) ==
                          value_noise(kSeed, lx, ly, 0));
                }
            }
            // One cell's step moves the smoothstep at most 1.5 x 2^16 / 2^w in Q16, across
            // a corner gap of at most 2^17: 3 x kNoiseOne >> w, plus rounding.
            const std::int32_t bound = (3 * kNoiseOne >> w) + 4;
            for (int y = -kHalf / 2; y < kHalf / 2; ++y) {
                for (int x = -kHalf / 2; x < kHalf / 2; ++x) {
                    const std::int32_t v = value_noise(kSeed, x, y, w);
                    if (std::abs(value_noise(kSeed, x + 1, y, w) - v) > bound ||
                        std::abs(value_noise(kSeed, x, y + 1, w) - v) > bound) {
                        FAIL("a step past the bound at " << x << "," << y);
                    }
                }
            }
        }
    }

    TEST_CASE("scans and rows give the free functions' values, whatever they were asked before") {
        // One NoiseScan and one FbmScan asked in turn for two seeds and several wavelengths,
        // and a NoiseRow / FbmRow reused with another width and origin, over negative and
        // positive cells: every value equals value_noise, fbm and ridged.
        constexpr int kSpan = 300;
        constexpr int kStep = 7;
        const GeographyParams p;
        NoiseScan scan;
        FbmScan fbm_scan;
        for (int y = -kSpan; y < kSpan; y += kStep) {
            for (int x = -kSpan; x < kSpan; x += kStep) {
                const Seed s = (x / kStep) % 2 == 0 ? kSeed : kSeed + 9;
                const int w = ((x + y) / kStep % 5 + 5) % 5 + 1;
                if (scan.value(s, x, y, w) != value_noise(s, x, y, w) ||
                    fbm_scan.fbm(s, x, y, p.hills) != fbm(s, x, y, p.hills) ||
                    fbm_scan.ridged(s, x, y, p.range) != ridged(s, x, y, p.range)) {
                    FAIL("a scan differs at " << x << "," << y);
                }
            }
        }
        NoiseRow row;
        FbmRow fbm_row;
        for (const auto [x0, width] : {std::pair{-kSpan, 2 * kSpan}, std::pair{-17, 41}}) {
            row.reset(kSeed, 4, x0, width);
            fbm_row.reset(kSeed, p.forest, x0, width);
            for (int y = -kSpan; y < kSpan; y += 3) {
                for (int i = 0; i < width; i += 5) {
                    if (row.value(i, y) != value_noise(kSeed, x0 + i, y, 4) ||
                        fbm_row.fbm(i, y) != fbm(kSeed, x0 + i, y, p.forest) ||
                        fbm_row.ridged(i, y) != ridged(kSeed, x0 + i, y, p.forest)) {
                        FAIL("a row differs at " << x0 + i << "," << y);
                    }
                }
            }
        }
    }

    TEST_CASE("fbm and ridged stay in range for the map's octaves") {
        const GeographyParams p;
        for (const Octaves& o : {p.continent, p.range, p.range_mask, p.hills, p.forest}) {
            REQUIRE(valid(o));
            for (int y = -kHalf; y < kHalf; y += 3) {
                for (int x = -kHalf; x < kHalf; x += 3) {
                    const std::int32_t f = fbm(kSeed, x * 37, y * 37, o);
                    const std::int32_t r = ridged(kSeed, x * 37, y * 37, o);
                    if (f < -kNoiseOne || f > kNoiseOne || r < -kNoiseOne || r > kNoiseOne) {
                        FAIL("out of range at " << x << "," << y);
                    }
                }
            }
        }
    }
}
