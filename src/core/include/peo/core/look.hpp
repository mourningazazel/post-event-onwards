#pragma once

// PEO-120: looking at a seen cell (D-049). A cursor the player moves over what they can see
// now, and a report of what stands on the cell under it. Both read the sight field and the
// stage and change nothing, so looking never spends a turn or touches the world. The cursor
// lives in stage cells, never screen cells, so a later zoom (D-050) needs nothing here.

#include "peo/core/dead.hpp"
#include "peo/core/sight.hpp"
#include "peo/core/stage.hpp"
#include "peo/core/types.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace peo::core {

/// Cells one shifted cursor step tries to cover: a technical default, so the frontend holds
/// no number of its own.
inline constexpr int kLookJumpCells = 8;

/// 20 ft at the D-049 tile scale: within this many cells signs read and things show their
/// generic labels; past it they show as present only (D-053 A).
inline constexpr std::int32_t kLegibleRangeCells = 6;

/// Whether a cell at `distance_sq` from the player is close enough to make things out. The
/// same disc test sight uses, so a cell is legible exactly when its whole-cell distance is
/// within kLegibleRangeCells.
[[nodiscard]] constexpr bool legible(std::int32_t distance_sq) noexcept {
    return distance_sq <= kLegibleRangeCells * kLegibleRangeCells;
}

/// The Look cursor. It only ever rests on a cell the player sees now, so nothing it points
/// at can tell the player more than sight already has.
class LookCursor {
public:
    /// Start looking with the cursor on the player.
    void begin(Vec2i player) noexcept;

    /// Stop looking; the cursor stays put until the next begin.
    void end() noexcept;

    [[nodiscard]] bool active() const noexcept { return active_; }

    /// The cursor in stage cells (D-050).
    [[nodiscard]] Vec2i cell() const noexcept { return cell_; }

    /// Move up to `cells` cells along `dir` (one of kNeighbours8, asserted; cells >= 1,
    /// asserted), one cell at a time, stopping before the first cell `sight` does not see
    /// or that lies outside `bounds`. `bounds` is the D-052 seam: the caller passes what
    /// the cursor may reach (sight.window() under D-052 B); the cursor knows no screen.
    /// Returns whether it moved; an inactive cursor never moves.
    bool step(Vec2i dir, int cells, const SightField& sight, CellRect bounds) noexcept;

private:
    Vec2i cell_{};
    bool active_ = false;
};

/// What is at one seen cell: data only, no words (D-042).
struct LookReport {
    Vec2i cell;
    /// Squared distance from the player in cells: compared with r * r, the same disc test
    /// sight uses, so no square root and no float.
    std::int32_t distance_sq;
    /// legible(distance_sq): things here can be made out, not merely seen (PEO-121).
    bool legible;
    bool player;
    /// The cell blocks sight (a wall today).
    bool opaque;
    bool exit;
    /// How many of the Dead stand here (shared occupancy, ADR-0013).
    std::uint32_t dead;
};

/// The report for `cell`, or nullopt for any cell `sight` does not see, so nothing unseen is
/// ever reported. Takes parts, not World, so tests build it by hand and the Dead that can
/// see can reuse it. Walks `horde` once: called per cursor move, never per update.
[[nodiscard]] std::optional<LookReport> look_at(const SightField& sight, const Stage& stage,
                                                std::span<const Dead> horde, Vec2i player, Vec2i cell);

} // namespace peo::core
