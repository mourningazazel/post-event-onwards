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
/// And so the wind's two draws come from a stream of their own: drawing them changes
/// nothing else, so a calm world is the world it was before wind (PEO-048).
constexpr Seed kWindSeedSalt = 0x5EEDULL;
/// And so the Dead's draws never reuse a word of plan_slots' (PEO-009).
constexpr std::uint64_t kDrawSalt = 0xD8A3ULL;
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

bool same_moves(const std::vector<DeadMove>& a, const std::vector<DeadMove>& b) noexcept {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(),
                      [](const DeadMove& x, const DeadMove& y) { return x.unit == y.unit && x.to == y.to; });
}

/// Give every unit with a stronger neighbour its slots for cycle `cycle` (D-031),
/// bucketed by second in ascending unit order.
void poll(HordeState& d, std::uint64_t salt, std::uint64_t cycle, Seconds len) {
    d.slot_begin.assign(static_cast<std::size_t>(len) + 1, 0);
    for (std::size_t i = 0; i < d.horde.size(); ++i) {
        // Every unit draws at its slots (PEO-009): one with nowhere better to go
        // wanders or stays, by the draw.
        d.plans[i] = plan_slots(salt, cycle, i, d.horde[i].step_seconds, len);
        for (Seconds j = 0; j < d.plans[i].count; ++j) {
            ++d.slot_begin[slot_second(d.plans[i], j, len) + 1];
        }
    }
    for (Seconds s = 0; s < len; ++s) {
        d.slot_begin[s + 1] += d.slot_begin[s];
    }
    d.slot_units.resize(d.slot_begin[len]);
    // Fill each bucket in ascending unit order; slot_begin[s] walks up to the
    // bucket's end as it fills, and is walked back after.
    for (std::size_t i = 0; i < d.horde.size(); ++i) {
        for (Seconds j = 0; j < d.plans[i].count; ++j) {
            d.slot_units[d.slot_begin[slot_second(d.plans[i], j, len)]++] = static_cast<std::uint32_t>(i);
        }
    }
    for (Seconds s = len; s > 0; --s) {
        d.slot_begin[s] = d.slot_begin[s - 1];
    }
    d.slot_begin[0] = 0;
}

/// A decided move: reserve its tile and mark the unit moving until it lands.
void decided(HordeState& d, DeadMove m) {
    d.reserved.at(m.to) = true;
    d.moving[m.unit] = 1;
    d.deciding.push_back(m);
}

/// Last second's moves land; this second's become next second's landings.
void land(HordeState& d) {
    for (const DeadMove& m : d.landing) {
        Dead& unit = d.horde[m.unit];
        --d.occupied.at(unit.pos);
        unit.pos = m.to;
        ++d.occupied.at(m.to);
        d.reserved.at(m.to) = false;
        d.moving[m.unit] = 0;
    }
    d.landing.clear();
    std::swap(d.landing, d.deciding);
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
    scent_ = ScentWave(stage_.spec.width, stage_.spec.height, params_.scent);
    scent_.set_token(ScentWave::new_token());
    Rng wind_rng(stage_seed(seed_, index) ^ kWindSeedSalt);
    wind_.toward_degrees = wind_rng.range(0, kDegreesPerTurn - 1);
    wind_.intensity = wind_rng.range(0, std::clamp(params_.wind_max, 0, kMaxWindStep));
    scent_.set_wind(wind_);
    scent_.set_executor(executor_);
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
    HordeState& d = dead_;
    d.horde.clear();
    d.horde.reserve(static_cast<std::size_t>(count));
    // One of the Dead per tile (D-031): an occupied draw is redrawn, so occupancy
    // starts valid. count <= open keeps the loop finite.
    d.occupied = Grid<std::uint8_t>(stage_.spec.width, stage_.spec.height, 0);
    for (int i = 0; i < count; ++i) {
        Vec2i p;
        do {
            p = {rng_.range(1, stage_.spec.width - 2), rng_.range(1, stage_.spec.height - 2)};
        } while (stage_.blocked.at(p) || p == player_ || d.occupied.at(p) != 0);
        d.occupied.at(p) = 1;
        d.horde.push_back(
            {.pos = p,
             .step_seconds = static_cast<std::uint16_t>(
                 static_cast<Seconds>(rng_.range(kMinDeadSpeed, kMaxDeadSpeed)) * params_.update_period)});
    }
    turn_ = 0;
    seconds_ = 0;
    updates_ = 0;
    log_.clear();

    d.reserved = Grid<bool>(stage_.spec.width, stage_.spec.height, false);
    d.moving.assign(d.horde.size(), 0);
    d.plans.resize(d.horde.size());
    // At most one slot per unit per second of the cycle: reserve the bound once so
    // no poll allocates, however the hashes fall.
    d.slot_units.reserve(d.horde.size() * params_.dead_cycle);
    d.deciding.clear();
    d.landing.clear();
    d.deciding.reserve(d.horde.size());
    d.landing.reserve(d.horde.size());
    d.intents.reserve(d.horde.size());
    stage_salt_ = stage_seed(seed_, index);
    desire_ = DesireField(stage_.spec.width, stage_.spec.height);
    desire_.build(scent_, stage_.blocked, dead_.occupied, params_.draw, executor_);
    dead_second(dead_, 0, nullptr); // instant 0: the first poll and slot 0's decisions
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

// A tile held for the whole period deposits strength; one held for part of it
// deposits as if the scent were that much older: strength less age_cost x the
// share of the period it was empty, never more (a runner's tiles read a little
// less than a walker's, D-015). Integer, so the port is exact.
std::int32_t World::logged_strength(Seconds seconds) const noexcept {
    const auto period = static_cast<std::int32_t>(params_.update_period);
    const auto held = static_cast<std::int32_t>(std::min(seconds, params_.update_period));
    return params_.scent.strength - params_.scent.age_cost * (period - held) / period;
}

void World::deposit_log(ScentWave& field) const noexcept {
    for (const Occupancy& o : log_) {
        field.deposit(o.tile, logged_strength(o.seconds));
    }
}

void World::run_update() {
    deposit_log(scent_);
    scent_.update(stage_.blocked, &stage_.openness);
    desire_.build(scent_, stage_.blocked, dead_.occupied, params_.draw, executor_);
    log_.clear();
    ++updates_;
}

// The speculated update ran with no deposit; the logged tiles' deposits are added
// after it, which ScentWave makes bit-identical to depositing first (PEO-030).
void World::finish_from(Speculation& spec) {
    // A deposit raises cells within speed + gust of it, so the desire it can move is
    // that block and the ring round it (PEO-009).
    const int reach = params_.scent.speed + std::max(params_.scent.gust, 0) + 1;
    const bool desire_ready = spec.to == seconds_; // the Dead's seconds reached the boundary
    for (const Occupancy& o : log_) {
        spec.scent.patch_deposit(scent_, o.tile, logged_strength(o.seconds), stage_.blocked,
                                 &stage_.openness);
        if (desire_ready) {
            spec.desire.rebuild(spec.scent, stage_.blocked, params_.draw, o.tile.x - reach, o.tile.y - reach,
                                o.tile.x + reach + 1, o.tile.y + reach + 1);
        }
    }
    std::swap(scent_, spec.scent); // swap, not move: spec keeps its buffers to reuse
    if (desire_ready) {
        std::swap(desire_, spec.desire);
    } else { // a second poll stopped the speculation early: the snapshot is now
        desire_.build(scent_, stage_.blocked, dead_.occupied, params_.draw, executor_);
    }
    spec.update = kSpentUpdate;
    log_.clear();
    ++updates_;
}

// D-031's order within one second t: the slot's units decide first, so a unit
// that decided at t-1 still stands on its old tile while they look; then the
// moves decided at t-1 land. A vacated tile is therefore free only from t+1.
void World::dead_second(HordeState& d, Seconds t, Speculation* record) const {
    const Seconds len = params_.dead_cycle;
    if (t % len == 0) {
        poll(d, stage_salt_, t / len, len);
        if (record != nullptr) {
            record->poll_at = t;
            record->poll_begin = d.slot_begin; // copy-assign reuses capacity
            record->poll_units = d.slot_units;
        }
    }
    const Seconds s = t % len;
    const std::uint32_t first = d.slot_begin[s];
    const std::uint32_t batch = d.slot_begin[s + 1] - first;
    if (executor_ != nullptr && executor_->width() > 1 && batch >= params_.parallel_decide_min) {
        // Intents, then claims (PEO-080): every unit's choice reads only the second's
        // starting occupancy and reservations, so the units decide as pieces, each into
        // its own slot. A calm unit whose tile is taken stays put, so resolving claims in
        // slot order (ascending unit index) gives each tile to the lowest index that
        // wanted it: exactly the serial loop's outcome.
        d.intents.resize(batch); // reserved at the stage's start: no allocation
        run_ranges(executor_, batch, [&](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) {
                const std::uint32_t u = d.slot_units[first + i];
                d.intents[i] = d.moving[u] != 0 ? std::nullopt
                                                : decide_move(d.horde[u], desire_, d.occupied, d.reserved,
                                                              draw_word(stage_salt_ ^ kDrawSalt, t, u));
            }
        });
        for (std::uint32_t i = 0; i < batch; ++i) {
            if (const auto to = d.intents[i]; to && !d.reserved.at(*to)) {
                decided(d, {.unit = d.slot_units[first + i], .to = *to});
            }
        }
        d.intents.clear();
    } else {
        for (std::uint32_t k = first; k < first + batch; ++k) {
            const std::uint32_t u = d.slot_units[k];
            if (d.moving[u] != 0) {
                continue;
            }
            if (const auto to = decide_move(d.horde[u], desire_, d.occupied, d.reserved,
                                            draw_word(stage_salt_ ^ kDrawSalt, t, u))) {
                decided(d, {.unit = u, .to = *to});
            }
        }
    }
    if (record != nullptr) {
        record->decided.insert(record->decided.end(), d.deciding.begin(), d.deciding.end());
        record->decided_begin.push_back(static_cast<std::uint32_t>(record->decided.size()));
    }
    land(d);
}

void World::replay_second(Seconds t, const Speculation& spec) {
    if (t == spec.poll_at) {
        dead_.slot_begin = spec.poll_begin; // the poll's buckets, as it would have made them
        dead_.slot_units = spec.poll_units;
    }
    const std::size_t i = t - spec.from - 1;
    for (std::uint32_t k = spec.decided_begin[i]; k < spec.decided_begin[i + 1]; ++k) {
        decided(dead_, spec.decided[k]);
    }
    land(dead_);
}
void World::advance(Seconds duration, Speculation* spec) {
    for (Seconds left = std::max<Seconds>(duration, 1); left > 0; --left) {
        log_seconds(1);
        ++seconds_;
        if (spec != nullptr && seconds_ > spec->from && seconds_ <= spec->to) {
            replay_second(seconds_, *spec);
        } else {
            dead_second(dead_, seconds_, nullptr);
        }
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

void World::set_executor(Executor* executor) noexcept {
    executor_ = executor;
    scent_.set_executor(executor);
}

void World::speculate(Speculation& out) const {
    out.update = updates_;
    out.stage_index = stage_index_;
    // Two independent halves (PEO-080): the update writes out.scent, the Dead's seconds
    // read only scent_ and write out.ahead and the record, so they run as two pieces.
    run_pieces(executor_, 2, [&](std::size_t half) {
        if (half == 0) {
            speculate_scent(out);
        } else {
            speculate_dead(out);
        }
    });
    // The desire for the next window reads both halves: the new field and the
    // occupancy at the boundary. Built only when the Dead's seconds reached it.
    if (out.to == (seconds_ / params_.update_period + 1) * params_.update_period) {
        if (out.desire.width() != stage_.spec.width || out.desire.height() != stage_.spec.height) {
            out.desire = DesireField(stage_.spec.width, stage_.spec.height); // cold: once a stage size
        }
        out.desire.build(out.scent, stage_.blocked, out.ahead.occupied, params_.draw, executor_);
    }
}

void World::speculate_scent(Speculation& out) const {
    // A partner wave (the one finish_from swapped out) copies only what changed.
    out.synced_tiles = out.scent.sync_from(scent_);
    out.scent.set_executor(executor_);
    out.scent.update(stage_.blocked, &stage_.openness);
}

void World::speculate_dead(Speculation& out) const {
    // The Dead's seconds up to and including the boundary read only scent_, which
    // cannot change before it, so they are fixed now. A second poll in the window
    // (only when update_period > dead_cycle) is left to run live.
    out.ahead = dead_;
    // Bounds, reserved once so no speculate allocates when warm: one pending move
    // per unit; a unit decides at most every other second (it is moving the next);
    // one poll's slots, as HordeState's own bound.
    const std::size_t units = dead_.horde.size();
    out.ahead.deciding.reserve(units);
    out.ahead.landing.reserve(units);
    out.ahead.intents.reserve(units);
    out.decided.reserve(units * ((params_.update_period + 1) / 2));
    out.ahead.slot_units.reserve(units * params_.dead_cycle);
    out.poll_units.reserve(units * params_.dead_cycle);
    out.poll_begin.reserve(static_cast<std::size_t>(params_.dead_cycle) + 1);
    out.decided_begin.reserve(static_cast<std::size_t>(params_.update_period) + 1);
    out.from = seconds_;
    out.poll_at = 0;
    out.decided.clear();
    out.decided_begin.assign(1, 0);
    const Seconds boundary = (seconds_ / params_.update_period + 1) * params_.update_period;
    Seconds t = seconds_ + 1;
    for (; t <= boundary; ++t) {
        if (t % params_.dead_cycle == 0 && out.poll_at != 0) {
            break;
        }
        dead_second(out.ahead, t, &out);
    }
    out.to = t - 1;
}
void World::commit(Speculation& spec, Action action) {
    // patch_deposit is exact for one round per update; faster waves run live.
    if (spec.update != updates_ || spec.stage_index != stage_index_ || spec.from > seconds_ ||
        params_.scent.speed != 1) {
        step(action);
        return;
    }
    apply_action(action);
    advance(action.seconds, &spec);
    finish_turn();
}

bool World::equivalent(const World& a, const World& b) noexcept {
    const HordeState& da = a.dead_;
    const HordeState& db = b.dead_;
    if (a.stage_index_ != b.stage_index_ || a.turn_ != b.turn_ || a.seconds_ != b.seconds_ ||
        a.updates_ != b.updates_ || a.player_ != b.player_ || a.log_.size() != b.log_.size() ||
        da.horde.size() != db.horde.size() || a.scent_.updates() != b.scent_.updates() ||
        a.scent_.values() != b.scent_.values() || !same_moves(da.landing, db.landing) ||
        !same_moves(da.deciding, db.deciding) || da.moving != db.moving || da.slot_begin != db.slot_begin ||
        da.slot_units != db.slot_units || !same_cells(da.occupied, db.occupied) ||
        !same_cells(da.reserved, db.reserved) || !(a.desire_ == b.desire_)) {
        return false;
    }
    for (std::size_t i = 0; i < a.log_.size(); ++i) {
        if (a.log_[i].tile != b.log_[i].tile || a.log_[i].seconds != b.log_[i].seconds) {
            return false;
        }
    }
    for (std::size_t i = 0; i < da.horde.size(); ++i) {
        if (da.horde[i].pos != db.horde[i].pos || da.horde[i].step_seconds != db.horde[i].step_seconds) {
            return false;
        }
    }
    return true;
}
} // namespace peo::core
