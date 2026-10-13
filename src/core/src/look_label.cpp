// PEO-121: what a thing shows from a distance (peo/core/look_label.hpp).
#include "peo/core/look_label.hpp"

#include <string_view>

namespace peo::core {

namespace {

constexpr std::string_view kReadableKind = "features.readable.kind";
constexpr std::string_view kSignKind = "sign";
constexpr std::string_view kContainer = "features.container";
constexpr std::string_view kCompartments = "compartments";

/// The topmost non-abstract item on `item`'s chain, or `item` when every ancestor is abstract
/// or it has none. The chain is nearest first, so the top is its end.
ItemId generic_label(const Content& content, ItemId item) noexcept {
    const auto chain = content.ancestry(item);
    for (auto up = chain.rbegin(); up != chain.rend(); ++up) {
        if (!content.abstract(*up)) {
            return *up;
        }
    }
    return item;
}

} // namespace

FarLabel far_label(const Content& content, ItemId archetype) noexcept {
    if (content.string_at(archetype, kReadableKind) == kSignKind) {
        return {archetype, ThingKind::Sign};
    }
    if (content.has(archetype, kContainer) || content.has(archetype, kCompartments)) {
        return {generic_label(content, archetype), ThingKind::Container};
    }
    return {generic_label(content, archetype), ThingKind::Item};
}

} // namespace peo::core
