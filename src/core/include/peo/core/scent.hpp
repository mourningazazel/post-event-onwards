#pragma once

#include "peo/core/grid.hpp"
#include "peo/core/types.hpp"

#include <optional>

namespace peo::core {

/// Scent a standing player deposits per turn. The field runs on a 0-500 scale
/// (D-007) so a usable gradient reaches the map edge instead of flushing to zero;
/// measurements in docs/design/scent-performance.md.
inline constexpr float kPlayerScent = 500.0F;

/// Tunables for one scent layer. Kept as a struct so stages can vary them.
struct ScentParams {
    /// Fraction of each cell's scent that spreads to its 4 orthogonal neighbours per step.
    /// 0.5, not higher: the checkerboard mode is amplified by 1 - 2 * diffusion, so 0.5
    /// damps it exactly and the trail stays monotone (0.8 ripples and misleads climbers).
    float diffusion = 0.5F;
    /// Fraction of scent that evaporates each step, applied after diffusion.
    float decay = 0.01F;
    /// Scent below this is clamped to zero so the field stays sparse. Reach goes as
    /// lambda * ln(peak / floor) (D-007): at 1e-6 the field was cut off at 25-29
    /// cells on real stages and distant Dead never moved; 1e-30 spans an 80x45
    /// stage. Not lower: a cell at the floor times the smallest factor step applies
    /// (diffusion / 4) must stay far above FLT_MIN (1.18e-38), or step would touch
    /// subnormals, which are slow on many CPUs.
    float floor = 1e-30F;
};

/// A diffusing, decaying scalar field. The player (and anything else that
/// smells) deposits into it; hordes climb its gradient. This is the single
/// mechanism behind "scent-driven hordes", so it must stay cheap: one step is
/// O(cells) with no allocation and touches memory linearly.
class ScentField {
public:
    ScentField(int width, int height, ScentParams params = {});

    void deposit(Vec2i at, float amount) noexcept;
    void clear() noexcept;

    /// Advance the field one simulation step. Walls (blocked cells) neither
    /// receive nor emit scent; pass nullptr for an open field. Same result as
    /// step_linear() then clamp_floor(), in one sweep.
    void step(const Grid<bool>* blocked = nullptr) noexcept;

    /// The linear part of step(): diffuse and decay, no floor clamp. Linear in the
    /// field, so a deposit's effect can be added afterwards (patch_deposit).
    void step_linear(const Grid<bool>* blocked = nullptr) noexcept;

    /// The only nonlinearity: flush values below params().floor to zero.
    void clamp_floor() noexcept;

    /// Add what step_linear() would have made of `amount` deposited at `at` before
    /// it ran: the source cell and its four open neighbours, nothing else. Call it
    /// between step_linear() and clamp_floor(). Lets PEO-007 speculate a turn
    /// without the player's deposit and patch it in once the move is known.
    void patch_deposit(Vec2i at, float amount, const Grid<bool>* blocked = nullptr) noexcept;

    [[nodiscard]] float sample(Vec2i at) const noexcept;
    [[nodiscard]] float total() const noexcept;
    [[nodiscard]] const Grid<float>& cells() const noexcept { return front_; }
    [[nodiscard]] const ScentParams& params() const noexcept { return params_; }
    [[nodiscard]] int width() const noexcept { return front_.width(); }
    [[nodiscard]] int height() const noexcept { return front_.height(); }

    /// The neighbouring cell (8-connected) with the strongest scent that is
    /// strictly stronger than `from`. Empty when nothing pulls harder.
    [[nodiscard]] std::optional<Vec2i>
    strongest_neighbour(Vec2i from, const Grid<bool>* blocked = nullptr) const noexcept;

private:
    void sweep(const Grid<bool>* blocked, bool clamp) noexcept;

    ScentParams params_;
    Grid<float> front_;
    Grid<float> back_;
};

} // namespace peo::core
