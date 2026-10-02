#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace peo::core {

/// One update's rounds of a calm scent wave, as a field backend runs them (ADR-0014,
/// PEO-081). Plain views of the wave's own arrays: the backend reads the masks and the
/// values, writes the values back after the rounds, and fills the per-tile flags and
/// every cell's direction byte. Integer max and subtraction only, so any backend gives
/// the CPU pull's bits.
struct FieldRounds {
    int width = 0;
    int height = 0;
    int tiles_x = 0;
    int tiles_y = 0;
    int tile_width = 0;
    int tile_height = 0;
    /// The wave's identity token: while it is unchanged, so are the masks (one stage).
    std::uint64_t stage = 0;
    /// Per cell: 1 when open; and the diagonals that may offer into it (NE, SE, SW, NW).
    const std::uint8_t* open = nullptr;
    const std::uint8_t* diag = nullptr;
    /// In: the field before the rounds (deposits included). Out: after them.
    std::int32_t* values = nullptr;
    /// Out, per tile: changed in the last full round or any gust round after it (what
    /// the next update pulls); changed in any round.
    std::uint8_t* last_changed = nullptr;
    std::uint8_t* any_changed = nullptr;
    /// Out, per cell: the direction byte for the new values (PEO-079).
    std::uint8_t* flow = nullptr;
    std::int32_t distance_cost = 0;
    std::int32_t age_line = 0;
    /// Full rounds (the wave's speed).
    int rounds = 0;
    /// Under a wind (PEO-087): per cell, how far the wind reaches it (0 indoors, 255
    /// outdoors). Per neighbour k in kNeighbours8 order, for the offer from that neighbour
    /// into a cell: the step taken off its cost, scaled by the sender's openness / 255
    /// (positive cheaper, negative dearer); bit k of gust_from when it offers in a gust
    /// round (it lies upwind). gust_rounds of those follow the full ones. Calm: windy is
    /// false and the rest is ignored.
    bool windy = false;
    const std::uint8_t* openness = nullptr;
    std::array<std::int32_t, 8> wind_step{};
    std::uint8_t gust_from = 0;
    int gust_rounds = 0;
};

/// Where a calm wave's update can run instead of ScentWave's own CPU pull, which is the
/// reference every backend must match bit for bit (ADR-0014). Core holds only this
/// seam: a GPU backend lives in its own library (peo_gpu), handed in by the frontend,
/// and core never creates a device. run() may decline (a field too small to pay, no
/// device): the wave then runs the CPU pull, with the same result.
class FieldBackend {
public:
    FieldBackend() = default;
    FieldBackend(const FieldBackend&) = delete;
    FieldBackend& operator=(const FieldBackend&) = delete;
    virtual ~FieldBackend() = default;

    /// Run `rounds`; false when this backend will not, leaving everything untouched.
    [[nodiscard]] virtual bool run(const FieldRounds& rounds) = 0;
};

} // namespace peo::core
