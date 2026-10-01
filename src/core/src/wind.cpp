#include "peo/core/wind.hpp"

#include <algorithm>
#include <cstddef>

namespace peo::core {

namespace {
constexpr std::int32_t kQuarterTurn = 90;
constexpr std::int32_t kHalfTurn = 180;
constexpr std::int32_t kThreeQuarterTurn = 270;

/// round(kTrigOne x cos(d)) for d = 0..90 degrees.
constexpr std::array<std::int32_t, kQuarterTurn + 1> kCosQuarter{
    4096, 4095, 4094, 4090, 4086, 4080, 4074, 4065, 4056, 4046, 4034, 4021, 4006, 3991, 3974, 3956,
    3937, 3917, 3896, 3873, 3849, 3824, 3798, 3770, 3742, 3712, 3681, 3650, 3617, 3582, 3547, 3511,
    3474, 3435, 3396, 3355, 3314, 3271, 3228, 3183, 3138, 3091, 3044, 2996, 2946, 2896, 2845, 2793,
    2741, 2687, 2633, 2578, 2522, 2465, 2408, 2349, 2290, 2231, 2171, 2110, 2048, 1986, 1923, 1860,
    1796, 1731, 1666, 1600, 1534, 1468, 1401, 1334, 1266, 1198, 1129, 1060, 991,  921,  852,  782,
    711,  641,  570,  499,  428,  357,  286,  214,  143,  71,   0};

/// n / d rounded half away from zero, d > 0.
std::int64_t round_div(std::int64_t n, std::int64_t d) noexcept {
    return n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d);
}
} // namespace

bool WindTable::calm() const noexcept {
    return downwind == 0 && std::all_of(step.begin(), step.end(), [](std::int32_t s) { return s == 0; });
}

std::int32_t cos_degrees(std::int32_t degrees) noexcept {
    const std::int32_t a = ((degrees % kDegreesPerTurn) + kDegreesPerTurn) % kDegreesPerTurn;
    const auto at = [](std::int32_t d) { return kCosQuarter[static_cast<std::size_t>(d)]; };
    if (a <= kQuarterTurn) {
        return at(a);
    }
    if (a <= kHalfTurn) {
        return -at(kHalfTurn - a);
    }
    if (a <= kThreeQuarterTurn) {
        return -at(a - kHalfTurn);
    }
    return at(kDegreesPerTurn - a);
}

WindTable wind_table(Wind wind, std::int32_t loss) noexcept {
    WindTable t;
    if (wind.intensity <= 0) {
        return t; // calm: no wind to lose scent to either
    }
    // Toward +y is clockwise on the map, so sin(a) = cos(a - 90).
    const std::int64_t c = cos_degrees(wind.toward_degrees);
    const std::int64_t s = cos_degrees(wind.toward_degrees - kQuarterTurn);
    for (std::size_t d = 0; d < t.step.size(); ++d) {
        const std::int64_t progress = kNeighbours8[d].x * c + kNeighbours8[d].y * s;
        const std::int64_t along = round_div(wind.intensity * progress, kTrigOne);
        const std::int64_t step = along - std::int64_t{loss} * wind.intensity;
        t.step[d] = static_cast<std::int32_t>(std::clamp<std::int64_t>(step, -kMaxWindStep, kMaxWindStep));
        if (along > 0) {
            t.downwind = static_cast<std::uint8_t>(t.downwind | (1U << d));
        }
    }
    return t;
}

} // namespace peo::core
