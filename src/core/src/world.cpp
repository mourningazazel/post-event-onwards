#include "peo/core/world.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace peo::core {

namespace {
/// Mixed into the stage seed so the horde's placement stream differs from the map's.
constexpr Seed kHordeSeedSalt = 0xABCDULL;
/// Range of the spawn draw: updates between a spawned Dead's steps. Stored in
/// seconds as draw * update_period, so the draw sequence is unchanged (D-015).
constexpr int kMinDeadSpeed = 1;
constexpr int kMaxDeadSpeed = 3;
/// The deposit patch touches the player's cell and its 4 neighbours; one of the Dead
/// reads the 3x3 around itself. So only Dead within 2 (Chebyshev) can see a change.
constexpr int kRepatchRadius = 2;
} // namespace

World::World(Seed seed, WorldParams params) : seed_(seed), params_(params) {
    params_.stage_width = std::max(params_.stage_width, kMinStageSide);
    params_.stage_height = std::max(params_.stage_height, kMinStageSide);
    params_.update_period = std::max<Seconds>(params_.update_period, 1);
    load_stage(0);
}

void World::load_stage(std::uint32_t index) {
    stage_index_ = index;
    StageSpec spec = stage_spec(seed_, index);
    spec.width = params_.stage_width;
    spec.height = params_.stage_height;
    stage_ = generate_stage(spec);
    scent_ = ScentField(stage_.spec.width, stage_.spec.height, params_.scent);
    player_ = stage_.entry;
    rng_.reseed(stage_seed(seed_, index) ^ kHordeSeedSalt);
    // Cap the spawn at the open cells a Dead may start on, so the placement loop
    // below always terminates. The draws for those placed are unchanged.
    int open = 0;
    for (int y = 1; y < stage_.spec.height - 1; ++y) {
        for (int x = 1; x < stage_.spec.width - 1; ++x) {
            open += !stage_.blocked.at(x, y) && Vec2i{x, y} != player_ ? 1 : 0;
        }
    }
    const int count = std::clamp(params_.initial_dead, 0, open);
    horde_.clear();
    horde_.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        Vec2i p;
        do {
            p = {rng_.range(1, stage_.spec.width - 2), rng_.range(1, stage_.spec.height - 2)};
        } while (stage_.blocked.at(p) || p == player_);
        horde_.push_back(
            {.pos = p,
             .cooldown_s = 0,
             .step_seconds = static_cast<std::uint16_t>(
                 static_cast<Seconds>(rng_.range(kMinDeadSpeed, kMaxDeadSpeed)) * params_.update_period)});
    }
    turn_ = 0;
}

void World::apply_action(Action action) noexcept {
    if (action.kind == ActionKind::Step) {
        const Vec2i next = player_ + action.dir;
        // A step into a wall (or off the map) becomes a wait: the turn is still spent.
        if (stage_.blocked.in_bounds(next) && !stage_.blocked.at(next)) {
            player_ = next;
        }
    }
}

void World::finish_turn() {
    ++turn_;
    if (player_ == stage_.exit) {
        load_stage(stage_index_ + 1);
    }
}

// step(), speculate() and commit() run the same float operations in the same
// order: step_linear, patch_deposit at the player, clamp_floor. That is what makes
// commit(speculate()) bit-identical to step(), not the algebra.
void World::step(Action action) {
    apply_action(action);
    scent_.step_linear(&stage_.blocked);
    scent_.patch_deposit(player_, params_.player_scent, &stage_.blocked);
    scent_.clamp_floor();
    step_horde(horde_, scent_, stage_.blocked, params_.update_period);
    finish_turn();
}

Speculation World::speculate() const {
    Speculation spec;
    speculate(spec);
    return spec;
}

void World::speculate(Speculation& out) const {
    out.scent = scent_; // copy-assign reuses out's storage once it is the right size
    out.horde = horde_;
    out.before = horde_;
    out.turn = turn_;
    out.stage_index = stage_index_;
    out.scent.step_linear(&stage_.blocked);
    // The Dead decide on the clamped field, as they will after commit; only the
    // cells the deposit patch touches can differ, and commit re-decides those.
    out.clamped = out.scent;
    out.clamped.clamp_floor();
    step_horde(out.horde, out.clamped, stage_.blocked, params_.update_period);
}

void World::commit(Speculation& spec, Action action) {
    if (spec.turn != turn_ || spec.stage_index != stage_index_ || spec.horde.size() != horde_.size()) {
        step(action);
        return;
    }
    apply_action(action);
    spec.scent.patch_deposit(player_, params_.player_scent, &stage_.blocked);
    spec.scent.clamp_floor();
    for (std::size_t i = 0; i < spec.before.size(); ++i) {
        const Vec2i d = spec.before[i].pos - player_;
        if (std::max(std::abs(d.x), std::abs(d.y)) <= kRepatchRadius) {
            Dead unit = spec.before[i];
            step_dead(unit, spec.scent, stage_.blocked, params_.update_period);
            spec.horde[i] = unit;
        }
    }
    std::swap(scent_, spec.scent); // swap, not move: spec keeps buffers to reuse
    std::swap(horde_, spec.horde);
    spec.turn = ~Tick{0}; // spent: a second commit falls back to step()
    finish_turn();
}

bool World::equivalent(const World& a, const World& b) noexcept {
    if (a.stage_index_ != b.stage_index_ || a.turn_ != b.turn_ || a.player_ != b.player_ ||
        a.horde_.size() != b.horde_.size() || a.scent_.cells().size() != b.scent_.cells().size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.horde_.size(); ++i) {
        const Dead& x = a.horde_[i];
        const Dead& y = b.horde_[i];
        if (x.pos != y.pos || x.cooldown_s != y.cooldown_s || x.step_seconds != y.step_seconds) {
            return false;
        }
    }
    const float* fa = a.scent_.cells().data();
    const float* fb = b.scent_.cells().data();
    for (std::size_t i = 0; i < a.scent_.cells().size(); ++i) {
        if (std::bit_cast<std::uint32_t>(fa[i]) != std::bit_cast<std::uint32_t>(fb[i])) {
            return false;
        }
    }
    return true;
}

} // namespace peo::core
