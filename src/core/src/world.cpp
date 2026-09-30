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
/// Cap on the occupancy log's up-front reservation. At most update_period distinct
/// tiles fit in one update (each action spends at least a second on one tile).
constexpr Seconds kMaxLogReserve = 64;

/// A spent speculation's update number: never equal to a live World's.
constexpr Tick kSpentUpdate = ~Tick{0};

template <typename T> bool same_cells(const Grid<T>& a, const Grid<T>& b) noexcept {
    return a.width() == b.width() && a.height() == b.height() && std::equal(a.begin(), a.end(), b.begin());
}
} // namespace

World::World(Seed seed, WorldParams params) : seed_(seed), params_(params) {
    params_.stage_width = std::max(params_.stage_width, kMinStageSide);
    params_.stage_height = std::max(params_.stage_height, kMinStageSide);
    params_.update_period = std::max<Seconds>(params_.update_period, 1);
    params_.dead_cycle = std::max<Seconds>(params_.dead_cycle, 1);
    log_.reserve(std::min(params_.update_period, kMaxLogReserve));
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
    // One of the Dead per tile (D-031): an occupied draw is redrawn, so occupancy
    // starts valid. count <= open keeps the loop finite.
    occupied_ = Grid<std::uint8_t>(stage_.spec.width, stage_.spec.height, 0);
    for (int i = 0; i < count; ++i) {
        Vec2i p;
        do {
            p = {rng_.range(1, stage_.spec.width - 2), rng_.range(1, stage_.spec.height - 2)};
        } while (stage_.blocked.at(p) || p == player_ || occupied_.at(p) != 0);
        occupied_.at(p) = 1;
        horde_.push_back(
            {.pos = p,
             .step_seconds = static_cast<std::uint16_t>(
                 static_cast<Seconds>(rng_.range(kMinDeadSpeed, kMaxDeadSpeed)) * params_.update_period)});
    }
    turn_ = 0;
    seconds_ = 0;
    updates_ = 0;
    log_.clear();

    reserved_ = Grid<bool>(stage_.spec.width, stage_.spec.height, false);
    moving_.assign(horde_.size(), 0);
    plans_.resize(horde_.size());
    // At most one slot per unit per second of the cycle: reserve the bound once so
    // no poll allocates, however the hashes fall.
    slot_units_.reserve(horde_.size() * params_.dead_cycle);
    deciding_.clear();
    landing_.clear();
    deciding_.reserve(horde_.size());
    landing_.reserve(horde_.size());
    stage_salt_ = stage_seed(seed_, index);
    dead_second(0); // instant 0: the first poll and slot 0's decisions
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

void World::log_seconds(Seconds s) {
    for (Occupancy& o : log_) {
        if (o.tile == player_) {
            o.seconds += s;
            return;
        }
    }
    log_.push_back({.tile = player_, .seconds = s});
}

// Each logged tile gets player_scent * (seconds / period). A full period on one
// tile makes the factor exactly 1.0F, so walking deposits exactly player_scent,
// as before the clock existed.
void World::deposit_log(ScentField& field) const noexcept {
    const auto period = static_cast<float>(params_.update_period);
    for (const Occupancy& o : log_) {
        const float share = static_cast<float>(o.seconds) / period;
        field.patch_deposit(o.tile, params_.player_scent * share, &stage_.blocked);
    }
}

// step() and commit() run the same float operations in the same order:
// step_linear, patch_deposit per logged tile, clamp_floor. That is what makes
// commit(speculate()) bit-identical to step(), not the algebra.
void World::run_update() {
    scent_.step_linear(&stage_.blocked);
    deposit_log(scent_);
    scent_.clamp_floor();
    log_.clear();
    ++updates_;
}

void World::finish_from(Speculation& spec) {
    deposit_log(spec.scent);
    spec.scent.clamp_floor();
    std::swap(scent_, spec.scent); // swap, not move: spec keeps its buffer to reuse
    spec.update = kSpentUpdate;
    log_.clear();
    ++updates_;
}

void World::poll(std::uint64_t cycle) {
    const Seconds len = params_.dead_cycle;
    slot_begin_.assign(static_cast<std::size_t>(len) + 1, 0);
    for (std::size_t i = 0; i < horde_.size(); ++i) {
        // Only a unit with somewhere better to go this cycle gets slots.
        plans_[i] = scent_.strongest_neighbour(horde_[i].pos, &stage_.blocked)
                        ? plan_slots(stage_salt_, cycle, i, horde_[i].step_seconds, len)
                        : SlotPlan{};
        for (Seconds j = 0; j < plans_[i].count; ++j) {
            ++slot_begin_[slot_second(plans_[i], j, len) + 1];
        }
    }
    for (Seconds s = 0; s < len; ++s) {
        slot_begin_[s + 1] += slot_begin_[s];
    }
    slot_units_.resize(slot_begin_[len]);
    // Fill each bucket in ascending unit order; slot_begin_[s] walks up to the
    // bucket's end as it fills, and is walked back after.
    for (std::size_t i = 0; i < horde_.size(); ++i) {
        for (Seconds j = 0; j < plans_[i].count; ++j) {
            slot_units_[slot_begin_[slot_second(plans_[i], j, len)]++] = static_cast<std::uint32_t>(i);
        }
    }
    for (Seconds s = len; s > 0; --s) {
        slot_begin_[s] = slot_begin_[s - 1];
    }
    slot_begin_[0] = 0;
}

// D-031's order within one second t: the slot's units decide first, so a unit
// that decided at t-1 still stands on its old tile while they look; then the
// moves decided at t-1 land. A vacated tile is therefore free only from t+1.
void World::dead_second(Seconds t) {
    const Seconds len = params_.dead_cycle;
    if (t % len == 0) {
        poll(t / len);
    }
    const Seconds s = t % len;
    for (std::uint32_t k = slot_begin_[s]; k < slot_begin_[s + 1]; ++k) {
        const std::uint32_t u = slot_units_[k];
        if (moving_[u] != 0) {
            continue;
        }
        if (const auto to = decide_move(horde_[u], scent_, stage_.blocked, occupied_, reserved_)) {
            reserved_.at(*to) = true;
            moving_[u] = 1;
            deciding_.push_back({.unit = u, .to = *to});
        }
    }
    for (const Move& m : landing_) {
        Dead& unit = horde_[m.unit];
        --occupied_.at(unit.pos);
        unit.pos = m.to;
        ++occupied_.at(m.to);
        reserved_.at(m.to) = false;
        moving_[m.unit] = 0;
    }
    landing_.clear();
    std::swap(landing_, deciding_);
}

void World::advance(Seconds duration, Speculation* spec) {
    for (Seconds left = std::max<Seconds>(duration, 1); left > 0; --left) {
        log_seconds(1);
        ++seconds_;
        dead_second(seconds_);
        if (seconds_ % params_.update_period == 0) {
            if (spec != nullptr) {
                finish_from(*spec);
                spec = nullptr;
            } else {
                run_update();
            }
        }
    }
}

void World::step(Action action) {
    apply_action(action);
    advance(action.seconds, nullptr);
    finish_turn();
}

Speculation World::speculate() const {
    Speculation spec;
    speculate(spec);
    return spec;
}

void World::speculate(Speculation& out) const {
    out.scent = scent_; // copy-assign reuses out's storage once it is the right size
    out.update = updates_;
    out.stage_index = stage_index_;
    out.scent.step_linear(&stage_.blocked);
}

void World::commit(Speculation& spec, Action action) {
    if (spec.update != updates_ || spec.stage_index != stage_index_) {
        step(action);
        return;
    }
    apply_action(action);
    advance(action.seconds, &spec);
    finish_turn();
}

bool World::equivalent(const World& a, const World& b) noexcept {
    if (a.stage_index_ != b.stage_index_ || a.turn_ != b.turn_ || a.seconds_ != b.seconds_ ||
        a.updates_ != b.updates_ || a.player_ != b.player_ || a.log_.size() != b.log_.size() ||
        a.horde_.size() != b.horde_.size() || a.scent_.cells().size() != b.scent_.cells().size() ||
        a.landing_.size() != b.landing_.size() || a.moving_ != b.moving_ || a.slot_begin_ != b.slot_begin_ ||
        a.slot_units_ != b.slot_units_ || !same_cells(a.occupied_, b.occupied_) ||
        !same_cells(a.reserved_, b.reserved_)) {
        return false;
    }
    for (std::size_t i = 0; i < a.log_.size(); ++i) {
        if (a.log_[i].tile != b.log_[i].tile || a.log_[i].seconds != b.log_[i].seconds) {
            return false;
        }
    }
    for (std::size_t i = 0; i < a.landing_.size(); ++i) {
        if (a.landing_[i].unit != b.landing_[i].unit || a.landing_[i].to != b.landing_[i].to) {
            return false;
        }
    }
    for (std::size_t i = 0; i < a.horde_.size(); ++i) {
        if (a.horde_[i].pos != b.horde_[i].pos || a.horde_[i].step_seconds != b.horde_[i].step_seconds) {
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
