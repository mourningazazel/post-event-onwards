#pragma once

// PEO-121: what a thing shows from a distance, from content alone (D-049, D-046, D-030). Within
// 20 ft an item shows its family's generic label (a claw hammer reads as a hammer), a container
// only its own kind, and a sign its text; everything finer needs examining. The frontend shows
// the label's content `name` verbatim (D-042).

#include "peo/core/content.hpp"

#include <cstdint>

namespace peo::core {

enum class ThingKind : std::uint8_t { Item, Container, Sign };

/// What a thing reads as from a distance: the archetype whose name is shown, and its kind.
struct FarLabel {
    ItemId label;
    ThingKind kind;

    friend constexpr bool operator==(FarLabel, FarLabel) noexcept = default;
};

/// The far label of `archetype`, from its resolved content, in order:
/// 1. `features.readable.kind` is "sign": Sign, labelled as itself (its text is instance data);
/// 2. it has `features.container` or `compartments`: Container, labelled as the topmost
///    non-abstract item on its parent chain (itself when there is none);
/// 3. otherwise Item, labelled the same way (claw_hammer -> hammer, past an abstract root).
/// Never a compartment's name or purpose, a desc or contents. Reads Content::ancestry, so it
/// walks nothing; no allocation, no I/O.
[[nodiscard]] FarLabel far_label(const Content& content, ItemId archetype) noexcept;

} // namespace peo::core
