#pragma once

// PEO-092: content migration on load (ADR-0017 decision 2, D-046). A save remembers each item
// archetype it refers to as a ContentRef: its ID, its parent chain at save time and its base
// name. On restore each resolves once against current content, in order: still there (kept), a
// remap to its new ID, the nearest spawnable ancestor (a stand-in), else junk. It never refuses
// a save, and it is a pure function of the record and the content.

#include "peo/core/content.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace peo::core {

/// What a save remembers of one item archetype. The ancestry is kept because a removed item's
/// chain is no longer in content: without it a removal could only ever become junk.
struct ContentRef {
    std::string id;
    /// Its parent chain at save time, nearest first.
    std::vector<std::string> ancestry;
    /// Its base name at save time, so junk can still say what it was.
    std::string name;

    friend bool operator==(const ContentRef&, const ContentRef&) = default;
};

/// What a save's CDIC entry may hold; anything more is Corrupt. A save is untrusted input, so
/// it has limits content does not: 512 ancestors (owner, 2026-10-12), far past any real family.
inline constexpr std::size_t kMaxSavedAncestry = 512;
inline constexpr std::size_t kMaxContentIdBytes = 64;
inline constexpr std::size_t kMaxContentNameBytes = 128;

enum class Migration : std::uint8_t { Kept, Remapped, StandIn, Junk };

/// Where a ContentRef lands in current content, and how.
struct Resolved {
    /// The current archetype. Nullopt only for Junk when content has no kJunkItemId (test
    /// content; lint requires it in content/).
    std::optional<ItemId> item;
    Migration how = Migration::Junk;

    friend bool operator==(const Resolved&, const Resolved&) = default;
};

/// Resolve `ref` against `content` (ADR-0017 order):
/// 1. its ID is present and not abstract: Kept;
/// 2. its remaps, followed one to the next (at most remap_count() steps, so a cycle ends), reach
///    a present, non-abstract item: Remapped;
/// 3. the first ancestor, nearest first, that is present and not abstract, or whose remaps reach
///    one (so a renamed parent still counts): StandIn;
/// 4. the junk archetype: Junk.
/// No allocation and no I/O; the same inputs give the same result.
[[nodiscard]] Resolved resolve(const ContentRef& ref, const Content& content) noexcept;

/// What a save records for `item`: its ID, its parent chain from Content::ancestry and its base
/// name, so World::save never assembles a chain by hand.
[[nodiscard]] ContentRef content_ref(const Content& content, ItemId item);

/// Every save entry that did not stay as it was, for the player to be told once.
struct MigrationReport {
    struct Line {
        std::string old_id;
        std::string old_name;
        Resolved to;
        /// The base name of what it became (empty for junk with no junk archetype), so the
        /// summary needs no content.
        std::string to_name;
        /// How many references in the save point at this entry (zero until items exist).
        std::uint32_t references = 0;
    };
    /// In dictionary order (the save's CDIC order); Kept entries are not listed.
    std::vector<Line> lines;
};

/// The one message the player is told, e.g. "Since this save: claw hammer is now a hammer; 2
/// old things are now junk." Empty when nothing changed. Placeholder wording: the owner writes
/// the final text (D-042).
[[nodiscard]] std::string migration_summary(const MigrationReport& report);

} // namespace peo::core
