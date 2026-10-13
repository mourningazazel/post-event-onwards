// World::save and World::restore (PEO-091): the World's sections of a save image.
// The container (save.hpp) knows nothing of them; this file knows nothing of bytes on
// disk. Each section is written field by field through ByteWriter and read back through
// ByteReader, and every value read is checked before the World uses it.

#include "peo/core/byte_io.hpp"
#include "peo/core/world.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace peo::core {

namespace {
constexpr std::uint32_t kWorldSection = section_id("WRLD");
constexpr std::uint32_t kStageSection = section_id("STAG");
constexpr std::uint32_t kScentSection = section_id("SCNT");
constexpr std::uint32_t kHordeSection = section_id("HORD");
constexpr std::uint32_t kLogSection = section_id("OCCL");
constexpr std::uint32_t kCharacterSection = section_id("CHAR");
/// The player's map memory (PEO-124, D-051 A). Optional: saves from before it have none.
constexpr std::uint32_t kSeenSection = section_id("SEEN");
// CDIC (PEO-092): the content dictionary. The rule it fixes: any section that refers to content
// stores a u32 index into CDIC, never an ID string, so migration runs once per distinct
// archetype, not once per item, and leaves a record's own fields (material, quality, contents)
// alone.
constexpr std::uint32_t kDictionarySection = section_id("CDIC");
// The least one CDIC entry takes: an empty-length id, an ancestry count and an empty-length name.
constexpr std::size_t kMinDictionaryEntryBytes = 4 + 2 + 4;
constexpr std::uint16_t kSectionVersion = 1;

/// The largest stage side a save may ask for: D-036 plans stages to 2048 square, so
/// twice that is no save this game wrote, and is refused before anything is allocated.
constexpr int kMaxRestoreSide = 4096;
/// Bounds on the wave's rounds per update, for the same reason: the patch block is
/// sized from them.
constexpr int kMaxRestoreRounds = 64;
constexpr int kMaxRestoreCompanyRadius = 64;
/// The longest update period and Dead cycle a save may hold, far past the game's own
/// (12 and 18 substeps): the Dead's buffers are reserved per slot of them, so a forged
/// value must be refused before a World is built from it.
constexpr Substeps kMaxRestorePeriod = 240;
constexpr Substeps kMaxRestoreCycle = 360;
/// The substeps a save may say ran on earlier stages: far past centuries of play, and
/// low enough that since_event() cannot overflow.
constexpr EventSubsteps kMaxRestoreEarlier = EventSubsteps{1} << 60;
/// One character today (CHAR is written as a count so a second is a second record).
constexpr std::uint32_t kCharacters = 1;
/// Bytes per record, so a count is checked against what is left before allocating.
constexpr std::uint64_t kLogRecordBytes = 12;  // Vec2i, u32
constexpr std::uint64_t kDeadRecordBytes = 12; // Vec2i, u16 step, u16 fate
constexpr std::uint64_t kMoveRecordBytes = 12; // u32, Vec2i
/// SEEN packs a cell a bit, row-major, the first cell in a byte's least significant bit.
constexpr std::size_t kCellsPerByte = 8;
constexpr std::uint16_t kMaxStep = std::numeric_limits<std::uint16_t>::max() / kSlotSubsteps * kSlotSubsteps;

void write_params(ByteWriter& w, const WorldParams& p) {
    w.i32(p.initial_dead);
    w.i32(p.scent.strength);
    w.i32(p.scent.distance_cost);
    w.i32(p.scent.age_cost);
    w.i32(p.scent.speed);
    w.i32(p.scent.gust);
    w.i32(p.scent.wind_loss);
    w.i32(p.stage_width);
    w.i32(p.stage_height);
    w.u32(p.update_period);
    w.u32(p.dead_cycle);
    w.i32(p.wind_max);
    w.u64(p.parallel_decide_min);
    w.i32(p.draw.lean_edge);
    w.i32(p.draw.lean_full);
    w.i32(p.draw.stay);
    w.i32(p.draw.company_gain);
    w.i32(p.draw.company_radius);
    w.u32(p.start_day);
    w.u8(p.attrition.count);
    for (const CurveKnot& k : p.attrition.knots) {
        w.u32(k.day);
        w.u32(k.affected);
    }
}

WorldParams read_params(ByteReader& r) {
    WorldParams p;
    p.initial_dead = r.i32();
    p.scent.strength = r.i32();
    p.scent.distance_cost = r.i32();
    p.scent.age_cost = r.i32();
    p.scent.speed = r.i32();
    p.scent.gust = r.i32();
    p.scent.wind_loss = r.i32();
    p.stage_width = r.i32();
    p.stage_height = r.i32();
    p.update_period = r.u32();
    p.dead_cycle = r.u32();
    p.wind_max = r.i32();
    p.parallel_decide_min = static_cast<std::size_t>(r.u64());
    p.draw.lean_edge = r.i32();
    p.draw.lean_full = r.i32();
    p.draw.stay = r.i32();
    p.draw.company_gain = r.i32();
    p.draw.company_radius = r.i32();
    p.start_day = r.u32();
    p.attrition.count = r.u8();
    for (CurveKnot& k : p.attrition.knots) {
        k.day = r.u32();
        k.affected = r.u32();
    }
    // Knots past the count mean nothing; a World keeps them zero, so a save that says
    // otherwise fails the kept-parameters comparison below.
    return p;
}

template <typename T> void write_grid(ByteWriter& w, const Grid<T>& g) {
    w.u64(g.size());
    for (const auto v : g) {
        w.u8(static_cast<std::uint8_t>(v));
    }
}

bool sides_ok(int w, int h) noexcept {
    return w >= kMinStageSide && h >= kMinStageSide && w <= kMaxRestoreSide && h <= kMaxRestoreSide;
}

bool open_cell(const Stage& s, Vec2i p) noexcept {
    return s.blocked.in_bounds(p) && !s.blocked.at(p);
}

/// CDIC's payload: u32 entry count, then per entry the id, a u16 ancestry count and each
/// ancestor nearest first, and the base name, every string a u32 length and its bytes.
void write_dictionary(ByteWriter& w, std::span<const ContentRef> entries) {
    w.u32(static_cast<std::uint32_t>(entries.size()));
    for (const ContentRef& entry : entries) {
        w.string(entry.id);
        w.u16(static_cast<std::uint16_t>(entry.ancestry.size()));
        for (const std::string& ancestor : entry.ancestry) {
            w.string(ancestor);
        }
        w.string(entry.name);
    }
}

/// Read CDIC into `out`, or say what is wrong with it. Every length is checked before anything
/// is allocated for it.
std::optional<std::string> read_dictionary(ByteReader& r, std::vector<ContentRef>& out) {
    const std::uint32_t count = r.u32();
    if (r.failed()) {
        return "payload ends early";
    }
    if (count > r.remaining() / kMinDictionaryEntryBytes) {
        return "entry count past the bytes left";
    }
    out.reserve(count);
    const auto id = [&r](std::string& into) -> std::optional<std::string> {
        std::optional<std::string> text = r.string(kMaxContentIdBytes);
        if (!text) {
            return r.failed() ? "payload ends early"
                              : "an id over " + std::to_string(kMaxContentIdBytes) + " bytes";
        }
        if (text->empty()) {
            return "an empty id";
        }
        into = std::move(*text);
        return std::nullopt;
    };
    for (std::uint32_t i = 0; i < count; ++i) {
        ContentRef& entry = out.emplace_back();
        if (auto error = id(entry.id)) {
            return error;
        }
        const std::uint16_t ancestors = r.u16();
        if (r.failed()) {
            return "payload ends early";
        }
        if (ancestors > kMaxSavedAncestry) {
            return "ancestry over " + std::to_string(kMaxSavedAncestry);
        }
        entry.ancestry.resize(ancestors);
        for (std::string& ancestor : entry.ancestry) {
            if (auto error = id(ancestor)) {
                return error;
            }
        }
        std::optional<std::string> name = r.string(kMaxContentNameBytes);
        if (!name) {
            return r.failed() ? "payload ends early"
                              : "a name over " + std::to_string(kMaxContentNameBytes) + " bytes";
        }
        entry.name = std::move(*name);
    }
    if (!r.done()) {
        return "bytes after the payload";
    }
    return std::nullopt;
}

/// Where the parsing and checking of one image goes wrong: the section and the field.
struct Refusal {
    std::uint32_t section;
    SyncScope scope;
    std::string what;
    SaveStatus status = SaveStatus::Corrupt;
};
} // namespace

SaveImage World::save(std::string_view build) const {
    SaveImage image{.format = kSaveFormat, .build = std::string(build), .sections = {}};
    const auto add = [&](std::uint32_t id, SyncScope scope, ByteWriter& w) {
        image.sections.push_back({.id = id, .version = kSectionVersion, .scope = scope, .payload = w.take()});
    };

    ByteWriter world;
    world.u64(seed_);
    write_params(world, params_);
    world.u32(stage_index_);
    world.u8(hand_built_ ? 1 : 0);
    world.u64(turn_);
    world.u32(substeps_);
    world.u64(updates_);
    world.u64(earlier_substeps_);
    for (const std::uint64_t word : rng_.state()) {
        world.u64(word);
    }
    add(kWorldSection, SyncScope::World, world);

    if (hand_built_) {
        ByteWriter stage;
        stage.i32(stage_.spec.width);
        stage.i32(stage_.spec.height);
        write_grid(stage, stage_.blocked);
        write_grid(stage, stage_.openness);
        stage.vec2i(stage_.entry);
        add(kStageSection, SyncScope::World, stage);
    }

    ByteWriter scent;
    scent.u32(scent_.updates());
    scent.i32_vector(scent_.values());
    scent.byte_vector(scent_.changed_tiles());
    add(kScentSection, SyncScope::World, scent);

    // Between turns no move is being decided (land() swaps them into landing), so the
    // pending moves are landing alone.
    ByteWriter horde;
    horde.u32(static_cast<std::uint32_t>(dead_.horde.size()));
    for (const Dead& d : dead_.horde) {
        horde.vec2i(d.pos);
        horde.u16(d.step_substeps);
        horde.u16(d.fate);
    }
    horde.u32(static_cast<std::uint32_t>(dead_.landing.size()));
    for (const DeadMove& m : dead_.landing) {
        horde.u32(m.unit);
        horde.vec2i(m.to);
    }
    horde.byte_vector(desire_.all_log_odds());
    add(kHordeSection, SyncScope::World, horde);

    ByteWriter log;
    log.u32(static_cast<std::uint32_t>(log_.size()));
    for (const Occupancy& o : log_) {
        log.vec2i(o.tile);
        log.u32(o.substeps);
    }
    add(kLogSection, SyncScope::World, log);

    // CDIC: always written, so the format does not change when items land. The World holds no
    // item yet, so it has no entries.
    ByteWriter dictionary;
    write_dictionary(dictionary, {});
    add(kDictionarySection, SyncScope::World, dictionary);

    ByteWriter character;
    character.u32(kCharacters);
    character.vec2i(player_);
    add(kCharacterSection, SyncScope::Character, character);

    // SEEN: per character, the stage it is of, its size, then its cells as bits; padding
    // bits are zero, so the same memory always gives the same bytes.
    ByteWriter seen;
    seen.u32(kCharacters);
    seen.u32(stage_index_);
    seen.i32(memory_.width());
    seen.i32(memory_.height());
    const Grid<bool>& remembered = memory_.cells();
    std::uint8_t bits = 0;
    std::size_t i = 0;
    for (const std::uint8_t cell : remembered) {
        bits = static_cast<std::uint8_t>(bits | ((cell != 0 ? 1U : 0U) << (i % kCellsPerByte)));
        if (++i % kCellsPerByte == 0) {
            seen.u8(bits);
            bits = 0;
        }
    }
    if (i % kCellsPerByte != 0) {
        seen.u8(bits);
    }
    add(kSeenSection, SyncScope::Character, seen);
    return image;
}

SaveStatus World::restore(const SaveImage& image, std::vector<SaveIssue>& issues) {
    MigrationReport unused;
    return restore(image, issues, unused);
}

SaveStatus World::restore(const SaveImage& image, std::vector<SaveIssue>& issues, MigrationReport& report) {
    const auto refuse = [&](const Refusal& r) {
        issues.push_back({.section_id = r.section, .scope = r.scope, .what = r.what});
        return r.status;
    };
    if (image.format == 0) {
        return refuse({0, SyncScope::World, "format 0", SaveStatus::BadHeader});
    }
    if (image.format > kSaveFormat) {
        return refuse({0, SyncScope::World, "written in a newer format", SaveStatus::NewerFormat});
    }

    // Which sections this World reads, and the scope each must declare.
    const std::map<std::uint32_t, SyncScope> known{
        {kWorldSection, SyncScope::World},    {kStageSection, SyncScope::World},
        {kScentSection, SyncScope::World},    {kHordeSection, SyncScope::World},
        {kLogSection, SyncScope::World},      {kCharacterSection, SyncScope::Character},
        {kSeenSection, SyncScope::Character}, {kDictionarySection, SyncScope::World}};
    std::map<std::uint32_t, const SaveSection*> found;
    for (const SaveSection& s : image.sections) {
        const auto k = known.find(s.id);
        if (k == known.end()) {
            issues.push_back({.section_id = s.id, .scope = s.scope, .what = "unknown section, skipped"});
            continue;
        }
        if (s.version > kSectionVersion) {
            return refuse({s.id, s.scope, "section written by a newer build", SaveStatus::NewerFormat});
        }
        if (s.version == 0 || s.scope != k->second || !found.emplace(s.id, &s).second) {
            return refuse({s.id, s.scope, "bad version, scope or a second copy"});
        }
    }
    for (const std::uint32_t id :
         {kWorldSection, kScentSection, kHordeSection, kLogSection, kCharacterSection}) {
        if (!found.contains(id)) {
            return refuse({id, known.at(id), "missing"});
        }
    }
    const auto reader = [&](std::uint32_t id) { return ByteReader(found.at(id)->payload); };
    const auto whole = [&](const ByteReader& r, std::uint32_t id) -> std::optional<Refusal> {
        if (!r.done()) {
            return Refusal{id, known.at(id), r.failed() ? "payload ends early" : "bytes after the payload"};
        }
        return std::nullopt;
    };

    // WRLD: the parameters are checked for size before anything is built from them.
    ByteReader wr = reader(kWorldSection);
    const Seed seed = wr.u64();
    const WorldParams params = read_params(wr);
    const std::uint32_t stage_index = wr.u32();
    const std::uint8_t hand_built = wr.u8();
    const Tick turn = wr.u64();
    const Substeps substeps = wr.u32();
    const Tick updates = wr.u64();
    const EventSubsteps earlier = wr.u64();
    Rng::State rng{};
    for (std::uint64_t& word : rng) {
        word = wr.u64();
    }
    if (const auto r = whole(wr, kWorldSection)) {
        return refuse(*r);
    }
    const auto world_field = [&](const char* what) {
        return refuse({kWorldSection, SyncScope::World, what});
    };
    if (!sides_ok(params.stage_width, params.stage_height)) {
        return world_field("stage size out of range");
    }
    if (params.initial_dead > params.stage_width * params.stage_height) {
        return world_field("initial_dead out of range");
    }
    if (params.scent.speed < 1 || params.scent.speed > kMaxRestoreRounds || params.scent.gust < 0 ||
        params.scent.gust > kMaxRestoreRounds || params.scent.distance_cost < 1 ||
        params.scent.age_cost < 0 || params.scent.strength < 1 || params.scent.wind_loss < 0) {
        return world_field("scent parameters out of range");
    }
    const auto whole_slots_upto = [](Substeps s, Substeps max) {
        return s >= kSlotSubsteps && s <= max && s % kSlotSubsteps == 0;
    };
    if (!whole_slots_upto(params.update_period, kMaxRestorePeriod) ||
        !whole_slots_upto(params.dead_cycle, kMaxRestoreCycle)) {
        return world_field("update_period or dead_cycle not whole slots in range");
    }
    if (params.draw.company_radius < 0 || params.draw.company_radius > kMaxRestoreCompanyRadius) {
        return world_field("company_radius out of range");
    }
    if (!valid(params.attrition)) {
        return world_field("attrition curve not valid");
    }
    if (earlier > kMaxRestoreEarlier) {
        return world_field("event clock past any game's");
    }
    if (hand_built > 1) {
        return world_field("hand-built flag not 0 or 1");
    }
    if (std::all_of(rng.begin(), rng.end(), [](std::uint64_t w) { return w == 0; })) {
        return world_field("spawn stream state all zero");
    }
    if (hand_built == 1 && !found.contains(kStageSection)) {
        return refuse({kStageSection, SyncScope::World, "missing for a hand-built stage"});
    }
    if (hand_built == 0 && found.contains(kStageSection)) {
        return refuse({kStageSection, SyncScope::World, "present for a generated stage"});
    }

    World w(seed, params);
    {
        // The constructor keeps params whole slots and in range; a save of a World
        // holds them as it kept them, so a difference means the image was not one.
        ByteWriter kept;
        ByteWriter read;
        write_params(kept, w.params_);
        write_params(read, params);
        if (kept.data() != read.data()) {
            return world_field("parameters not as a World keeps them");
        }
    }

    // CHAR, read before the stage is set up so a hand-built stage gets its player.
    ByteReader cr = reader(kCharacterSection);
    const std::uint32_t characters = cr.u32();
    const Vec2i player = cr.vec2i();
    if (const auto r = whole(cr, kCharacterSection)) {
        return refuse(*r);
    }
    if (characters != kCharacters) {
        return refuse({kCharacterSection, SyncScope::Character, "a World holds one character today"});
    }

    if (hand_built == 1) {
        ByteReader sr = reader(kStageSection);
        const int sw = sr.i32();
        const int sh = sr.i32();
        if (!sides_ok(sw, sh)) {
            return refuse({kStageSection, SyncScope::World, "stage size out of range"});
        }
        Stage stage;
        stage.spec.width = sw;
        stage.spec.height = sh;
        stage.blocked = Grid<bool>(sw, sh, false);
        stage.openness = Grid<std::uint8_t>(sw, sh, kOpennessOutdoors);
        const auto read_grid = [&](auto& grid, std::uint8_t max) {
            if (sr.u64() != grid.size() || !sr.has(grid.size())) {
                return false;
            }
            for (auto& cell : grid) {
                const std::uint8_t v = sr.u8();
                if (v > max) {
                    return false;
                }
                cell = v;
            }
            return true;
        };
        if (!read_grid(stage.blocked, 1) || !read_grid(stage.openness, kOpennessOutdoors)) {
            return refuse({kStageSection, SyncScope::World, "blocked or openness grid wrong size or value"});
        }
        stage.entry = sr.vec2i();
        if (const auto r = whole(sr, kStageSection)) {
            return refuse(*r);
        }
        if (!stage.blocked.in_bounds(stage.entry)) {
            return refuse({kStageSection, SyncScope::World, "entry off the stage"});
        }
        w.stage_index_ = stage_index;
        w.load_layout(std::move(stage), player, {});
    } else if (stage_index != 0) {
        w.load_stage(stage_index);
    }
    const int width = w.stage_.spec.width;
    const int height = w.stage_.spec.height;
    const auto cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (!open_cell(w.stage_, player)) {
        return refuse({kCharacterSection, SyncScope::Character, "player not on an open cell of the stage"});
    }

    // The clock: one update per period crossed, and actions take at least a substep.
    const Substeps period = w.params_.update_period;
    if (updates != substeps / period || turn > substeps) {
        return world_field("clock inconsistent with update_period");
    }

    // OCCL: the substeps since the last update, on open cells.
    ByteReader lr = reader(kLogSection);
    const std::uint32_t logged = lr.u32();
    if (logged > period || !lr.has(logged * kLogRecordBytes)) {
        return refuse({kLogSection, SyncScope::World, "more tiles than substeps in a period"});
    }
    std::vector<Occupancy> log;
    Grid<bool> seen(w.stage_.spec.width, w.stage_.spec.height, false); // a World merges per tile
    Substeps held = 0;
    for (std::uint32_t i = 0; i < logged; ++i) {
        const Vec2i tile = lr.vec2i();
        const Substeps s = lr.u32();
        if (!open_cell(w.stage_, tile) || s == 0 || s > period || seen.at(tile)) {
            return refuse(
                {kLogSection, SyncScope::World, "a logged tile off the stage, walled, empty or twice"});
        }
        seen.at(tile) = true;
        held += s;
        log.push_back({.tile = tile, .substeps = s});
    }
    if (const auto r = whole(lr, kLogSection)) {
        return refuse(*r);
    }
    if (held != substeps % period) {
        return refuse({kLogSection, SyncScope::World, "logged substeps differ from the clock"});
    }

    // SCNT: sizes and values are checked by the wave itself.
    ByteReader nr = reader(kScentSection);
    const std::uint32_t scent_updates = nr.u32();
    std::vector<std::int32_t> values = nr.i32_vector();
    std::vector<std::uint8_t> changed = nr.byte_vector<std::uint8_t>();
    if (const auto r = whole(nr, kScentSection)) {
        return refuse(*r);
    }
    if (scent_updates != updates) {
        return refuse({kScentSection, SyncScope::World, "update count differs from the clock"});
    }

    // HORD: every one of the Dead on an open cell, a step of whole slots, at most 255 to
    // a tile; each pending move one of theirs, once, to an open neighbouring cell.
    ByteReader hr = reader(kHordeSection);
    const std::uint32_t units = hr.u32();
    const auto horde_field = [&](const char* what) {
        return refuse({kHordeSection, SyncScope::World, what});
    };
    if (units > cells || !hr.has(units * kDeadRecordBytes)) {
        return horde_field("more of the Dead than cells");
    }
    std::vector<Dead> horde(units);
    Grid<std::uint8_t> count(width, height, 0);
    for (Dead& d : horde) {
        d.pos = hr.vec2i();
        d.step_substeps = hr.u16();
        d.fate = hr.u16();
        if (!open_cell(w.stage_, d.pos) || d.step_substeps == 0 || d.step_substeps % kSlotSubsteps != 0 ||
            d.step_substeps > kMaxStep || count.at(d.pos) == std::numeric_limits<std::uint8_t>::max()) {
            return horde_field("one of the Dead off the stage, walled, crowded or with a bad step");
        }
        ++count.at(d.pos);
    }
    const std::uint32_t pending = hr.u32();
    if (pending > units || !hr.has(pending * kMoveRecordBytes)) {
        return horde_field("more pending moves than the Dead");
    }
    std::vector<DeadMove> landing(pending);
    std::vector<std::uint8_t> moving(units, 0);
    Grid<bool> target(width, height, false);
    for (DeadMove& m : landing) {
        m.unit = hr.u32();
        m.to = hr.vec2i();
        if (m.unit >= units || moving[m.unit] != 0 || !open_cell(w.stage_, m.to) || target.at(m.to) ||
            count.at(m.to) != 0) {
            return horde_field(
                "a pending move for no unit, a second one, or to a walled, taken or reserved cell");
        }
        const Vec2i from = horde[m.unit].pos;
        if (std::max(std::abs(m.to.x - from.x), std::abs(m.to.y - from.y)) != 1) {
            return horde_field("a pending move that is not one step");
        }
        moving[m.unit] = 1;
        target.at(m.to) = true;
    }
    std::vector<std::int8_t> log_odds = hr.byte_vector<std::int8_t>();
    if (const auto r = whole(hr, kHordeSection)) {
        return refuse(*r);
    }

    // SEEN, optional: without it (a save from before PEO-124) the player remembers nothing.
    // The stage load above left w's memory empty at the stage's size; the bits fill it.
    if (found.contains(kSeenSection)) {
        ByteReader er = reader(kSeenSection);
        const auto seen_field = [&](const char* what) {
            return refuse({kSeenSection, SyncScope::Character, what});
        };
        const std::uint32_t seen_characters = er.u32();
        const std::uint32_t seen_stage = er.u32();
        const int seen_width = er.i32();
        const int seen_height = er.i32();
        if (er.failed()) {
            return seen_field("payload ends early");
        }
        if (seen_characters != kCharacters) {
            return seen_field("a World holds one character today");
        }
        if (seen_stage != stage_index || seen_width != width || seen_height != height) {
            return seen_field("stage or size differs from the stage WRLD builds");
        }
        const std::size_t seen_bytes = (cells + kCellsPerByte - 1) / kCellsPerByte;
        if (!er.has(seen_bytes)) {
            return seen_field("payload ends early");
        }
        for (std::size_t b = 0; b < seen_bytes; ++b) {
            const std::uint8_t byte = er.u8();
            for (std::size_t bit = 0; bit < kCellsPerByte; ++bit) {
                if (((byte >> bit) & 1U) == 0) {
                    continue;
                }
                const std::size_t cell = b * kCellsPerByte + bit;
                if (cell >= cells) {
                    return seen_field("a padding bit set");
                }
                w.memory_.mark({static_cast<int>(cell % static_cast<std::size_t>(width)),
                                static_cast<int>(cell / static_cast<std::size_t>(width))});
            }
        }
        if (const auto r = whole(er, kSeenSection)) {
            return refuse(*r);
        }
    }

    // CDIC, optional: an image from before PEO-092 has none and restores with an empty report.
    // Each entry resolves once, into the index-to-Resolved table later sections will read.
    MigrationReport migrated;
    if (found.contains(kDictionarySection)) {
        ByteReader dr = reader(kDictionarySection);
        std::vector<ContentRef> entries;
        if (std::optional<std::string> error = read_dictionary(dr, entries)) {
            return refuse({kDictionarySection, SyncScope::World, std::move(*error)});
        }
        if (!entries.empty() && content_ == nullptr) {
            return refuse({kDictionarySection, SyncScope::World,
                           "content entries but no content to resolve them against"});
        }
        std::vector<Resolved> resolved;
        resolved.reserve(entries.size());
        for (const ContentRef& entry : entries) {
            resolved.push_back(resolve(entry, *content_));
        }
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const Resolved& to = resolved[i];
            if (to.how == Migration::Kept) {
                continue;
            }
            migrated.lines.push_back({entries[i].id, entries[i].name, to,
                                      to.item ? std::string(content_->name(*to.item)) : std::string{}, 0});
        }
    }

    // All read and checked: set the clock first (the slot poll reads it), then the rest.
    w.turn_ = turn;
    w.substeps_ = substeps;
    w.updates_ = updates;
    w.earlier_substeps_ = earlier;
    w.rng_ = Rng::from_state(rng);
    w.player_ = player;
    w.log_.assign(log.begin(), log.end()); // keeps the constructor's reservation: no allocation per step
    if (!w.scent_.restore(std::move(values), scent_updates, std::move(changed))) {
        return refuse(
            {kScentSection, SyncScope::World, "field size or a value not one this stage's wave writes"});
    }
    if (!w.desire_.restore_log_odds(std::move(log_odds), w.stage_.blocked)) {
        return horde_field("desire snapshot not one this stage builds (size, or a closed move offered)");
    }
    w.restore_horde(std::move(horde), std::move(landing));

    Executor* executor = executor_;
    const Content* content = content_;
    FieldBackend* backend = field_backend_;
    const std::uint64_t epoch = epoch_ + 1; // a speculation from before cannot commit
    *this = std::move(w);
    epoch_ = epoch;
    set_executor(executor);
    set_field_backend(backend);
    content_ = content;
    report = std::move(migrated);
    return SaveStatus::Ok;
}

} // namespace peo::core
