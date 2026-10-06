#include "peo/core/world.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
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
/// And so each unit's fate is a hash of its own (PEO-094): the spawn stream never moves.
constexpr std::uint64_t kFateSalt = 0xFA7EULL;
/// Range of the spawn draw: updates between a spawned Dead's steps. Stored in
/// substeps as draw * update_period, so the draw sequence is unchanged (D-015).
constexpr int kMinDeadSpeed = 1;
constexpr int kMaxDeadSpeed = 3;
/// Cap on the occupancy log's up-front reservation. At most update_period distinct
/// tiles fit in one update (each action spends at least a substep on one tile).
constexpr Substeps kMaxLogReserve = 64;

/// `s` rounded up to a whole number of slots, at least one (ADR-0016): the Dead and
/// the update run on even substeps only.
constexpr Substeps whole_slots(Substeps s) noexcept {
    // In 64 bits and saturating: the largest Substeps rounds down to the largest even one
    // rather than wrapping to 0.
    const std::uint64_t slots =
        std::max<std::uint64_t>((std::uint64_t{s} + kSlotSubsteps - 1) / kSlotSubsteps, 1);
    constexpr std::uint64_t kMaxSlots = std::numeric_limits<Substeps>::max() / kSlotSubsteps;
    return static_cast<Substeps>(std::min(slots, kMaxSlots) * kSlotSubsteps);
}
/// The largest step a Dead's uint16 holds that is a whole number of slots. Also the
/// longest cycle a World keeps: a unit's buckets reserve one entry per slot of it.
constexpr Substeps kMaxStepSubsteps =
    std::numeric_limits<std::uint16_t>::max() / kSlotSubsteps * kSlotSubsteps;
/// The largest update period whose slowest spawned step (kMaxDeadSpeed x it) still fits.
constexpr Substeps kMaxUpdatePeriod = kMaxStepSubsteps / kMaxDeadSpeed / kSlotSubsteps * kSlotSubsteps;

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

/// Give every unit its slots for cycle `cycle` (D-031), bucketed by slot in
/// ascending unit order. `len` is the cycle in slots.
void poll(HordeState& d, std::uint64_t salt, std::uint64_t cycle, Slot len) {
    d.slot_begin.assign(static_cast<std::size_t>(len) + 1, 0);
    for (std::size_t i = 0; i < d.horde.size(); ++i) {
        // Every unit draws at its slots (PEO-009): one with nowhere better to go
        // wanders or stays, by the draw.
        d.plans[i] = plan_slots(salt, cycle, i, d.horde[i].step_substeps / kSlotSubsteps, len);
        for (Slot j = 0; j < d.plans[i].count; ++j) {
            ++d.slot_begin[slot_second(d.plans[i], j, len) + 1];
        }
    }
    for (Slot s = 0; s < len; ++s) {
        d.slot_begin[s + 1] += d.slot_begin[s];
    }
    d.slot_units.resize(d.slot_begin[len]);
    // Fill each bucket in ascending unit order; slot_begin[s] walks up to the
    // bucket's end as it fills, and is walked back after.
    for (std::size_t i = 0; i < d.horde.size(); ++i) {
        for (Slot j = 0; j < d.plans[i].count; ++j) {
            d.slot_units[d.slot_begin[slot_second(d.plans[i], j, len)]++] = static_cast<std::uint32_t>(i);
        }
    }
    for (Slot s = len; s > 0; --s) {
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

/// Last slot's moves land; this slot's become next slot's landings.
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
    params_.update_period = std::min(whole_slots(params_.update_period), kMaxUpdatePeriod);
    params_.dead_cycle = std::min(whole_slots(params_.dead_cycle), kMaxStepSubsteps);
    params_.attrition = valid(params_.attrition) ? normalized(params_.attrition) : kDefaultAttrition;
    log_.reserve(std::min(params_.update_period, kMaxLogReserve));
    load_stage(0);
}

void World::enter_stage(std::uint32_t index) {
    stage_index_ = index;
    StageSpec spec = stage_spec(seed_, index);
    spec.width = params_.stage_width;
    spec.height = params_.stage_height;
    stage_ = generate_stage(spec);
    hand_built_ = false;
    start_field(stage_seed(seed_, index));
    player_ = stage_.entry;
    rng_.reseed(stage_seed(seed_, index) ^ kHordeSeedSalt);
}

void World::load_stage(std::uint32_t index) {
    enter_stage(index);
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
             .step_substeps = static_cast<std::uint16_t>(
                 static_cast<Substeps>(rng_.range(kMinDeadSpeed, kMaxDeadSpeed)) * params_.update_period)});
    }
    start_horde(stage_seed(seed_, index));
    draw_fates(stage_seed(seed_, index));
}

std::optional<AreaRecord> World::store_area() const {
    if (hand_built_) {
        return std::nullopt;
    }
    return AreaRecord{.stage_index = stage_index_, .updated_at = since_event(), .horde = dead_.horde};
}

void World::resume_area(AreaRecord record) {
    // The clock at the return, read before start_horde moves the stage counters into
    // the event clock.
    const EventSubsteps now = since_event();
    enter_stage(record.stage_index);
    const int w = stage_.spec.width;
    const int h = stage_.spec.height;
    // Make room before thinning, against the whole stored horde: where a unit is moved
    // then depends only on the record, never on when it is resumed, so several short
    // absences still leave exactly what one long one does. A unit moves when it stands
    // on the player's entry or where it cannot stand, to the nearest free open cell,
    // ring by ring, rows then columns; a stage with none left drops it rather than put
    // two on a tile (D-031).
    Grid<std::uint8_t> taken(w, h, 0);
    const auto standable = [&](Vec2i p) {
        return stage_.blocked.in_bounds(p) && !stage_.blocked.at(p) && p != player_ && taken.at(p) == 0;
    };
    std::vector<std::uint8_t> placed(record.horde.size(), 0);
    for (std::size_t i = 0; i < record.horde.size(); ++i) {
        if (standable(record.horde[i].pos)) {
            ++taken.at(record.horde[i].pos);
            placed[i] = 1;
        }
    }
    for (std::size_t i = 0; i < record.horde.size(); ++i) {
        if (placed[i] != 0) {
            continue;
        }
        const Vec2i at = record.horde[i].pos;
        const Vec2i from{std::clamp(at.x, 0, w - 1), std::clamp(at.y, 0, h - 1)};
        for (int r = 1; r < std::max(w, h) && placed[i] == 0; ++r) {
            for (int dy = -r; dy <= r && placed[i] == 0; ++dy) {
                // The ring's perimeter only: its top and bottom rows whole, else its two ends.
                const int stride = std::abs(dy) == r ? 1 : 2 * r;
                for (int dx = -r; dx <= r; dx += stride) {
                    const Vec2i c = from + Vec2i{dx, dy};
                    if (standable(c)) {
                        record.horde[i].pos = c;
                        ++taken.at(c);
                        placed[i] = 1;
                        break;
                    }
                }
            }
        }
    }
    std::size_t kept = 0;
    for (std::size_t i = 0; i < record.horde.size(); ++i) {
        if (placed[i] != 0) {
            record.horde[kept++] = record.horde[i];
        }
    }
    record.horde.resize(kept);
    // Closed form: one comparison a unit (ADR-0019 point 2).
    thin_horde(record.horde, params_.attrition, now);
    HordeState& d = dead_;
    d.horde = std::move(record.horde);
    d.occupied = Grid<std::uint8_t>(w, h, 0);
    for (const Dead& unit : d.horde) {
        ++d.occupied.at(unit.pos);
    }
    start_horde(stage_seed(seed_, stage_index_));
}

void World::draw_fates(Seed stage_seed_value) noexcept {
    const EventSubsteps now = since_event();
    for (std::size_t i = 0; i < dead_.horde.size(); ++i) {
        dead_.horde[i].fate = draw_fate(params_.attrition, now, hash_u64(stage_seed_value ^ kFateSalt, i, 0));
    }
}

void World::load_layout(Stage stage, Vec2i player, std::vector<Dead> horde) {
    stage_ = std::move(stage);
    stage_.exit = kNoExit;
    hand_built_ = true;
    const int w = stage_.blocked.width();
    const int h = stage_.blocked.height();
    stage_.spec.width = w;
    stage_.spec.height = h;
    if (stage_.openness.width() != w || stage_.openness.height() != h) {
        stage_.openness = Grid<std::uint8_t>(w, h, kOpennessIndoors);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                stage_.openness.at(x, y) = stage_.blocked.at(x, y) ? kOpennessIndoors : kOpennessOutdoors;
            }
        }
    }
    start_field(stage_seed(seed_, stage_index_));
    player_ = player;
    HordeState& d = dead_;
    d.horde = std::move(horde);
    d.occupied = Grid<std::uint8_t>(w, h, 0);
    for (Dead& unit : d.horde) {
        unit.step_substeps =
            static_cast<std::uint16_t>(std::min(whole_slots(unit.step_substeps), kMaxStepSubsteps));
        ++d.occupied.at(unit.pos);
    }
    start_horde(stage_seed(seed_, stage_index_));
    draw_fates(stage_seed(seed_, stage_index_));
}

void World::start_field(Seed stage_seed_value) {
    scent_ = ScentWave(stage_.spec.width, stage_.spec.height, params_.scent);
    scent_.set_token(ScentWave::new_token());
    Rng wind_rng(stage_seed_value ^ kWindSeedSalt);
    wind_.toward_degrees = wind_rng.range(0, kDegreesPerTurn - 1);
    wind_.intensity = wind_rng.range(0, std::clamp(params_.wind_max, 0, kMaxWindStep));
    scent_.set_wind(wind_);
    scent_.set_executor(executor_);
}

void World::start_horde(Seed stage_seed_value) {
    HordeState& d = dead_;
    ++epoch_;
    earlier_substeps_ += substeps_; // the event clock keeps running (PEO-094)
    turn_ = 0;
    substeps_ = 0;
    updates_ = 0;
    log_.clear();

    d.reserved = Grid<bool>(stage_.spec.width, stage_.spec.height, false);
    d.moving.assign(d.horde.size(), 0);
    d.plans.resize(d.horde.size());
    // At most one slot per unit per slot of the cycle: reserve the bound once so
    // no poll allocates, however the hashes fall.
    d.slot_units.reserve(d.horde.size() * cycle_slots());
    d.deciding.clear();
    d.landing.clear();
    d.deciding.reserve(d.horde.size());
    d.landing.reserve(d.horde.size());
    d.intents.reserve(d.horde.size());
    stage_salt_ = stage_seed_value;
    desire_ = DesireField(stage_.spec.width, stage_.spec.height);
    desire_.build(scent_, stage_.blocked, dead_.occupied, params_.draw, executor_);
    dead_slot(dead_, 0, nullptr); // instant 0: the first poll and slot 0's decisions
}

void World::restore_horde(std::vector<Dead> horde, std::vector<DeadMove> landing) {
    HordeState& d = dead_;
    const std::size_t units = horde.size();
    d.horde = std::move(horde);
    d.occupied = Grid<std::uint8_t>(stage_.spec.width, stage_.spec.height, 0);
    for (const Dead& unit : d.horde) {
        ++d.occupied.at(unit.pos);
    }
    d.reserved = Grid<bool>(stage_.spec.width, stage_.spec.height, false);
    d.moving.assign(units, 0);
    d.plans.resize(units);
    d.slot_units.reserve(units * cycle_slots());
    d.deciding.clear();
    d.deciding.reserve(units);
    d.landing = std::move(landing);
    d.landing.reserve(units);
    d.intents.reserve(units);
    for (const DeadMove& m : d.landing) {
        d.reserved.at(m.to) = true;
        d.moving[m.unit] = 1;
    }
    // The buckets are a pure hash of (salt, cycle, index, step): the last poll ran at
    // the start of the cycle the clock is in, so polling that cycle again rebuilds them.
    const Slot len = cycle_slots();
    poll(d, stage_salt_, (substeps_ / kSlotSubsteps) / len, len);
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

void World::log_substeps(Substeps s) {
    for (Occupancy& o : log_) {
        if (o.tile == player_) {
            o.substeps += s;
            return;
        }
    }
    log_.push_back({.tile = player_, .substeps = s});
}

// A tile held for the whole period deposits strength; one held for part of it
// deposits as if the scent were that much older: strength less age_cost x the
// share of the period it was empty, never more (a runner's tiles read a little
// less than a walker's, D-015). Integer, so the port is exact; for an even `held`
// age_cost x (12 - held) / 12 is exactly the old age_cost x (6 - held / 2) / 6.
std::int32_t World::logged_strength(Substeps held) const noexcept {
    const auto period = static_cast<std::int32_t>(params_.update_period);
    const auto in = static_cast<std::int32_t>(std::min(held, params_.update_period));
    return params_.scent.strength - params_.scent.age_cost * (period - in) / period;
}

void World::deposit_log(ScentWave& field) const noexcept {
    for (const Occupancy& o : log_) {
        field.deposit(o.tile, logged_strength(o.substeps));
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
    const bool desire_ready = spec.to == substeps_; // the Dead's slots reached the boundary
    for (const Occupancy& o : log_) {
        spec.scent.patch_deposit(scent_, o.tile, logged_strength(o.substeps), stage_.blocked,
                                 &stage_.openness);
        if (desire_ready) {
            spec.desire.rebuild(spec.scent, stage_.blocked, params_.draw, o.tile.x - reach, o.tile.y - reach,
                                o.tile.x + reach + 1, o.tile.y + reach + 1);
        }
    }
    std::swap(scent_, spec.scent);     // swap, not move: spec keeps its buffers to reuse
    scent_.set_field_backend(nullptr); // the swap brought the speculation's backend over
    if (desire_ready) {
        std::swap(desire_, spec.desire);
    } else { // a second poll stopped the speculation early: build the snapshot now
        desire_.build(scent_, stage_.blocked, dead_.occupied, params_.draw, executor_);
    }
    spec.update = kSpentUpdate;
    log_.clear();
    ++updates_;
}

// D-031's order within one slot t: the slot's units decide first, so a unit
// that decided at t-1 still stands on its old tile while they look; then the
// moves decided at t-1 land. A vacated tile is therefore free only from t+1.
// Slots are today's whole seconds on the even substeps (ADR-0016), so every
// hash below sees the numbers it always did.
void World::dead_slot(HordeState& d, Slot t, Speculation* record) const {
    const Slot len = cycle_slots();
    if (t % len == 0) {
        poll(d, stage_salt_, t / len, len);
        if (record != nullptr) {
            record->poll_at = t * kSlotSubsteps;
            record->poll_begin = d.slot_begin; // copy-assign reuses capacity
            record->poll_units = d.slot_units;
        }
    }
    const Slot s = t % len;
    const std::uint32_t first = d.slot_begin[s];
    const std::uint32_t batch = d.slot_begin[s + 1] - first;
    if (executor_ != nullptr && executor_->width() > 1 && batch >= params_.parallel_decide_min) {
        // Intents, then claims (PEO-080): every unit's choice reads only the slot's
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

void World::replay_slot(Substeps t, const Speculation& spec) {
    if (t == spec.poll_at) {
        dead_.slot_begin = spec.poll_begin; // the poll's buckets, as it would have made them
        dead_.slot_units = spec.poll_units;
    }
    // The record starts at the first even substep after `from`.
    const std::size_t i = t / kSlotSubsteps - spec.from / kSlotSubsteps - 1;
    for (std::uint32_t k = spec.decided_begin[i]; k < spec.decided_begin[i + 1]; ++k) {
        decided(dead_, spec.decided[k]);
    }
    land(dead_);
}
void World::advance(Substeps duration, Speculation* spec) {
    // An odd substep (the between layer, ADR-0016) costs the counter and the log only.
    for (Substeps left = duration == 0 ? kSlotSubsteps : duration; left > 0; --left) {
        log_substeps(1);
        ++substeps_;
        if (substeps_ % kSlotSubsteps != 0) {
            continue;
        }
        if (spec != nullptr && substeps_ > spec->from && substeps_ <= spec->to) {
            replay_slot(substeps_, *spec);
        } else {
            dead_slot(dead_, substeps_ / kSlotSubsteps, nullptr);
        }
        if (substeps_ % params_.update_period == 0) {
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
    advance(action.substeps, nullptr);
    finish_turn();
}

Speculation World::speculate() const {
    Speculation spec;
    speculate(spec);
    return spec;
}

void World::set_field_backend(FieldBackend* backend) noexcept {
    // Only the speculation's wave runs on it: the World's own updates (a live step when
    // no speculation is ready) stay on the CPU, so a turn never waits on the GPU.
    field_backend_ = backend;
}

void World::set_executor(Executor* executor) noexcept {
    executor_ = executor;
    scent_.set_executor(executor);
}

void World::speculate(Speculation& out) const {
    out.update = updates_;
    out.epoch = epoch_;
    out.stage_index = stage_index_;
    // Two independent halves (PEO-080): the update writes out.scent, the Dead's slots
    // read only scent_ and write out.ahead and the record, so they run as two pieces.
    run_pieces(executor_, 2, [&](std::size_t half) {
        if (half == 0) {
            speculate_scent(out);
        } else {
            speculate_dead(out);
        }
    });
    // The desire for the next window reads both halves: the new field and the
    // occupancy at the boundary. Built only when the Dead's slots reached it.
    if (out.to == (substeps_ / params_.update_period + 1) * params_.update_period) {
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
    out.scent.set_field_backend(field_backend_);
    out.scent.update(stage_.blocked, &stage_.openness);
}

void World::speculate_dead(Speculation& out) const {
    // The Dead's slots up to and including the boundary read only scent_, which
    // cannot change before it, so they are fixed now. A second poll in the window
    // (only when update_period > dead_cycle) is left to run live.
    out.ahead = dead_;
    // Bounds, reserved once so no speculate allocates when warm: one pending move
    // per unit; a unit decides at most every other slot (it is moving the next);
    // one poll's slots, as HordeState's own bound.
    const std::size_t units = dead_.horde.size();
    const Slot period_slots = params_.update_period / kSlotSubsteps;
    out.ahead.deciding.reserve(units);
    out.ahead.landing.reserve(units);
    out.ahead.intents.reserve(units);
    out.decided.reserve(units * ((period_slots + 1) / 2));
    out.ahead.slot_units.reserve(units * cycle_slots());
    out.poll_units.reserve(units * cycle_slots());
    out.poll_begin.reserve(static_cast<std::size_t>(cycle_slots()) + 1);
    out.decided_begin.reserve(static_cast<std::size_t>(period_slots) + 1);
    out.from = substeps_;
    out.poll_at = 0;
    out.decided.clear();
    out.decided_begin.assign(1, 0);
    const Substeps boundary = (substeps_ / params_.update_period + 1) * params_.update_period;
    // Even substeps only, from the first after `from`; the boundary is even.
    Substeps t = (substeps_ / kSlotSubsteps + 1) * kSlotSubsteps;
    for (; t <= boundary; t += kSlotSubsteps) {
        if (t % params_.dead_cycle == 0 && out.poll_at != 0) {
            break;
        }
        dead_slot(out.ahead, t / kSlotSubsteps, &out);
    }
    out.to = std::min(t - 1, boundary);
}
void World::commit(Speculation& spec, Action action) {
    // patch_deposit is exact for one round per update; faster waves run live.
    if (spec.update != updates_ || spec.stage_index != stage_index_ || spec.epoch != epoch_ ||
        spec.from > substeps_ || params_.scent.speed != 1) {
        step(action);
        return;
    }
    apply_action(action);
    advance(action.substeps, &spec);
    finish_turn();
}

bool World::equivalent(const World& a, const World& b) noexcept {
    const HordeState& da = a.dead_;
    const HordeState& db = b.dead_;
    if (a.stage_index_ != b.stage_index_ || a.turn_ != b.turn_ || a.substeps_ != b.substeps_ ||
        a.earlier_substeps_ != b.earlier_substeps_ || a.params_.start_day != b.params_.start_day ||
        !(a.params_.attrition == b.params_.attrition) || a.updates_ != b.updates_ || a.player_ != b.player_ ||
        a.log_.size() != b.log_.size() || da.horde.size() != db.horde.size() ||
        a.scent_.updates() != b.scent_.updates() || a.scent_.values() != b.scent_.values() ||
        !same_moves(da.landing, db.landing) || !same_moves(da.deciding, db.deciding) ||
        da.moving != db.moving || da.slot_begin != db.slot_begin || da.slot_units != db.slot_units ||
        !same_cells(da.occupied, db.occupied) || !same_cells(da.reserved, db.reserved) ||
        !(a.desire_ == b.desire_)) {
        return false;
    }
    for (std::size_t i = 0; i < a.log_.size(); ++i) {
        if (a.log_[i].tile != b.log_[i].tile || a.log_[i].substeps != b.log_[i].substeps) {
            return false;
        }
    }
    for (std::size_t i = 0; i < da.horde.size(); ++i) {
        if (da.horde[i].pos != db.horde[i].pos || da.horde[i].step_substeps != db.horde[i].step_substeps ||
            da.horde[i].fate != db.horde[i].fate) {
            return false;
        }
    }
    return true;
}
} // namespace peo::core
