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
    const int w = front_.width();
    const int h = front_.height();
    const float share = params_.diffusion / 4.0F;
    const float keep = 1.0F - params_.decay;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Vec2i p{x, y};
            if (blocked && blocked->at(p)) {
                back_.at(p) = 0.0F;
                continue;
            }
            const float here = front_.at(p);
            // Scent that leaves `here` towards open neighbours, and scent that
            // arrives from them. Edges and walls reflect: scent that cannot leave stays.
            float out = 0.0F;
            float in = 0.0F;
            for (const Vec2i d : kNeighbours4) {
                const Vec2i n = p + d;
                if (!front_.in_bounds(n) || (blocked && blocked->at(n))) {
                    continue;
                }
                out += here * share;
                in += front_.at(n) * share;
            }
            float v = (here - out + in) * keep;
            if (v < params_.floor) {
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
