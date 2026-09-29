#include "peo/core/scent.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace peo::core {

ScentField::ScentField(int width, int height, ScentParams params)
    : params_(params), front_(width, height, 0.0F), back_(width, height, 0.0F) {}

void ScentField::deposit(Vec2i at, float amount) noexcept {
    if (front_.in_bounds(at)) {
        front_.at(at) += amount;
    }
}

void ScentField::clear() noexcept {
    front_.fill(0.0F);
    back_.fill(0.0F);
}

float ScentField::sample(Vec2i at) const noexcept {
    return front_.in_bounds(at) ? front_.at(at) : 0.0F;
}

float ScentField::total() const noexcept {
    return std::accumulate(front_.begin(), front_.end(), 0.0F);
}

void ScentField::step(const Grid<bool>* blocked) noexcept {
    sweep(blocked, true);
}

void ScentField::step_linear(const Grid<bool>* blocked) noexcept {
    sweep(blocked, false);
}

void ScentField::clamp_floor() noexcept {
    for (float& v : front_) {
        if (v < params_.floor) {
            v = 0.0F;
        }
    }
}

void ScentField::patch_deposit(Vec2i at, float amount, const Grid<bool>* blocked) noexcept {
    const auto open = [&](Vec2i p) { return front_.in_bounds(p) && !(blocked && blocked->at(p)); };
    if (!open(at)) {
        return; // step_linear zeroes walls, so a deposit there leaves no trace
    }
    // Each term is formed exactly as sweep() forms it: (source * factor) * keep.
    const float keep = 1.0F - params_.decay;
    const float stay = 1.0F - params_.diffusion;
    const float share = params_.diffusion / 4.0F;
    front_.at(at) += (amount * stay) * keep;
    for (const Vec2i d : kNeighbours4) {
        if (open(at + d)) {
            front_.at(at + d) += (amount * share) * keep;
        }
    }
}

void ScentField::sweep(const Grid<bool>* blocked, bool clamp) noexcept {
    const int w = front_.width();
    const int h = front_.height();
    const float share = params_.diffusion / 4.0F;
    const float keep = 1.0F - params_.decay;
    const float stay = 1.0F - params_.diffusion;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Vec2i p{x, y};
            if (blocked && blocked->at(p)) {
                back_.at(p) = 0.0F;
                continue;
            }
            // Every open cell sheds `diffusion` of its scent, split four ways, whatever
            // lies beyond. Only open in-bounds neighbours send any back, so the share
            // that goes into a wall or off the map is absorbed there and lost.
            float in = 0.0F;
            for (const Vec2i d : kNeighbours4) {
                const Vec2i n = p + d;
                if (!front_.in_bounds(n) || (blocked && blocked->at(n))) {
                    continue;
                }
                in += front_.at(n) * share;
            }
            float v = (front_.at(p) * stay + in) * keep;
            if (clamp && v < params_.floor) {
                v = 0.0F;
            }
            back_.at(p) = v;
        }
    }
    std::swap(front_, back_);
}

std::optional<Vec2i> ScentField::strongest_neighbour(Vec2i from, const Grid<bool>* blocked) const noexcept {
    float best = sample(from);
    std::optional<Vec2i> result;
    for (const Vec2i d : kNeighbours8) {
        const Vec2i n = from + d;
        if (!front_.in_bounds(n) || (blocked && blocked->at(n))) {
            continue;
        }
        const float v = front_.at(n);
        if (v > best) {
            best = v;
            result = n;
        }
    }
    return result;
}

} // namespace peo::core
