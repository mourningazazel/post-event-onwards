#pragma once

#include "peo/core/types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace peo::core {

/// Game time since the Event, in substeps (ADR-0009, ADR-0019): one clock across every
/// stage. Sixty-four bits, because 32 run out after about 34 game years.
using EventSubsteps = std::uint64_t;
inline constexpr std::uint32_t kHoursPerDay = 24;
inline constexpr EventSubsteps kSubstepsPerDay = EventSubsteps{kSubstepsPerHour} * kHoursPerDay;

/// How much of something an aftermath effect has reached, in parts of this.
inline constexpr std::uint32_t kAffectedScale = 65536;
/// The most a knot may say: one below the scale, so a fate (at most 65535) drawn at a
/// clock is never already past the curve there.
inline constexpr std::uint32_t kMaxAffected = kAffectedScale - 1;
inline constexpr std::size_t kMaxCurveKnots = 8;

/// One point of an aftermath curve: by `day` since the Event, `affected` parts of
/// kAffectedScale have met the effect.
struct CurveKnot {
    std::uint32_t day = 0;
    std::uint32_t affected = 0;
    friend bool operator==(const CurveKnot&, const CurveKnot&) = default;
};

/// An aftermath curve (ADR-0009): piecewise linear between its knots, flat before the
/// first and after the last (it saturates, ADR-0011 point 2). Fixed size: no allocation.
struct Curve {
    std::array<CurveKnot, kMaxCurveKnots> knots{};
    std::uint8_t count = 0;
    friend bool operator==(const Curve&, const Curve&) = default;
};

/// A curve affected_at can read: one to kMaxCurveKnots knots, days strictly rising,
/// values never falling and never past kMaxAffected.
[[nodiscard]] constexpr bool valid(const Curve& c) noexcept {
    if (c.count == 0 || c.count > kMaxCurveKnots) {
        return false;
    }
    for (std::size_t i = 0; i < c.count; ++i) {
        if (c.knots[i].affected > kMaxAffected) {
            return false;
        }
        if (i > 0 &&
            (c.knots[i].day <= c.knots[i - 1].day || c.knots[i].affected < c.knots[i - 1].affected)) {
            return false;
        }
    }
    return true;
}

/// The curve's value at `t`, in parts of kAffectedScale. Integer maths: between two
/// knots the distances are shifted right until the product fits 64 bits, which keeps the
/// result non-decreasing in `t` and exact at every knot. `c` must be valid().
[[nodiscard]] constexpr std::uint32_t affected_at(const Curve& c, EventSubsteps t) noexcept {
    const auto at = [&](std::size_t i) { return EventSubsteps{c.knots[i].day} * kSubstepsPerDay; };
    if (t <= at(0)) {
        return c.knots[0].affected;
    }
    for (std::size_t i = 1; i < c.count; ++i) {
        if (t < at(i)) {
            constexpr int kProductBits = 46; // a rise is at most 2^17, so 17 + 46 < 64
            const EventSubsteps span = at(i) - at(i - 1);
            const int shift = std::max(0, std::bit_width(span) - kProductBits);
            const std::uint64_t rise = c.knots[i].affected - c.knots[i - 1].affected;
            const std::uint64_t part = rise * ((t - at(i - 1)) >> shift) / (span >> shift);
            return c.knots[i - 1].affected + static_cast<std::uint32_t>(part);
        }
    }
    return c.knots[c.count - 1].affected;
}

// The Dead gone by each day since the Event: proposals the owner tunes (ADR-0009 says
// its parameters are proposals; content data takes the curve over once content loads).
// See docs/design/world-generation.md, 'Aftermath model', the Dead attrition row.
inline constexpr std::uint32_t kAttritionWeekDay = 7;
inline constexpr std::uint32_t kAttritionWeek = kAffectedScale * 5 / 100; // 5% at one week
inline constexpr std::uint32_t kAttritionMonthDay = 30;
inline constexpr std::uint32_t kAttritionMonth = kAffectedScale * 25 / 100; // 25% at one month
inline constexpr std::uint32_t kAttritionYearDay = 365;
inline constexpr std::uint32_t kAttritionYear = kAffectedScale * 70 / 100; // 70% at one year
inline constexpr std::uint32_t kAttritionLongDay = 1825;
inline constexpr std::uint32_t kAttritionLong = kAffectedScale * 85 / 100; // 85% at five years, then flat

inline constexpr Curve kDefaultAttrition{.knots = {{{.day = 0, .affected = 0},
                                                    {.day = kAttritionWeekDay, .affected = kAttritionWeek},
                                                    {.day = kAttritionMonthDay, .affected = kAttritionMonth},
                                                    {.day = kAttritionYearDay, .affected = kAttritionYear},
                                                    {.day = kAttritionLongDay, .affected = kAttritionLong}}},
                                         .count = 5};
static_assert(valid(kDefaultAttrition));

/// `c` with every knot past its count zeroed, so equal behaviour compares equal.
[[nodiscard]] constexpr Curve normalized(Curve c) noexcept {
    for (std::size_t i = c.count; i < kMaxCurveKnots; ++i) {
        c.knots[i] = {};
    }
    return c;
}

/// A unit's attrition threshold (ADR-0009), drawn from `hash` when it appears at clock
/// `at`: uniform in [affected_at(at), 65535], so a unit that exists at `at` has not yet
/// met the curve, and it is gone once affected_at passes it.
[[nodiscard]] constexpr std::uint16_t draw_fate(const Curve& c, EventSubsteps at,
                                                std::uint64_t hash) noexcept {
    constexpr std::uint32_t kTop = std::numeric_limits<std::uint16_t>::max();
    const std::uint32_t low = std::min(affected_at(c, at), kTop); // valid() keeps it <= kTop
    return static_cast<std::uint16_t>(low + hash % (kTop - low + 1));
}

} // namespace peo::core
