#pragma once

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
    /// Out, per tile: changed in the last round; changed in any round.
    std::uint8_t* last_changed = nullptr;
    std::uint8_t* any_changed = nullptr;
    /// Out, per cell: the direction byte for the new values (PEO-079).
    std::uint8_t* flow = nullptr;
    std::int32_t distance_cost = 0;
    std::int32_t age_line = 0;
    int rounds = 0;
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
