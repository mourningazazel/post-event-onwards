#include "peo/core/look.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>

namespace peo::core {

namespace {

/// Read only by step()'s assert, so a release build would otherwise warn it unused.
[[nodiscard, maybe_unused]] bool is_unit_step(Vec2i dir) noexcept {
    return std::find(std::begin(kNeighbours8), std::end(kNeighbours8), dir) != std::end(kNeighbours8);
}

[[nodiscard]] bool contains(CellRect rect, Vec2i cell) noexcept {
    return cell.x >= rect.min.x && cell.y >= rect.min.y && cell.x <= rect.max.x && cell.y <= rect.max.y;
}

} // namespace

void LookCursor::begin(Vec2i player) noexcept {
    cell_ = player;
    active_ = true;
}

void LookCursor::end() noexcept {
    active_ = false;
}

bool LookCursor::step(Vec2i dir, int cells, const SightField& sight, CellRect bounds) noexcept {
    assert(is_unit_step(dir));
    assert(cells >= 1);
    if (!active_) {
        return false;
    }
    const Vec2i start = cell_;
    for (int i = 0; i < cells; ++i) {
        const Vec2i next = cell_ + dir;
        if (!contains(bounds, next) || !sight.seen(next)) {
            break;
        }
        cell_ = next;
    }
    return cell_ != start;
}

std::optional<LookReport> look_at(const SightField& sight, const Stage& stage, std::span<const Dead> horde,
                                  Vec2i player, Vec2i cell) {
    // A field computed over another stage could see past this one's edge.
    if (!sight.seen(cell) || !stage.blocked.in_bounds(cell)) {
        return std::nullopt;
    }
    // Max 2 * 2048^2 = 8.4e6 with both cells on a stage of side at most 2048 (PEO-083): far
    // inside int32. player is not asserted to be sight.origin() so the Dead that can see may
    // pass their own origin later.
    const Vec2i d = cell - player;
    const auto dead =
        std::count_if(horde.begin(), horde.end(), [cell](const Dead& unit) { return unit.pos == cell; });
    return LookReport{cell,
                      d.x * d.x + d.y * d.y,
                      cell == player,
                      stage.blocked.at(cell) != 0,
                      cell == stage.exit,
                      static_cast<std::uint32_t>(dead)};
}

} // namespace peo::core
