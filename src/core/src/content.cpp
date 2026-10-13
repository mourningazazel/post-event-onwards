// PEO-008: content load and resolve (peo/core/content.hpp), ported from tools/content/common.py
// load and tools/content/derive.py resolve_item; those stay the reference.
#include "peo/core/content.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <map>
#include <toml.hpp>
#include <utility>

namespace peo::core {

namespace {

// common.py RECORD_TYPES: tables of records by ID.
constexpr std::array<std::string_view, 30> kRecordTypes{
    "property",    "feature",     "capability", "stimulus", "namespace",    "tag",
    "fastener",    "scent",       "action",     "shape",    "skill",        "schema",
    "material",    "substance",   "item",       "modifier", "detail_table", "company",
    "brand",       "store_chain", "setting",    "building", "room",         "loot",
    "outdoor_set", "profile",     "purpose",    "detail",   "detail_loot",  "extras"};
// common.py MERGED_TYPES: single tables merged across files.
constexpr std::array<std::string_view, 4> kMergedTypes{"dominance", "meta", "status", "default"};

constexpr std::string_view kItemType = "item";
// Stands in for a `parent` value that is not a string; no ID can match it.
constexpr std::string_view kParentNotAString = "(not a string)";

template <std::size_t N>
bool listed(const std::array<std::string_view, N>& names, std::string_view name) noexcept {
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::string cat(std::initializer_list<std::string_view> parts) {
    std::string out;
    for (const std::string_view part : parts) {
        out += part;
    }
    return out;
}

/// common._merge and derive.deep_merge: a table over a table merges key by key; anything else
/// replaces what was there.
void merge_into(toml::table& into, const toml::table& from) {
    for (auto&& [key, node] : from) {
        if (const toml::table* over = node.as_table()) {
            if (toml::table* base = into.get_as<toml::table>(key.str())) {
                merge_into(*base, *over);
                continue;
            }
        }
        into.insert_or_assign(key, node);
    }
}

using Records = std::map<std::string, toml::table, std::less<>>;

/// The node a dotted path names under `root`: a part is a key in a table and a decimal index
/// in an array. Null when any step is missing.
const toml::node* at_path(const toml::table& root, std::string_view path) noexcept {
    const toml::node* node = &root;
    while (node != nullptr) {
        const std::size_t dot = path.find('.');
        const std::string_view part = path.substr(0, dot);
        if (const toml::table* table = node->as_table()) {
            node = table->get(part);
        } else if (const toml::array* array = node->as_array()) {
            std::size_t index = 0;
            const char* end = part.data() + part.size();
            const auto [stop, error] = std::from_chars(part.data(), end, index);
            node = (part.empty() || error != std::errc{} || stop != end) ? nullptr : array->get(index);
        } else {
            return nullptr;
        }
        if (dot == std::string_view::npos) {
            return node;
        }
        path.remove_prefix(dot + 1);
    }
    return nullptr;
}

} // namespace

struct Content::Impl {
    struct Item {
        std::string id;
        toml::table resolved;
        std::vector<std::string> tags;
        std::optional<ItemId> parent;
    };

    /// Sorted by ID; an ItemId is an index here.
    std::vector<Item> items;
    /// Every record type as authored (items too), by type then ID.
    std::map<std::string, Records, std::less<>> records;
    std::map<std::string, toml::table, std::less<>> merged;
};

/// The one place a Content is made and filled; Content's constructor and Impl stay private.
struct ContentLoader {
    using Impl = Content::Impl;
    static Content make(std::unique_ptr<Impl> impl) noexcept { return Content(std::move(impl)); }
    static void read_sources(std::span<const ContentSource> sources, Impl& impl,
                             std::vector<std::string>& errors);
    static void resolve_items(Impl& impl, std::vector<std::string>& errors);
};

Content::Content(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Content::Content(Content&&) noexcept = default;
Content& Content::operator=(Content&&) noexcept = default;
Content::~Content() = default;

std::optional<ItemId> Content::item(std::string_view id) const noexcept {
    const auto& items = impl_->items;
    const auto it =
        std::lower_bound(items.begin(), items.end(), id,
                         [](const Impl::Item& item, std::string_view key) { return item.id < key; });
    if (it == items.end() || it->id != id) {
        return std::nullopt;
    }
    return ItemId{static_cast<std::uint32_t>(it - items.begin())};
}

std::size_t Content::item_count() const noexcept {
    return impl_->items.size();
}

std::string_view Content::item_id(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    return impl_->items[item.index].id;
}

std::string_view Content::name(ItemId item) const noexcept {
    return string_at(item, "name").value_or(std::string_view{});
}

std::optional<ItemId> Content::parent(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    return impl_->items[item.index].parent;
}

bool Content::abstract(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    const toml::node* node = impl_->items[item.index].resolved.get("abstract");
    const auto* flag = node == nullptr ? nullptr : node->as_boolean();
    return flag != nullptr && flag->get();
}

std::span<const std::string> Content::tags(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    return impl_->items[item.index].tags;
}

bool Content::has(ItemId item, std::string_view dotted_path) const noexcept {
    assert(item.index < impl_->items.size());
    return at_path(impl_->items[item.index].resolved, dotted_path) != nullptr;
}

std::optional<std::int64_t> Content::int_at(ItemId item, std::string_view dotted_path) const noexcept {
    assert(item.index < impl_->items.size());
    const toml::node* node = at_path(impl_->items[item.index].resolved, dotted_path);
    const auto* value = node == nullptr ? nullptr : node->as_integer();
    if (value == nullptr) {
        return std::nullopt;
    }
    return value->get();
}

std::optional<std::string_view> Content::string_at(ItemId item, std::string_view dotted_path) const noexcept {
    assert(item.index < impl_->items.size());
    const toml::node* node = at_path(impl_->items[item.index].resolved, dotted_path);
    const auto* value = node == nullptr ? nullptr : node->as_string();
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string_view(value->get());
}

bool Content::has_record(std::string_view type, std::string_view id) const noexcept {
    const auto bucket = impl_->records.find(type);
    return bucket != impl_->records.end() && bucket->second.find(id) != bucket->second.end();
}

/// Read every source into `impl` as common.load does, appending its errors.
void ContentLoader::read_sources(std::span<const ContentSource> sources, Impl& impl,
                                 std::vector<std::string>& errors) {
    // Where each record was first seen, to name it in a duplicate error.
    std::map<std::string, std::map<std::string, std::string_view, std::less<>>, std::less<>> origin;
    for (const ContentSource& source : sources) {
        toml::parse_result parsed = toml::parse(source.text, source.path);
        if (!parsed) {
            const toml::parse_error& error = parsed.error();
            const toml::source_position at = error.source().begin;
            errors.push_back(cat({source.path, ": TOML parse error: ", error.description(), " (at line ",
                                  std::to_string(at.line), ", column ", std::to_string(at.column), ")"}));
            continue;
        }
        for (auto&& [key, node] : parsed.table()) {
            const std::string_view type = key.str();
            if (listed(kMergedTypes, type)) {
                if (const toml::table* table = node.as_table()) {
                    merge_into(impl.merged[std::string(type)], *table);
                }
                continue;
            }
            if (!listed(kRecordTypes, type)) {
                errors.push_back(cat({source.path, ": unknown record type [", type, ".*]"}));
                continue;
            }
            toml::table* records = node.as_table();
            if (records == nullptr) {
                errors.push_back(cat({source.path, ": [", type, "] must be a table of records"}));
                continue;
            }
            Records& bucket = impl.records[std::string(type)];
            auto& first_in = origin[std::string(type)];
            for (auto&& [rid, rec] : *records) {
                const std::string_view id = rid.str();
                toml::table* record = rec.as_table();
                if (record == nullptr) {
                    errors.push_back(cat({source.path, ": ", type, ".", id, " is not a table"}));
                    continue;
                }
                if (const auto seen = first_in.find(id); seen != first_in.end()) {
                    errors.push_back(
                        cat({source.path, ": duplicate ", type, ".", id, " (first in ", seen->second, ")"}));
                    continue;
                }
                first_in.emplace(std::string(id), source.path);
                bucket.emplace(std::string(id), std::move(*record));
            }
        }
    }
}

/// derive.resolve_item over every item, in ID order. Each item resolves once: a parent before
/// its children, so a child merges over its parent's resolved table.
void ContentLoader::resolve_items(Impl& impl, std::vector<std::string>& errors) {
    const auto found = impl.records.find(kItemType);
    if (found == impl.records.end()) {
        return;
    }
    const Records& authored = found->second;
    std::vector<const toml::table*> sources;
    impl.items.reserve(authored.size());
    sources.reserve(authored.size());
    for (const auto& [id, record] : authored) {
        impl.items.push_back({id, {}, {}, std::nullopt});
        sources.push_back(&record);
    }
    // impl.items is in the map's order, so sorted by ID.
    const auto index_of = [&impl](std::string_view id) -> std::optional<std::uint32_t> {
        const auto& items = impl.items;
        const auto it =
            std::lower_bound(items.begin(), items.end(), id,
                             [](const Impl::Item& item, std::string_view key) { return item.id < key; });
        if (it == items.end() || it->id != id) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(it - items.begin());
    };
    const auto parent_id = [&sources](std::uint32_t index) -> std::string_view {
        const toml::node* parent = sources[index]->get("parent");
        if (parent == nullptr) {
            return {};
        }
        // resolve_item: `if parent:`, so an empty string is no parent. A parent that is not a
        // string names no item, so it is reported as unknown.
        const auto* name = parent->as_string();
        return name == nullptr ? kParentNotAString : std::string_view(name->get());
    };

    std::vector<bool> done(impl.items.size(), false);
    // Ancestors of each resolved item, so a walk that stops at one still counts the whole chain.
    std::vector<std::uint32_t> depth(impl.items.size(), 0);
    std::vector<std::uint32_t> chain;
    chain.reserve(kMaxItemAncestry + 2);
    for (std::uint32_t start = 0; start < impl.items.size(); ++start) {
        if (done[start]) {
            continue;
        }
        // Walk up to the first resolved ancestor or the root, checking the chain.
        chain.assign(1, start);
        const auto chain_text = [&](std::string_view tail) {
            std::string text;
            for (const std::uint32_t link : chain) {
                text += impl.items[link].id;
                text += " -> ";
            }
            text += tail;
            return text;
        };
        bool ok = true;
        while (!done[chain.back()]) {
            const std::string_view parent = parent_id(chain.back());
            if (parent.empty()) {
                break;
            }
            const std::optional<std::uint32_t> next = index_of(parent);
            const std::string_view id = impl.items[start].id;
            if (!next) {
                errors.push_back(
                    cat({"item.", id, ": unknown parent '", parent, "' (", chain_text(parent), ")"}));
                ok = false;
                break;
            }
            if (std::find(chain.begin(), chain.end(), *next) != chain.end()) {
                errors.push_back(cat({"item.", id, ": inheritance cycle: ", chain_text(parent)}));
                ok = false;
                break;
            }
            chain.push_back(*next);
            // A walk stopping at a resolved ancestor adds that ancestor's own depth.
            const std::size_t ancestors = chain.size() - 1 + (done[chain.back()] ? depth[chain.back()] : 0);
            if (ancestors > kMaxItemAncestry) {
                errors.push_back(cat({"item.", id, ": parent chain longer than ",
                                      std::to_string(kMaxItemAncestry), ": ", chain_text("...")}));
                ok = false;
                break;
            }
        }
        if (!ok) {
            continue;
        }
        // Resolve from the top of the chain down.
        for (auto link = chain.rbegin(); link != chain.rend(); ++link) {
            const std::uint32_t index = *link;
            if (done[index]) {
                continue;
            }
            Impl::Item& item = impl.items[index];
            const toml::table& own = *sources[index];
            const std::string_view parent = parent_id(index);
            if (parent.empty()) {
                item.resolved = own;
            } else {
                const std::uint32_t up = *index_of(parent);
                item.parent = ItemId{up};
                depth[index] = depth[up] + 1;
                item.resolved = impl.items[up].resolved;
                item.resolved.erase("abstract");
                merge_into(item.resolved, own);
                // Tags accumulate through inheritance, parent first, first occurrence kept.
                std::vector<std::string_view> tags;
                const auto add = [&tags](const toml::table& table) {
                    if (const toml::array* list = table.get_as<toml::array>("tags")) {
                        for (const toml::node& tag : *list) {
                            if (const auto* text = tag.as_string();
                                text != nullptr &&
                                std::find(tags.begin(), tags.end(), text->get()) == tags.end()) {
                                tags.push_back(text->get());
                            }
                        }
                    }
                };
                add(impl.items[up].resolved);
                add(own);
                toml::array merged;
                for (const std::string_view tag : tags) {
                    merged.push_back(std::string(tag));
                }
                item.resolved.insert_or_assign("tags", std::move(merged));
            }
            item.resolved.insert_or_assign("id", item.id);
            done[index] = true;
        }
    }
    if (!errors.empty()) {
        return;
    }
    for (Impl::Item& item : impl.items) {
        if (const toml::array* list = item.resolved.get_as<toml::array>("tags")) {
            for (const toml::node& tag : *list) {
                if (const auto* text = tag.as_string()) {
                    item.tags.push_back(text->get());
                }
            }
        }
    }
}

ContentLoad load_content(std::span<const ContentSource> sources) {
    auto impl = std::make_unique<ContentLoader::Impl>();
    ContentLoad load;
    ContentLoader::read_sources(sources, *impl, load.errors);
    if (load.errors.empty()) {
        ContentLoader::resolve_items(*impl, load.errors);
    }
    if (load.errors.empty()) {
        load.content = ContentLoader::make(std::move(impl));
    }
    return load;
}

} // namespace peo::core
