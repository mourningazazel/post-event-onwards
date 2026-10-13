// PEO-092: content migration on load (peo/core/content_migration.hpp).
#include "peo/core/content_migration.hpp"

#include <string_view>

namespace peo::core {

namespace {

/// The item `id` names when it is present and may spawn.
std::optional<ItemId> spawnable(const Content& content, std::string_view id) noexcept {
    const std::optional<ItemId> item = content.item(id);
    if (item && !content.abstract(*item)) {
        return item;
    }
    return std::nullopt;
}

/// Follow `id`'s remaps to a spawnable item. Each step names a new ID, so remap_count() steps
/// cover every chain without a cycle; a cycle, a target that is gone, or a present abstract
/// item ends the walk with nothing.
std::optional<ItemId> follow_remaps(const Content& content, std::string_view id) noexcept {
    std::string_view current = id;
    for (std::size_t step = 0; step < content.remap_count(); ++step) {
        const std::optional<std::string_view> next = content.remap(current);
        if (!next) {
            return std::nullopt;
        }
        current = *next;
        if (const auto item = spawnable(content, current)) {
            return item;
        }
        if (content.item(current)) {
            return std::nullopt; // present but abstract: a remap never passes through an item
        }
    }
    return std::nullopt;
}

} // namespace

Resolved resolve(const ContentRef& ref, const Content& content) noexcept {
    if (const auto item = spawnable(content, ref.id)) {
        return {item, Migration::Kept};
    }
    if (const auto item = follow_remaps(content, ref.id)) {
        return {item, Migration::Remapped};
    }
    for (const std::string& ancestor : ref.ancestry) {
        if (const auto item = spawnable(content, ancestor)) {
            return {item, Migration::StandIn};
        }
        if (const auto item = follow_remaps(content, ancestor)) {
            return {item, Migration::StandIn};
        }
    }
    return {content.item(kJunkItemId), Migration::Junk};
}

ContentRef content_ref(const Content& content, ItemId item) {
    ContentRef ref{std::string(content.item_id(item)), {}, std::string(content.name(item))};
    const auto chain = content.ancestry(item);
    ref.ancestry.reserve(chain.size());
    for (const ItemId up : chain) {
        ref.ancestry.emplace_back(content.item_id(up));
    }
    return ref;
}

std::string migration_summary(const MigrationReport& report) {
    // Placeholder wording (D-042): the owner writes the final text.
    std::string changed;
    std::size_t junk = 0;
    for (const MigrationReport::Line& line : report.lines) {
        if (line.to.how == Migration::Junk) {
            ++junk;
            continue;
        }
        if (!changed.empty()) {
            changed += "; ";
        }
        changed += line.old_name.empty() ? line.old_id : line.old_name;
        changed += " is now a ";
        changed += line.to_name;
    }
    if (junk > 0) {
        if (!changed.empty()) {
            changed += "; ";
        }
        changed += std::to_string(junk);
        changed += junk == 1 ? " old thing is now junk" : " old things are now junk";
    }
    if (changed.empty()) {
        return {};
    }
    return "Since this save: " + changed + ".";
}

} // namespace peo::core
