#include "peo/core/world.hpp"

namespace peo::core {

namespace {
/// Mixed into the stage seed so the horde's placement stream differs from the map's.
constexpr Seed kHordeSeedSalt = 0xABCDULL;
/// Speed range for spawned Dead (ticks between moves come from speed; see dead.hpp).
constexpr int kMinDeadSpeed = 1;
constexpr int kMaxDeadSpeed = 3;
} // namespace

World::World(Seed seed, WorldParams params) : seed_(seed), params_(params) {
    load_stage(0);
}

void World::load_stage(std::uint32_t index) {
    stage_index_ = index;
    stage_ = generate_stage(stage_spec(seed_, index));
    scent_ = ScentField(stage_.spec.width, stage_.spec.height, params_.scent);
    player_ = stage_.entry;
    rng_.reseed(stage_seed(seed_, index) ^ kHordeSeedSalt);
    horde_.clear();
    horde_.reserve(static_cast<std::size_t>(params_.initial_dead));
    for (int i = 0; i < params_.initial_dead; ++i) {
        Vec2i p;
        do {
            p = {rng_.range(1, stage_.spec.width - 2), rng_.range(1, stage_.spec.height - 2)};
        } while (stage_.blocked.at(p) || p == player_);
        horde_.push_back({.pos = p,
                          .cooldown = 0,
                          .speed = static_cast<std::uint8_t>(rng_.range(kMinDeadSpeed, kMaxDeadSpeed))});
    }
    turn_ = 0;
}

void World::step(Action action) {
    if (action.kind == ActionKind::Step) {
        const Vec2i next = player_ + action.dir;
        // A step into a wall (or off the map) becomes a wait: the turn is still spent.
        if (stage_.blocked.in_bounds(next) && !stage_.blocked.at(next)) {
            player_ = next;
        }
    }

    scent_.deposit(player_, params_.player_scent);
    scent_.step(&stage_.blocked);
    step_horde(horde_, scent_, stage_.blocked);
    ++turn_;

    if (player_ == stage_.exit) {
        load_stage(stage_index_ + 1);
    }
}

} // namespace peo::core
