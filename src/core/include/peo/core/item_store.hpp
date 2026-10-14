#pragma once

// PEO-050: where things are (D-023, D-026, G03 D3.2 to D3.4). Every thing has exactly one
// location: a ground cell, or a compartment of a container thing, so nesting has no depth
// limit. A location lists its entries in order, each a Thing (an instantiated record with a
// permanent ID) or a Line (a nameless quantity of one archetype). Nothing here ticks: the store
// changes only when something acts on it. Reading through the seed and instantiating on take
// are PEO-139; this is the data seam.

#include "peo/core/content.hpp"
#include "peo/core/types.hpp"

#include <compare>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace peo::core {

/// A thing's permanent ID (ADR-0002 'Stable IDs'). 0 is none; IDs start at 1 and are never
/// reused.
struct ThingId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(ThingId, ThingId) noexcept = default;
    friend constexpr auto operator<=>(ThingId, ThingId) noexcept = default;
};

/// A generation address (G01 D1.5): given by generation or by hand, never interpreted here.
using Address = std::uint64_t;

/// Quality is in permille (D-029); PEO-013 may rescale.
inline constexpr std::uint16_t kQualityScale = 1000;

/// Hidden and far above play (D-023): never shown to the player. It refuses a new entry in a
/// location, never a line's growth.
inline constexpr std::uint32_t kMaxEntriesPerLocation = 4096;

/// Compartments a container may have: a compartment index is a byte.
inline constexpr std::uint32_t kMaxCompartments = 256;

struct Location {
    enum class Kind : std::uint8_t { Ground, In };
    Kind kind = Kind::Ground;
    /// Ground only.
    Vec2i cell{};
    /// In only: the container thing and which of its compartments.
    ThingId container{};
    std::uint8_t compartment = 0;

    [[nodiscard]] static Location ground(Vec2i cell) noexcept { return {Kind::Ground, cell, {}, 0}; }
    [[nodiscard]] static Location in(ThingId container, std::uint8_t compartment) noexcept {
        return {Kind::In, {}, container, compartment};
    }
    /// Equal when they name the same place (unused members are ignored).
    friend bool operator==(const Location& a, const Location& b) noexcept {
        if (a.kind != b.kind) {
            return false;
        }
        return a.kind == Kind::Ground ? a.cell == b.cell
                                      : a.container == b.container && a.compartment == b.compartment;
    }
};

struct Thing {
    ThingId id;
    ItemId archetype{};
    Address address = 0;
    Location at;
};

/// A nameless quantity of one archetype (D-026), with a mean quality and a spread (D-029).
struct Line {
    ItemId archetype{};
    Address address = 0;
    std::uint32_t count = 0;
    std::uint16_t quality_mean = 0;
    std::uint16_t quality_spread = 0;
    /// Units already instantiated from it (PEO-139 uses it for the next unit's address).
    std::uint32_t taken = 0;

    friend bool operator==(const Line&, const Line&) = default;
};

/// One entry of a listing: a thing or a line, by its index (thing_at, line_at).
struct Entry {
    enum class Kind : std::uint8_t { Thing, Line };
    Kind kind;
    std::uint32_t index;
};

struct ItemStoreParams {
    std::uint32_t max_entries = kMaxEntriesPerLocation;
};

/// Why a change was refused; on anything but Ok the store is unchanged.
enum class Placed : std::uint8_t {
    Ok,
    Full,          ///< the location already holds max_entries entries
    NoContainer,   ///< the container thing has no compartments and no container feature
    NoCompartment, ///< the compartment is past the container's count
    WouldNest,     ///< a thing into itself or into something inside it
    NoThing,       ///< no thing with that ID (the one named, or the container)
    OffStage,      ///< World only: a ground cell outside the stage
};

class ItemStore {
public:
    explicit ItemStore(ItemStoreParams params = {}) noexcept : params_(params) {}

    [[nodiscard]] const ItemStoreParams& params() const noexcept { return params_; }

    // -- changes; each changes nothing unless it returns Ok
    /// A new Thing at `at`, with the next ID, written to `out`.
    Placed place(const Content& content, ItemId archetype, Address address, Location at, ThingId& out);
    /// A new Line of `count` units at `at`.
    Placed place_line(const Content& content, ItemId archetype, Address address, std::uint32_t count,
                      std::uint16_t quality_mean, std::uint16_t quality_spread, Location at);
    /// Move a thing, and so everything inside it, to the end of `to`'s listing.
    Placed move(const Content& content, ThingId thing, Location to);
    /// Remove a thing; refuses one that still holds anything.
    bool remove(ThingId thing);
    /// Add to the line at `index` of `at`'s listing, saturating; never checks the cap (D-023).
    /// Does nothing when that entry is not a line.
    void grow(Location at, std::uint32_t index, std::uint32_t by) noexcept;
    /// Take from that line; at zero the line is removed.
    void shrink(Location at, std::uint32_t index, std::uint32_t by);

    // -- reads; none allocates
    /// The ground listing of `cell` (a container's contents are never on the cell).
    [[nodiscard]] std::span<const Entry> at(Vec2i cell) const noexcept;
    [[nodiscard]] std::span<const Entry> in(ThingId container, std::uint8_t compartment) const noexcept;
    [[nodiscard]] std::span<const Entry> listing(Location at) const noexcept;
    [[nodiscard]] const Thing* thing(ThingId id) const noexcept;
    [[nodiscard]] const Thing& thing_at(std::uint32_t index) const noexcept;
    [[nodiscard]] const Line& line_at(std::uint32_t index) const noexcept;
    /// How many compartments `container` has: its `compartments` array's length, 1 for
    /// features.container, else 0 (and 0 for no such thing).
    [[nodiscard]] std::uint32_t compartments(const Content& content, ThingId container) const noexcept;
    /// The same for an archetype, with no thing (restore checks a save's compartments with it).
    [[nodiscard]] static std::uint32_t compartments_of(const Content& content, ItemId archetype) noexcept;
    /// The ID the next placed thing gets.
    [[nodiscard]] ThingId next_id() const noexcept { return ThingId{next_id_}; }
    [[nodiscard]] std::size_t thing_count() const noexcept { return thing_slot_.size(); }
    [[nodiscard]] bool empty() const noexcept { return thing_slot_.empty() && listings_.empty(); }

    /// Every thing in ID order, and every non-empty listing in a fixed order (ground by row
    /// then column, then containers by ID and compartment): for saving. Allocates.
    [[nodiscard]] std::vector<ThingId> thing_ids() const;
    [[nodiscard]] std::vector<Location> locations() const;

    /// Rebuild from saved parts, trusting them (World::restore validates first): the next ID,
    /// the things, and each listing's entries as things by ID or lines inline.
    struct SavedEntry {
        Entry::Kind kind;
        ThingId thing;
        Line line;
    };
    void rebuild(std::uint64_t next_id, std::span<const Thing> things,
                 std::span<const std::pair<Location, std::vector<SavedEntry>>> listings);

    /// Same things (ID, archetype, address, location), same listings in the same order, same
    /// next ID; storage layout is ignored. No allocation.
    friend bool operator==(const ItemStore& a, const ItemStore& b) noexcept;

private:
    struct Key {
        std::uint64_t a; ///< ground: x and y; in: the container ID
        std::uint32_t b; ///< in: the compartment
        Location::Kind kind;
        friend bool operator==(const Key&, const Key&) = default;
    };
    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept;
    };
    [[nodiscard]] static Key key_of(Location at) noexcept;
    [[nodiscard]] static Location location_of(const Key& key) noexcept;

    Placed check_location(const Content& content, Location at) const noexcept;
    std::uint32_t new_thing(const Thing& thing);
    std::uint32_t new_line(const Line& line);
    void erase_entry(Location at, Entry::Kind kind, std::uint32_t index);

    ItemStoreParams params_;
    std::uint64_t next_id_ = 1;
    std::vector<Thing> things_; ///< a slot with id 0 is free
    std::vector<std::uint32_t> free_things_;
    std::vector<Line> lines_;
    std::vector<std::uint8_t> line_used_;
    std::vector<std::uint32_t> free_lines_;
    std::unordered_map<std::uint64_t, std::uint32_t>
        thing_slot_; ///< ID -> slot; looked up, never iterated for a result
    std::unordered_map<Key, std::vector<Entry>, KeyHash> listings_; ///< no empty listing is kept
};

} // namespace peo::core
