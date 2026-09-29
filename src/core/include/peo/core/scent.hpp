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
    /// Scent below this is clamped to zero so the field stays sparse.
    float floor = 1e-6F;
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
    /// receive nor emit scent; pass nullptr for an open field.
    void step(const Grid<bool>* blocked = nullptr) noexcept;

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
    ScentParams params_;
    Grid<float> front_;
    Grid<float> back_;
};

} // namespace peo::core
