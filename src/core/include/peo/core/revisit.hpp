#pragma once

#include "peo/core/aftermath.hpp"
#include "peo/core/dead.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace peo::core {

/// A stored area (ADR-0019): which generated stage it is, the event clock it was
/// stored at, and its Dead. The stage itself is regenerated from the seed on resume;
/// scent is not carried.
struct AreaRecord {
    std::uint32_t stage_index = 0;
    EventSubsteps updated_at = 0;
    std::vector<Dead> horde;
};

/// Bring a stored horde to the clock `now` in closed form (ADR-0019 point 2): drop
/// every unit whose fate the curve has passed, keeping the survivors in their order.
/// One comparison a unit, whatever the absence; and it reads only `now`, never when
/// the horde was stored, so several short absences leave exactly what one long one does.
inline void thin_horde(std::vector<Dead>& horde, const Curve& attrition, EventSubsteps now) {
    const std::uint32_t gone = affected_at(attrition, now);
    std::erase_if(horde, [&](const Dead& d) { return d.fate < gone; });
}

} // namespace peo::core
