// PEO-050: where things are (peo/core/item_store.hpp).
#include "peo/core/item_store.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <limits>
#include <string_view>

namespace peo::core {

namespace {

constexpr std::string_view kContainerFeature = "features.container";
constexpr std::string_view kCompartmentsPrefix = "compartments.";

/// Whether `archetype` has a compartments entry at `index`, built without allocating.
bool has_compartment(const Content& content, ItemId archetype, std::uint32_t index) noexcept {
    std::array<char, 32> path{};
    std::copy(kCompartmentsPrefix.begin(), kCompartmentsPrefix.end(), path.begin());
    char* first = path.data() + kCompartmentsPrefix.size();
    const auto [end, error] = std::to_chars(first, path.data() + path.size(), index);
    if (error != std::errc{}) {
        return false;
    }
    return content.has(archetype, std::string_view(path.data(), static_cast<std::size_t>(end - path.data())));
}

} // namespace

std::size_t ItemStore::KeyHash::operator()(const Key& k) const noexcept {
    // Mix the parts (splitmix64's finaliser); only lookups use it, never an order.
    std::uint64_t h =
        k.a ^ (static_cast<std::uint64_t>(k.b) << 1) ^ (static_cast<std::uint64_t>(k.kind) << 63);
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return static_cast<std::size_t>(h);
}

ItemStore::Key ItemStore::key_of(Location at) noexcept {
    if (at.kind == Location::Kind::Ground) {
        const auto x = static_cast<std::uint32_t>(at.cell.x);
        const auto y = static_cast<std::uint32_t>(at.cell.y);
        return {(static_cast<std::uint64_t>(x) << 32) | y, 0, Location::Kind::Ground};
    }
    return {at.container.value, at.compartment, Location::Kind::In};
}

Location ItemStore::location_of(const Key& key) noexcept {
    if (key.kind == Location::Kind::Ground) {
        return Location::ground(
            {static_cast<std::int32_t>(static_cast<std::uint32_t>(key.a >> 32)),
             static_cast<std::int32_t>(static_cast<std::uint32_t>(key.a & 0xFFFFFFFFULL))});
    }
    return Location::in(ThingId{key.a}, static_cast<std::uint8_t>(key.b));
}

// ---------------------------------------------------------------- reads

std::span<const Entry> ItemStore::listing(Location at) const noexcept {
    const auto it = listings_.find(key_of(at));
    if (it == listings_.end()) {
        return {};
    }
    return it->second;
}

std::span<const Entry> ItemStore::at(Vec2i cell) const noexcept {
    return listing(Location::ground(cell));
}

std::span<const Entry> ItemStore::in(ThingId container, std::uint8_t compartment) const noexcept {
    return listing(Location::in(container, compartment));
}

const Thing* ItemStore::thing(ThingId id) const noexcept {
    const auto it = thing_slot_.find(id.value);
    return it == thing_slot_.end() ? nullptr : &things_[it->second];
}

const Thing& ItemStore::thing_at(std::uint32_t index) const noexcept {
    assert(index < things_.size() && things_[index].id.value != 0);
    return things_[index];
}

const Line& ItemStore::line_at(std::uint32_t index) const noexcept {
    assert(index < lines_.size() && line_used_[index] != 0);
    return lines_[index];
}

std::uint32_t ItemStore::compartments(const Content& content, ThingId container) const noexcept {
    const Thing* t = thing(container);
    if (t == nullptr) {
        return 0;
    }
    std::uint32_t count = 0;
    while (count < kMaxCompartments && has_compartment(content, t->archetype, count)) {
        ++count;
    }
    if (count == 0 && content.has(t->archetype, kContainerFeature)) {
        return 1;
    }
    return count;
}

std::vector<ThingId> ItemStore::thing_ids() const {
    std::vector<ThingId> ids;
    ids.reserve(thing_slot_.size());
    for (const auto& [id, slot] : thing_slot_) {
        ids.push_back(ThingId{id});
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<Location> ItemStore::locations() const {
    std::vector<Location> out;
    out.reserve(listings_.size());
    for (const auto& [key, entries] : listings_) {
        out.push_back(location_of(key));
    }
    std::sort(out.begin(), out.end(), [](const Location& a, const Location& b) {
        if (a.kind != b.kind) {
            return a.kind == Location::Kind::Ground;
        }
        if (a.kind == Location::Kind::Ground) {
            return a.cell.y != b.cell.y ? a.cell.y < b.cell.y : a.cell.x < b.cell.x;
        }
        return a.container != b.container ? a.container < b.container : a.compartment < b.compartment;
    });
    return out;
}

// ---------------------------------------------------------------- changes

Placed ItemStore::check_location(const Content& content, Location at) const noexcept {
    if (at.kind == Location::Kind::In) {
        if (thing(at.container) == nullptr) {
            return Placed::NoThing;
        }
        const std::uint32_t count = compartments(content, at.container);
        if (count == 0) {
            return Placed::NoContainer;
        }
        if (at.compartment >= count) {
            return Placed::NoCompartment;
        }
    }
    if (listing(at).size() >= params_.max_entries) {
        return Placed::Full;
    }
    return Placed::Ok;
}

std::uint32_t ItemStore::new_thing(const Thing& t) {
    std::uint32_t slot = 0;
    if (!free_things_.empty()) {
        slot = free_things_.back();
        free_things_.pop_back();
        things_[slot] = t;
    } else {
        slot = static_cast<std::uint32_t>(things_.size());
        things_.push_back(t);
    }
    thing_slot_.emplace(t.id.value, slot);
    return slot;
}

std::uint32_t ItemStore::new_line(const Line& line) {
    if (!free_lines_.empty()) {
        const std::uint32_t slot = free_lines_.back();
        free_lines_.pop_back();
        lines_[slot] = line;
        line_used_[slot] = 1;
        return slot;
    }
    lines_.push_back(line);
    line_used_.push_back(1);
    return static_cast<std::uint32_t>(lines_.size() - 1);
}

void ItemStore::erase_entry(Location at, Entry::Kind kind, std::uint32_t index) {
    const auto it = listings_.find(key_of(at));
    if (it == listings_.end()) {
        return;
    }
    std::vector<Entry>& entries = it->second;
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [&](const Entry& e) { return e.kind == kind && e.index == index; });
    if (found != entries.end()) {
        entries.erase(found);
    }
    if (entries.empty()) {
        listings_.erase(it);
    }
}

Placed ItemStore::place(const Content& content, ItemId archetype, Address address, Location at,
                        ThingId& out) {
    if (const Placed check = check_location(content, at); check != Placed::Ok) {
        return check;
    }
    const ThingId id{next_id_++};
    const std::uint32_t slot = new_thing({id, archetype, address, at});
    listings_[key_of(at)].push_back({Entry::Kind::Thing, slot});
    out = id;
    return Placed::Ok;
}

Placed ItemStore::place_line(const Content& content, ItemId archetype, Address address, std::uint32_t count,
                             std::uint16_t quality_mean, std::uint16_t quality_spread, Location at) {
    if (const Placed check = check_location(content, at); check != Placed::Ok) {
        return check;
    }
    const std::uint32_t slot = new_line({archetype, address, count, quality_mean, quality_spread, 0});
    listings_[key_of(at)].push_back({Entry::Kind::Line, slot});
    return Placed::Ok;
}

Placed ItemStore::move(const Content& content, ThingId id, Location to) {
    const auto found = thing_slot_.find(id.value);
    if (found == thing_slot_.end()) {
        return Placed::NoThing;
    }
    Thing& t = things_[found->second];
    if (t.at == to) {
        return Placed::Ok;
    }
    if (to.kind == Location::Kind::In) {
        // Up the chain of containers from the target: the thing moved must not be on it.
        for (ThingId c = to.container;;) {
            if (c == id) {
                return Placed::WouldNest;
            }
            const Thing* holder = thing(c);
            if (holder == nullptr || holder->at.kind == Location::Kind::Ground) {
                break;
            }
            c = holder->at.container;
        }
    }
    if (const Placed check = check_location(content, to); check != Placed::Ok) {
        return check;
    }
    const std::uint32_t slot = found->second;
    erase_entry(t.at, Entry::Kind::Thing, slot);
    t.at = to;
    listings_[key_of(to)].push_back({Entry::Kind::Thing, slot});
    return Placed::Ok;
}

bool ItemStore::remove(ThingId id) {
    const auto found = thing_slot_.find(id.value);
    if (found == thing_slot_.end()) {
        return false;
    }
    for (std::uint32_t c = 0; c < kMaxCompartments; ++c) {
        if (!in(id, static_cast<std::uint8_t>(c)).empty()) {
            return false;
        }
    }
    const std::uint32_t slot = found->second;
    erase_entry(things_[slot].at, Entry::Kind::Thing, slot);
    things_[slot] = Thing{};
    free_things_.push_back(slot);
    thing_slot_.erase(found);
    return true;
}

void ItemStore::grow(Location at, std::uint32_t index, std::uint32_t by) noexcept {
    const auto it = listings_.find(key_of(at));
    if (it == listings_.end() || index >= it->second.size() || it->second[index].kind != Entry::Kind::Line) {
        return;
    }
    Line& line = lines_[it->second[index].index];
    constexpr std::uint32_t kMax = std::numeric_limits<std::uint32_t>::max();
    line.count = by > kMax - line.count ? kMax : line.count + by;
}

void ItemStore::shrink(Location at, std::uint32_t index, std::uint32_t by) {
    const auto it = listings_.find(key_of(at));
    if (it == listings_.end() || index >= it->second.size() || it->second[index].kind != Entry::Kind::Line) {
        return;
    }
    const std::uint32_t slot = it->second[index].index;
    Line& line = lines_[slot];
    if (by < line.count) {
        line.count -= by;
        return;
    }
    it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
    if (it->second.empty()) {
        listings_.erase(it);
    }
    lines_[slot] = Line{};
    line_used_[slot] = 0;
    free_lines_.push_back(slot);
}

void ItemStore::rebuild(std::uint64_t next_id, std::span<const Thing> things,
                        std::span<const std::pair<Location, std::vector<SavedEntry>>> listings) {
    *this = ItemStore(params_);
    next_id_ = next_id;
    for (const Thing& t : things) {
        new_thing(t);
    }
    for (const auto& [location, entries] : listings) {
        std::vector<Entry>& out = listings_[key_of(location)];
        for (const SavedEntry& e : entries) {
            if (e.kind == Entry::Kind::Thing) {
                // World::restore checked every listed thing exists; an unknown one is skipped.
                const auto slot = thing_slot_.find(e.thing.value);
                if (slot != thing_slot_.end()) {
                    out.push_back({Entry::Kind::Thing, slot->second});
                }
            } else {
                out.push_back({Entry::Kind::Line, new_line(e.line)});
            }
        }
        if (out.empty()) {
            listings_.erase(key_of(location));
        }
    }
}

bool operator==(const ItemStore& a, const ItemStore& b) noexcept {
    if (a.next_id_ != b.next_id_ || a.thing_slot_.size() != b.thing_slot_.size() ||
        a.listings_.size() != b.listings_.size()) {
        return false;
    }
    for (const auto& [id, slot] : a.thing_slot_) {
        const Thing* other = b.thing(ThingId{id});
        const Thing& mine = a.things_[slot];
        if (other == nullptr || !(other->archetype == mine.archetype) || other->address != mine.address ||
            !(other->at == mine.at)) {
            return false;
        }
    }
    for (const auto& [key, entries] : a.listings_) {
        const auto it = b.listings_.find(key);
        if (it == b.listings_.end() || it->second.size() != entries.size()) {
            return false;
        }
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const Entry& x = entries[i];
            const Entry& y = it->second[i];
            if (x.kind != y.kind) {
                return false;
            }
            if (x.kind == Entry::Kind::Thing ? a.things_[x.index].id != b.things_[y.index].id
                                             : !(a.lines_[x.index] == b.lines_[y.index])) {
                return false;
            }
        }
    }
    return true;
}

} // namespace peo::core
