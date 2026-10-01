#include "peo/core/scent_wave.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>

namespace peo::core {

namespace {
/// Stored value of a cell nothing has reached: far below any age line, and far enough
/// above the type's minimum that subtracting a distance cost cannot overflow.
constexpr std::int32_t kUnreached = std::numeric_limits<std::int32_t>::min() / 2;

/// kNeighbours8 alternates orthogonal and diagonal (N, NE, E, SE, S, SW, W, NW): the
/// diagonal 2k+1 lies between orthogonals k and k+1 of kNeighbours4. Diagonal mask bit
/// k is kNeighbours8[2k+1]: NE, SE, SW, NW.
constexpr std::size_t kOrthogonals = 4;
constexpr std::uint8_t kNE = 1;
constexpr std::uint8_t kSE = 2;
constexpr std::uint8_t kSW = 4;
constexpr std::uint8_t kNW = 8;
/// A direction byte meaning "no neighbour is stronger" (indices 0-7 are kNeighbours8's).
constexpr std::uint8_t kNoFlow = 0xFF;
constexpr std::size_t kDirections = std::size(kNeighbours8);

#if defined(__x86_64__) && defined(__GNUC__)
// AVX2 has the 32-bit integer max the kernel needs; plain x86-64 (SSE2) does not, so
// pick at load time. arm64 has NEON as standard.
#define PEO_WAVE_KERNEL_CLONES __attribute__((target_clones("avx2", "default")))
#else
#define PEO_WAVE_KERNEL_CLONES
#endif

/// The pull for one row segment [begin, end) of interior cells (never on the map's
/// border, so all eight neighbours exist): branch-free so it vectorises. Returns
/// whether any cell changed.
PEO_WAVE_KERNEL_CLONES bool pull_row(const std::int32_t* __restrict a, std::int32_t* __restrict b,
                                     const std::uint8_t* __restrict open, const std::uint8_t* __restrict diag,
                                     std::ptrdiff_t w, std::ptrdiff_t begin, std::ptrdiff_t end,
                                     std::int32_t cost, std::int32_t line) {
    int changed = 0;
    for (std::ptrdiff_t i = begin; i < end; ++i) {
        // Every load is unconditional (interior cells have all eight neighbours) and the
        // masks only select, so the loop has no branches and vectorises.
        const std::int32_t m = diag[i];
        const std::int32_t ne = a[i - w + 1];
        const std::int32_t se = a[i + w + 1];
        const std::int32_t sw = a[i + w - 1];
        const std::int32_t nw = a[i - w - 1];
        std::int32_t best = std::max(std::max(a[i - w], a[i + 1]), std::max(a[i + w], a[i - 1]));
        best = std::max(best, (m & kNE) != 0 ? ne : kUnreached);
        best = std::max(best, (m & kSE) != 0 ? se : kUnreached);
        best = std::max(best, (m & kSW) != 0 ? sw : kUnreached);
        best = std::max(best, (m & kNW) != 0 ? nw : kUnreached);
        const std::int32_t candidate = best - cost;
        const std::int32_t old = a[i];
        const bool take = (open[i] != 0) & (candidate > line) & (candidate > old);
        const std::int32_t v = take ? candidate : old;
        b[i] = v;
        changed |= static_cast<int>(take);
    }
    return changed != 0;
}

/// The direction bytes for one row segment [begin, end) of interior cells (PEO-079): the
/// first neighbour in kNeighbours8 order holding the strict maximum stored value above
/// the cell's own, among those a step may reach (open; a diagonal also needs its corner
/// mask bit). Stored values keep the order sample() gives everything above the age line,
/// so with flow_target()'s check of the target against the line this is
/// strongest_neighbour. Branch-free, as pull_row: loads are unconditional, the masks are
/// integers that only select (a bool & bool here keeps GCC from vectorising).
PEO_WAVE_KERNEL_CLONES void flow_row(const std::int32_t* __restrict a, std::uint8_t* __restrict flow,
                                     const std::uint8_t* __restrict open, const std::uint8_t* __restrict diag,
                                     std::ptrdiff_t w, std::ptrdiff_t begin, std::ptrdiff_t end) {
    std::array<std::ptrdiff_t, kDirections> offset{};
    for (std::size_t d = 0; d < kDirections; ++d) {
        offset[d] = kNeighbours8[d].y * w + kNeighbours8[d].x;
    }
    for (std::ptrdiff_t i = begin; i < end; ++i) {
        const std::int32_t m = diag[i];
        std::int32_t best = a[i];
        std::int32_t to = kNoFlow;
        for (std::size_t d = 0; d < kDirections; ++d) {
            const std::ptrdiff_t n = i + offset[d];
            const std::int32_t stored = a[n];
            const std::int32_t corner = d % 2 == 0 ? 1 : m >> (d / 2); // diagonal d is mask bit d/2
            const std::int32_t reach = static_cast<std::int32_t>(open[n]) & corner & 1;
            const std::int32_t v = reach != 0 ? stored : kUnreached;
            to = v > best ? static_cast<std::int32_t>(d) : to;
            best = std::max(best, v);
        }
        flow[i] = static_cast<std::uint8_t>(to);
    }
}
} // namespace

ScentWave::ScentWave(int width, int height, WaveParams params)
    : params_(params), width_(width), height_(height),
      tiles_x_((width + kWaveTileWidth - 1) / kWaveTileWidth),
      tiles_y_((height + kWaveTileHeight - 1) / kWaveTileHeight) {
    const std::size_t cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t tiles = static_cast<std::size_t>(tiles_x_) * static_cast<std::size_t>(tiles_y_);
    buf_[0].assign(cells, kUnreached);
    buf_[1].assign(cells, kUnreached);
    open_.assign(cells, 0);
    diag_.assign(cells, 0);
    flow_.assign(cells, kNoFlow);
    stale_mark_.assign(tiles, 0);
    stale_.reserve(tiles);
    changed_.assign(tiles, 0);
    mark_.assign(tiles, 0);
    written_mark_.assign(tiles, 0);
    active_.reserve(tiles); // bounds, once: no allocation per update
    written_.reserve(tiles);
}

bool ScentWave::open_at(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < width_ && y < height_ && open_[index({x, y})] != 0;
}

void ScentWave::build_masks(const Grid<bool>& blocked) {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            open_[index({x, y})] = blocked.at(x, y) ? 0 : 1;
        }
    }
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            std::uint8_t m = 0;
            for (std::size_t k = 0; k < kOrthogonals; ++k) {
                const Vec2i d = kNeighbours8[2 * k + 1];
                // The step is symmetric: the same two cells lie beside it from either end.
                if (open_at(x + d.x, y) && open_at(x, y + d.y)) {
                    m = static_cast<std::uint8_t>(m | (1U << k));
                }
            }
            diag_[index({x, y})] = m;
        }
    }
    masks_built_ = true;
}

void ScentWave::touched(std::size_t tile) noexcept {
    changed_[tile] = 1;
    if (written_mark_[tile] == 0) {
        written_mark_[tile] = 1;
        written_.push_back(static_cast<std::uint32_t>(tile));
    }
}

void ScentWave::deposit(Vec2i at, std::int32_t strength) noexcept {
    std::int32_t& cell = buf_[cur_][index(at)];
    const std::int32_t v = age_line() + strength;
    if (v > cell) {
        cell = v;
        touched(tile_of(at.x, at.y));
        if (masks_built_) { // the bytes that read this cell: it and its eight neighbours
            constexpr int kDepositReach = 1;
            refresh_flow(at.x - kDepositReach, at.y - kDepositReach, at.x + kDepositReach + 1,
                         at.y + kDepositReach + 1);
        }
    }
}

void ScentWave::expand_active() {
    std::fill(mark_.begin(), mark_.end(), 0);
    for (int ty = 0; ty < tiles_y_; ++ty) {
        for (int tx = 0; tx < tiles_x_; ++tx) {
            if (changed_[static_cast<std::size_t>(ty) * static_cast<std::size_t>(tiles_x_) +
                         static_cast<std::size_t>(tx)] == 0) {
                continue;
            }
            for (int y = std::max(ty - 1, 0); y <= std::min(ty + 1, tiles_y_ - 1); ++y) {
                for (int x = std::max(tx - 1, 0); x <= std::min(tx + 1, tiles_x_ - 1); ++x) {
                    mark_[static_cast<std::size_t>(y) * static_cast<std::size_t>(tiles_x_) +
                          static_cast<std::size_t>(x)] = 1;
                }
            }
        }
    }
    active_.clear();
    for (std::size_t t = 0; t < mark_.size(); ++t) {
        if (mark_[t] != 0) {
            active_.push_back(static_cast<std::uint32_t>(t));
        }
    }
}

bool ScentWave::pull_border_cell(int x, int y) {
    const std::vector<std::int32_t>& a = buf_[cur_];
    std::vector<std::int32_t>& b = buf_[cur_ ^ 1U];
    const std::size_t i = index({x, y});
    const std::int32_t old = a[i];
    if (open_[i] == 0) {
        b[i] = old;
        return false;
    }
    std::int32_t best = kUnreached;
    for (const Vec2i d : kNeighbours4) {
        const int nx = x + d.x;
        const int ny = y + d.y;
        if (nx >= 0 && ny >= 0 && nx < width_ && ny < height_) {
            best = std::max(best, a[index({nx, ny})]);
        }
    }
    for (std::size_t k = 0; k < kOrthogonals; ++k) {
        if ((diag_[i] & (1U << k)) != 0) { // both sides open and on the map, so the corner is
            best = std::max(best, a[index(Vec2i{x, y} + kNeighbours8[2 * k + 1])]);
        }
    }
    const std::int32_t candidate = best - params_.distance_cost;
    const std::int32_t v = (candidate > age_line() && candidate > old) ? candidate : old;
    b[i] = v;
    return v != old;
}

bool ScentWave::pull_tile(std::size_t tile) {
    const int x0 = static_cast<int>(tile % static_cast<std::size_t>(tiles_x_)) * kWaveTileWidth;
    const int y0 = static_cast<int>(tile / static_cast<std::size_t>(tiles_x_)) * kWaveTileHeight;
    const int x1 = std::min(x0 + kWaveTileWidth, width_);
    const int y1 = std::min(y0 + kWaveTileHeight, height_);
    const std::int32_t* a = buf_[cur_].data();
    std::int32_t* b = buf_[cur_ ^ 1U].data();
    const auto w = static_cast<std::ptrdiff_t>(width_);
    bool changed = false;
    for (int y = y0; y < y1; ++y) {
        if (y == 0 || y == height_ - 1) {
            for (int x = x0; x < x1; ++x) {
                changed = pull_border_cell(x, y) || changed;
            }
            continue;
        }
        const int in0 = std::max(x0, 1);
        const int in1 = std::min(x1, width_ - 1);
        if (x0 == 0) {
            changed = pull_border_cell(0, y) || changed;
        }
        if (in0 < in1) {
            changed = pull_row(a, b, open_.data(), diag_.data(), w, y * w + in0, y * w + in1,
                               params_.distance_cost, age_line()) ||
                      changed;
        }
        if (x1 == width_ && width_ > 1) {
            changed = pull_border_cell(width_ - 1, y) || changed;
        }
    }
    return changed;
}

void ScentWave::round() {
    expand_active();
    std::fill(changed_.begin(), changed_.end(), 0);
    for (const std::uint32_t t : active_) {
        if (pull_tile(t)) {
            touched(t);
            if (stale_mark_[t] == 0) { // its bytes are rewritten once the rounds are done
                stale_mark_[t] = 1;
                stale_.push_back(t);
            }
        }
    }
    cur_ ^= 1U;
#ifndef NDEBUG
    // A tile nobody pulled holds the same values in both buffers (PEO-078's invariant).
    for (std::size_t t = 0; t < mark_.size(); ++t) {
        if (mark_[t] != 0) {
            continue;
        }
        const int x0 = static_cast<int>(t % static_cast<std::size_t>(tiles_x_)) * kWaveTileWidth;
        const int y0 = static_cast<int>(t / static_cast<std::size_t>(tiles_x_)) * kWaveTileHeight;
        for (int y = y0; y < std::min(y0 + kWaveTileHeight, height_); ++y) {
            for (int x = x0; x < std::min(x0 + kWaveTileWidth, width_); ++x) {
                assert(buf_[0][index({x, y})] == buf_[1][index({x, y})]);
            }
        }
    }
#endif
}

void ScentWave::refresh_flow_cell(int x, int y) noexcept {
    const std::size_t i = index({x, y});
    std::int32_t best = values()[i];
    std::uint8_t to = kNoFlow;
    for (std::size_t d = 0; d < kDirections; ++d) {
        const Vec2i n = Vec2i{x, y} + kNeighbours8[d];
        if (!open_at(n.x, n.y) || (d % 2 == 1 && (diag_[i] & (1U << (d / 2))) == 0)) {
            continue;
        }
        const std::int32_t v = values()[index(n)];
        if (v > best) {
            best = v;
            to = static_cast<std::uint8_t>(d);
        }
    }
    flow_[i] = to;
}

void ScentWave::refresh_flow(int x0, int y0, int x1, int y1) noexcept {
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, width_);
    y1 = std::min(y1, height_);
    const auto w = static_cast<std::ptrdiff_t>(width_);
    for (int y = y0; y < y1; ++y) {
        if (y == 0 || y == height_ - 1) {
            for (int x = x0; x < x1; ++x) {
                refresh_flow_cell(x, y);
            }
            continue;
        }
        const int in0 = std::max(x0, 1);
        const int in1 = std::min(x1, width_ - 1);
        if (x0 == 0) {
            refresh_flow_cell(0, y);
        }
        if (in0 < in1) {
            flow_row(values().data(), flow_.data(), open_.data(), diag_.data(), w, y * w + in0, y * w + in1);
        }
        if (x1 == width_ && width_ > 1) {
            refresh_flow_cell(width_ - 1, y);
        }
    }
}

std::array<int, 4> ScentWave::flow_reach(std::size_t tile) const noexcept {
    // A byte reads its eight neighbours, so a change on a tile's edge moves the bytes
    // one cell into the next tile.
    const int x0 = static_cast<int>(tile % static_cast<std::size_t>(tiles_x_)) * kWaveTileWidth;
    const int y0 = static_cast<int>(tile / static_cast<std::size_t>(tiles_x_)) * kWaveTileHeight;
    return {std::max(x0 - 1, 0), std::max(y0 - 1, 0), std::min(x0 + kWaveTileWidth + 1, width_),
            std::min(y0 + kWaveTileHeight + 1, height_)};
}

void ScentWave::update(const Grid<bool>& blocked) {
    const bool first = !masks_built_;
    if (first) {
        build_masks(blocked);
    }
    ++updates_;
    for (int r = 0; r < params_.speed; ++r) {
        round();
    }
    if (first) { // bytes written before the masks existed saw no open neighbour
        refresh_flow(0, 0, width_, height_);
    }
    for (const std::uint32_t t : stale_) {
        if (!first) {
            const auto [x0, y0, x1, y1] = flow_reach(t);
            refresh_flow(x0, y0, x1, y1);
        }
        stale_mark_[t] = 0;
    }
    stale_.clear();
}

void ScentWave::patch_deposit(const ScentWave& before, Vec2i at, std::int32_t strength,
                              const Grid<bool>& blocked) {
    if (!masks_built_) {
        build_masks(blocked);
    }
    // As deposit() would have made it, before the update: only a deposit that beat
    // the cell's value then took part in the update's round.
    const std::int32_t v = before.age_line() + strength;
    if (v <= before.values()[index(at)]) {
        return;
    }
    std::vector<std::int32_t>& cur = buf_[cur_];
    std::int32_t& cell = cur[index(at)];
    cell = std::max(cell, v);
    touched(tile_of(at.x, at.y)); // the other buffer is now behind here: pull it next round
    // The cells raised are the deposit and its eight neighbours, so the bytes that can
    // move are the 5x5 block round it.
    constexpr int kPatchReach = 2;
    const std::int32_t candidate = v - params_.distance_cost;
    if (candidate > age_line()) {
        for (std::size_t k = 0; k < std::size(kNeighbours8); ++k) {
            const Vec2i d = kNeighbours8[k];
            const Vec2i n = at + d;
            const bool diagonal = d.x != 0 && d.y != 0;
            if (!open_at(n.x, n.y) ||
                (diagonal && !(open_at(at.x + d.x, at.y) && open_at(at.x, at.y + d.y)))) {
                continue;
            }
            std::int32_t& target = cur[index(n)];
            if (candidate > target) {
                target = candidate;
                touched(tile_of(n.x, n.y));
            }
        }
    }
    refresh_flow(at.x - kPatchReach, at.y - kPatchReach, at.x + kPatchReach + 1, at.y + kPatchReach + 1);
}

std::size_t ScentWave::sync_from(const ScentWave& source) {
    if (this == &source) {
        return 0;
    }
    const bool partners = partner_token_ != 0 && partner_token_ == source.partner_token_ &&
                          width_ == source.width_ && height_ == source.height_;
    if (!partners) {
        *this = source; // copy-assign reuses this wave's storage once it is the right size
        for (const std::uint32_t t : written_) {
            written_mark_[t] = 0;
        }
        written_.clear();
        return changed_.size();
    }
    const bool masks_arrived = !masks_built_ && source.masks_built_;
    if (masks_arrived) {      // the same stage's masks, built on one side only
        open_ = source.open_; // same size: no allocation
        diag_ = source.diag_;
        masks_built_ = true;
    }
    // Tiles either side wrote since this wave last matched: copy those into both of this
    // wave's buffers. Everywhere else the two waves, and this wave's buffers, are equal.
    std::fill(mark_.begin(), mark_.end(), 0);
    std::size_t copied = 0;
    const auto copy_tile = [&](std::uint32_t t) {
        if (mark_[t] != 0) {
            return;
        }
        mark_[t] = 1;
        ++copied;
        const int x0 = static_cast<int>(t % static_cast<std::size_t>(tiles_x_)) * kWaveTileWidth;
        const int y0 = static_cast<int>(t / static_cast<std::size_t>(tiles_x_)) * kWaveTileHeight;
        const int x1 = std::min(x0 + kWaveTileWidth, width_);
        for (int y = y0; y < std::min(y0 + kWaveTileHeight, height_); ++y) {
            const auto from = source.values().begin() + static_cast<std::ptrdiff_t>(index({x0, y}));
            std::copy(from, from + (x1 - x0), buf_[0].begin() + static_cast<std::ptrdiff_t>(index({x0, y})));
            std::copy(from, from + (x1 - x0), buf_[1].begin() + static_cast<std::ptrdiff_t>(index({x0, y})));
        }
    };
    for (const std::uint32_t t : source.written_) {
        copy_tile(t);
    }
    for (const std::uint32_t t : written_) {
        copy_tile(t);
        written_mark_[t] = 0;
    }
    written_.clear();
    changed_ = source.changed_; // same size: no allocation
    updates_ = source.updates_;
    params_ = source.params_;
    // The values now equal the source's everywhere, so a byte can differ only where a
    // copied value is read: the copied tiles and their rings. A source with its masks
    // has exact bytes there to bring over; one without has none, so they are rewritten.
    if (!masks_built_) {
        return copied; // neither side has seen the stage: no bytes yet
    }
    if (masks_arrived) {
        flow_ = source.flow_; // same size: no allocation
        return copied;
    }
    for (std::size_t t = 0; t < mark_.size(); ++t) {
        if (mark_[t] == 0) {
            continue;
        }
        const auto [x0, y0, x1, y1] = flow_reach(t);
        if (!source.masks_built_) {
            refresh_flow(x0, y0, x1, y1);
            continue;
        }
        for (int y = y0; y < y1; ++y) {
            const auto from = source.flow_.begin() + static_cast<std::ptrdiff_t>(index({x0, y}));
            std::copy(from, from + (x1 - x0), flow_.begin() + static_cast<std::ptrdiff_t>(index({x0, y})));
        }
    }
    return copied;
}

std::size_t ScentWave::active_cells() const noexcept {
    std::size_t cells = 0;
    for (int ty = 0; ty < tiles_y_; ++ty) {
        for (int tx = 0; tx < tiles_x_; ++tx) {
            bool active = false;
            for (int y = std::max(ty - 1, 0); y <= std::min(ty + 1, tiles_y_ - 1) && !active; ++y) {
                for (int x = std::max(tx - 1, 0); x <= std::min(tx + 1, tiles_x_ - 1) && !active; ++x) {
                    active = changed_[static_cast<std::size_t>(y) * static_cast<std::size_t>(tiles_x_) +
                                      static_cast<std::size_t>(x)] != 0;
                }
            }
            if (active) {
                cells += static_cast<std::size_t>(std::min(kWaveTileWidth, width_ - tx * kWaveTileWidth)) *
                         static_cast<std::size_t>(std::min(kWaveTileHeight, height_ - ty * kWaveTileHeight));
            }
        }
    }
    return cells;
}

std::int32_t ScentWave::sample(Vec2i at) const noexcept {
    return std::max(values()[index(at)] - age_line(), 0);
}

std::optional<Vec2i> ScentWave::strongest_neighbour(Vec2i from, const Grid<bool>* blocked) const noexcept {
    std::int32_t best = sample(from);
    std::optional<Vec2i> result;
    const auto open = [&](Vec2i c) {
        return c.x >= 0 && c.y >= 0 && c.x < width_ && c.y < height_ && !(blocked && blocked->at(c));
    };
    std::array<bool, kOrthogonals> side{};
    for (std::size_t k = 0; k < kOrthogonals; ++k) {
        side[k] = open(from + kNeighbours4[k]);
    }
    for (std::size_t d = 0; d < std::size(kNeighbours8); ++d) {
        const std::size_t k = d / 2;
        const Vec2i n = from + kNeighbours8[d];
        if (d % 2 == 0 ? !side[k]
                       : !(side[k] && side[(k + 1) % kOrthogonals]) || (blocked && blocked->at(n))) {
            continue;
        }
        const std::int32_t v = sample(n);
        if (v > best) {
            best = v;
            result = n;
        }
    }
    return result;
}

std::optional<Vec2i> ScentWave::flow_target(Vec2i from) const noexcept {
    const std::uint8_t d = flow_[index(from)];
    if (d == kNoFlow) {
        return std::nullopt;
    }
    const Vec2i to = from + kNeighbours8[d];
    if (values()[index(to)] <= age_line()) { // samples 0: no stronger than any cell
        return std::nullopt;
    }
    return to;
}

} // namespace peo::core
