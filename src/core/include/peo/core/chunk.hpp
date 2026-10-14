#pragma once

// PEO-146: where a cell is on the one map (ADR-0007, PEO-118). World cells and chunks are
// 64-bit, so the map has no edge a player can reach; a chunk is 32x32 cells (GAMEPLAN §3.2,
// world-generation.md L4). Everything here is a pure function of its inputs.

#include "peo/core/rng.hpp"
#include "peo/core/types.hpp"

#include <compare>
#include <cstdint>

namespace peo::core {

/// Cells along one side of a chunk.
inline constexpr std::int64_t kChunkSide = 32;

/// A cell of the one map.
struct WorldCell {
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend constexpr bool operator==(WorldCell, WorldCell) noexcept = default;
    friend constexpr auto operator<=>(WorldCell, WorldCell) noexcept = default;
    friend constexpr WorldCell operator+(WorldCell a, Vec2i d) noexcept { return {a.x + d.x, a.y + d.y}; }
};

/// A chunk of the one map: the chunk holding cells [x * kChunkSide, (x + 1) * kChunkSide).
struct ChunkCoord {
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend constexpr bool operator==(ChunkCoord, ChunkCoord) noexcept = default;
    friend constexpr auto operator<=>(ChunkCoord, ChunkCoord) noexcept = default;
};

/// a / b rounded toward minus infinity, b > 0: cell -1 is in chunk -1, not chunk 0.
[[nodiscard]] constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

[[nodiscard]] constexpr ChunkCoord chunk_of(WorldCell cell) noexcept {
    return {floor_div(cell.x, kChunkSide), floor_div(cell.y, kChunkSide)};
}

/// The chunk's lowest cell (its top-left corner).
[[nodiscard]] constexpr WorldCell chunk_origin(ChunkCoord chunk) noexcept {
    return {chunk.x * kChunkSide, chunk.y * kChunkSide};
}

/// Salt for chunk seeds, so they never coincide with another per-cell hash of the same seed.
inline constexpr std::uint64_t kChunkSeedSalt = 0xC4C0ULL;

/// A chunk's own seed: what anything generated per chunk draws from.
[[nodiscard]] constexpr Seed chunk_seed(Seed seed, ChunkCoord chunk) noexcept {
    return hash_u64(seed ^ kChunkSeedSalt, static_cast<std::uint64_t>(chunk.x),
                    static_cast<std::uint64_t>(chunk.y));
}

} // namespace peo::core
