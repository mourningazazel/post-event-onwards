// PEO-008, PEO-142: content load and resolve (peo/core/content.hpp), ported from
// tools/content/common.py load, tools/content/peo_load.py and tools/content/derive.py
// resolve_item; those stay the reference. Records live as PeoValue trees whatever the source:
// .peo sources come from parse_peo, TOML sources through a small adapter that PEO-143 deletes.
#include "peo/core/content.hpp"

#include "peo/core/peo_format.hpp"

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
constexpr std::array<std::string_view, 31> kRecordTypes{
    "property",    "feature",  "capability",   "stimulus",    "namespace", "tag",         "fastener",
    "scent",       "action",   "shape",        "skill",       "schema",    "material",    "substance",
    "item",        "modifier", "detail_table", "company",     "brand",     "store_chain", "setting",
    "building",    "room",     "loot",         "outdoor_set", "profile",   "purpose",     "detail",
    "detail_loot", "extras",   "remap"};
// common.py MERGED_TYPES: single tables merged across files.
constexpr std::array<std::string_view, 4> kMergedTypes{"dominance", "meta", "status", "default"};

// The path table of docs/design/content-format.md 'Converting from TOML' (PEO-136): a record
// type's path prefix. An item is any other path under kItemRoot.
struct PathPrefix {
    std::string_view prefix;
    std::string_view type;
};
constexpr std::array<PathPrefix, 30> kPathPrefixes{{
    {"item.material.", "material"},
    {"substance.", "substance"},
    {"def.property.", "property"},
    {"def.feature.", "feature"},
    {"def.capability.", "capability"},
    {"def.stimulus.", "stimulus"},
    {"def.namespace.", "namespace"},
    {"def.fastener.", "fastener"},
    {"def.scent.", "scent"},
    {"def.shape.", "shape"},
    {"def.schema.", "schema"},
    {"def.tag.", "tag"},
    {"mechanic.action.", "action"},
    {"mechanic.skill.", "skill"},
    {"mechanic.modifier.", "modifier"},
    {"mechanic.detail_table.", "detail_table"},
    {"world.setting.", "setting"},
    {"world.building.", "building"},
    {"world.room.", "room"},
    {"world.purpose.", "purpose"},
    {"world.loot.", "loot"},
    {"world.extras.", "extras"},
    {"world.outdoor_set.", "outdoor_set"},
    {"world.detail.", "detail"},
    {"world.detail_loot.", "detail_loot"},
    {"brand.company.", "company"},
    {"brand.brand.", "brand"},
    {"brand.store_chain.", "store_chain"},
    {"unit.human.", "profile"},
    {"remap.item.", "remap"},
}};
constexpr std::array<PathPrefix, 4> kMergedPaths{{
    {"def.meta", "meta"},
    {"def.fastener_status", "status"},
    {"def.scent_dominance", "dominance"},
    {"mechanic.reaction.default", "default"},
}};
constexpr std::string_view kItemRoot = "item";
constexpr std::string_view kPeoSuffix = ".peo";

constexpr std::string_view kItemType = "item";
constexpr std::string_view kRemapType = "remap";
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

/// The record type and flat ID a .peo header path names (longest prefix wins, so
/// item.material.x is a material), or nullopt for a path under no root of the table.
std::optional<std::pair<std::string_view, std::string_view>> record_type(std::string_view path) noexcept {
    const PathPrefix* best = nullptr;
    for (const PathPrefix& p : kPathPrefixes) {
        if (path.starts_with(p.prefix) && (best == nullptr || p.prefix.size() > best->prefix.size())) {
            best = &p;
        }
    }
    if (best != nullptr) {
        return std::pair{best->type, path.substr(best->prefix.size())};
    }
    if (path.starts_with(kItemRoot) && path.size() > kItemRoot.size() && path[kItemRoot.size()] == '.') {
        return std::pair{kItemType, path.substr(path.rfind('.') + 1)};
    }
    return std::nullopt;
}

PeoValue string_value(std::string text) {
    PeoValue v;
    v.kind = PeoKind::String;
    v.text = std::move(text);
    return v;
}

const PeoValue* field(const PeoValue& table, std::string_view key) noexcept {
    for (const PeoField& f : table.fields) {
        if (f.key == key) {
            return &f.value;
        }
    }
    return nullptr;
}

PeoValue* field(PeoValue& table, std::string_view key) noexcept {
    for (PeoField& f : table.fields) {
        if (f.key == key) {
            return &f.value;
        }
    }
    return nullptr;
}

void set_field(PeoValue& table, std::string_view key, PeoValue value) {
    if (PeoValue* existing = field(table, key)) {
        *existing = std::move(value);
    } else {
        table.fields.push_back({std::string(key), std::move(value)});
    }
}

void erase_field(PeoValue& table, std::string_view key) {
    std::erase_if(table.fields, [&](const PeoField& f) { return f.key == key; });
}

/// common._merge and derive.deep_merge: a block over a block merges key by key; anything else
/// replaces what was there.
void merge_into(PeoValue& into, const PeoValue& from) {
    for (const PeoField& f : from.fields) {
        if (f.value.kind == PeoKind::Block) {
            if (PeoValue* base = field(into, f.key); base != nullptr && base->kind == PeoKind::Block) {
                merge_into(*base, f.value);
                continue;
            }
        }
        set_field(into, f.key, f.value);
    }
}

/// The text a value reads as (a String, or a Path's full path, or an Expr), else null.
const std::string* text_of(const PeoValue& v) noexcept {
    return (v.kind == PeoKind::String || v.kind == PeoKind::Path || v.kind == PeoKind::Expr) ? &v.text
                                                                                             : nullptr;
}

/// A TOML value as a PeoValue tree (PEO-143 deletes this with toml++).
std::optional<PeoValue> from_toml(const toml::node& node) {
    PeoValue v;
    if (const toml::table* table = node.as_table()) {
        v.kind = PeoKind::Block;
        for (auto&& [key, child] : *table) {
            std::optional<PeoValue> sub = from_toml(child);
            if (!sub) {
                return std::nullopt;
            }
            v.fields.push_back({std::string(key.str()), std::move(*sub)});
        }
    } else if (const toml::array* array = node.as_array()) {
        v.kind = PeoKind::List;
        for (const toml::node& child : *array) {
            std::optional<PeoValue> sub = from_toml(child);
            if (!sub) {
                return std::nullopt;
            }
            v.items.push_back(std::move(*sub));
        }
    } else if (const auto* integer = node.as_integer()) {
        v.kind = PeoKind::Integer;
        v.number = integer->get();
    } else if (const auto* boolean = node.as_boolean()) {
        v.kind = PeoKind::Boolean;
        v.boolean = boolean->get();
    } else if (const auto* text = node.as_string()) {
        v.kind = PeoKind::String;
        v.text = text->get();
    } else if (const auto* floating = node.as_floating_point()) {
        v.kind = PeoKind::Float;
        v.floating = floating->get();
    } else {
        return std::nullopt; // dates and times: content has none
    }
    return v;
}

using Records = std::map<std::string, PeoValue, std::less<>>;

/// What a dotted path names under a resolved item: a node, or one end of a range.
struct Found {
    const PeoValue* node = nullptr;
    int range_end = -1; ///< 0 or 1 when the path ends on a range's low or high end
};

/// A part is a key in a block and a decimal index in a list (and 0 or 1 on a range).
Found at_path(const PeoValue& root, std::string_view path) noexcept {
    const PeoValue* node = &root;
    while (true) {
        const std::size_t dot = path.find('.');
        const std::string_view part = path.substr(0, dot);
        std::size_t index = 0;
        const char* end = part.data() + part.size();
        const auto [stop, error] = std::from_chars(part.data(), end, index);
        const bool is_index = !part.empty() && error == std::errc{} && stop == end;
        if (node->kind == PeoKind::Block) {
            node = field(*node, part);
        } else if (node->kind == PeoKind::List) {
            node = is_index && index < node->items.size() ? &node->items[index] : nullptr;
        } else if (node->kind == PeoKind::Range && is_index && index < 2 && dot == std::string_view::npos) {
            return {node, static_cast<int>(index)};
        } else {
            return {};
        }
        if (node == nullptr) {
            return {};
        }
        if (dot == std::string_view::npos) {
            return {node, -1};
        }
        path.remove_prefix(dot + 1);
    }
}

} // namespace

struct Content::Impl {
    struct Item {
        std::string id;
        PeoValue resolved;
        std::vector<std::string> tags;
        std::optional<ItemId> parent;
        /// Its parent chain in `ancestry`, nearest first.
        std::uint32_t ancestry_begin = 0;
        std::uint32_t ancestry_count = 0;
    };

    /// Sorted by ID; an ItemId is an index here.
    std::vector<Item> items;
    /// Every item's parent chain, one after another.
    std::vector<ItemId> ancestry;
    /// Every record type as authored (items too), by type then flat ID.
    std::map<std::string, Records, std::less<>> records;
    std::map<std::string, PeoValue, std::less<>> merged;
    /// Each remap's `to` as a flat ID (a .peo `to` is a path), by the remap's flat ID.
    std::map<std::string, std::string, std::less<>> remap_to;
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

std::span<const ItemId> Content::ancestry(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    const Impl::Item& entry = impl_->items[item.index];
    return std::span<const ItemId>(impl_->ancestry).subspan(entry.ancestry_begin, entry.ancestry_count);
}

bool Content::abstract(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    const PeoValue* flag = field(impl_->items[item.index].resolved, "abstract");
    return flag != nullptr && flag->kind == PeoKind::Boolean && flag->boolean;
}

std::span<const std::string> Content::tags(ItemId item) const noexcept {
    assert(item.index < impl_->items.size());
    return impl_->items[item.index].tags;
}

bool Content::has(ItemId item, std::string_view dotted_path) const noexcept {
    assert(item.index < impl_->items.size());
    return at_path(impl_->items[item.index].resolved, dotted_path).node != nullptr;
}

std::optional<std::int64_t> Content::int_at(ItemId item, std::string_view dotted_path) const noexcept {
    assert(item.index < impl_->items.size());
    const Found found = at_path(impl_->items[item.index].resolved, dotted_path);
    if (found.node == nullptr) {
        return std::nullopt;
    }
    if (found.range_end >= 0) {
        return found.range_end == 0 ? found.node->number : found.node->high;
    }
    if (found.node->kind == PeoKind::Integer || found.node->kind == PeoKind::Quantity) {
        return found.node->number;
    }
    return std::nullopt;
}

std::optional<std::string_view> Content::string_at(ItemId item, std::string_view dotted_path) const noexcept {
    assert(item.index < impl_->items.size());
    const Found found = at_path(impl_->items[item.index].resolved, dotted_path);
    if (found.node == nullptr || found.range_end >= 0) {
        return std::nullopt;
    }
    const std::string* text = text_of(*found.node);
    if (text == nullptr) {
        return std::nullopt;
    }
    return std::string_view(*text);
}

std::size_t Content::remap_count() const noexcept {
    const auto bucket = impl_->records.find(kRemapType);
    return bucket == impl_->records.end() ? 0 : bucket->second.size();
}

std::optional<std::string_view> Content::remap(std::string_view old_id) const noexcept {
    const auto found = impl_->remap_to.find(old_id);
    if (found == impl_->remap_to.end()) {
        return std::nullopt;
    }
    return std::string_view(found->second);
}

bool Content::has_record(std::string_view type, std::string_view id) const noexcept {
    const auto bucket = impl_->records.find(type);
    return bucket != impl_->records.end() && bucket->second.find(id) != bucket->second.end();
}

namespace {

void emit(std::string_view path, ContentLeaf::Kind kind, const std::function<void(const ContentLeaf&)>& visit,
          std::int64_t integer = 0, bool boolean = false, double floating = 0.0, std::string_view text = {}) {
    visit(ContentLeaf{path, kind, integer, boolean, floating, text});
}

/// Visit every scalar under `node`, `path` holding the dotted path to it (restored on return).
/// Keys in sorted order and lists by index; a range is two integers, `.0` and `.1`.
void walk_leaves(const PeoValue& node, std::string& path,
                 const std::function<void(const ContentLeaf&)>& visit) {
    const auto descend = [&](std::string_view part, const auto& go) {
        const std::size_t mark = path.size();
        if (!path.empty()) {
            path += '.';
        }
        path += part;
        go();
        path.resize(mark);
    };
    switch (node.kind) {
    case PeoKind::Block: {
        std::vector<const PeoField*> sorted;
        sorted.reserve(node.fields.size());
        for (const PeoField& f : node.fields) {
            sorted.push_back(&f);
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const PeoField* a, const PeoField* b) { return a->key < b->key; });
        for (const PeoField* f : sorted) {
            descend(f->key, [&] { walk_leaves(f->value, path, visit); });
        }
        return;
    }
    case PeoKind::List:
        for (std::size_t i = 0; i < node.items.size(); ++i) {
            descend(std::to_string(i), [&] { walk_leaves(node.items[i], path, visit); });
        }
        return;
    case PeoKind::Range:
        descend("0", [&] { emit(path, ContentLeaf::Kind::Integer, visit, node.number); });
        descend("1", [&] { emit(path, ContentLeaf::Kind::Integer, visit, node.high); });
        return;
    case PeoKind::Integer:
    case PeoKind::Quantity:
        emit(path, ContentLeaf::Kind::Integer, visit, node.number);
        return;
    case PeoKind::Boolean:
        emit(path, ContentLeaf::Kind::Boolean, visit, 0, node.boolean);
        return;
    case PeoKind::Float:
        emit(path, ContentLeaf::Kind::Float, visit, 0, false, node.floating);
        return;
    case PeoKind::String:
    case PeoKind::Path:
    case PeoKind::Expr:
        emit(path, ContentLeaf::Kind::String, visit, 0, false, 0.0, node.text);
        return;
    }
}

} // namespace

void Content::for_each_leaf(ItemId item, const std::function<void(const ContentLeaf&)>& visit) const {
    assert(item.index < impl_->items.size());
    std::string path;
    walk_leaves(impl_->items[item.index].resolved, path, visit);
}

namespace {

/// A record ready to store: its type, flat ID, value and where it came from.
struct Incoming {
    std::string type;
    std::string id;
    PeoValue value;
};

} // namespace

/// Read every source into `impl` as common.load and peo_load do, appending their errors.
void ContentLoader::read_sources(std::span<const ContentSource> sources, Impl& impl,
                                 std::vector<std::string>& errors) {
    // Where each record was first seen, to name it in a duplicate error, whatever its format.
    std::map<std::string, std::map<std::string, std::string_view, std::less<>>, std::less<>> origin;
    const auto store = [&](const ContentSource& source, Incoming rec) {
        auto& first_in = origin[rec.type];
        if (const auto seen = first_in.find(rec.id); seen != first_in.end()) {
            errors.push_back(
                cat({source.path, ": duplicate ", rec.type, ".", rec.id, " (first in ", seen->second, ")"}));
            return;
        }
        first_in.emplace(rec.id, source.path);
        impl.records[rec.type].emplace(rec.id, std::move(rec.value));
    };

    // .peo items are parented by their header paths, so every source is parsed before any is read.
    std::vector<std::pair<const ContentSource*, PeoParse>> peo;
    std::map<std::string, std::string, std::less<>> item_paths; // .peo item path -> flat ID
    for (const ContentSource& source : sources) {
        if (!std::string_view(source.path).ends_with(kPeoSuffix)) {
            continue;
        }
        PeoParse parsed = parse_peo(source);
        errors.insert(errors.end(), parsed.errors.begin(), parsed.errors.end());
        for (const PeoRecord& rec : parsed.records) {
            if (const auto kind = record_type(rec.path); kind && kind->first == kItemType) {
                item_paths.emplace(rec.path, std::string(kind->second));
            }
        }
        peo.emplace_back(&source, std::move(parsed));
    }

    std::size_t next_peo = 0;
    for (const ContentSource& source : sources) {
        if (std::string_view(source.path).ends_with(kPeoSuffix)) {
            for (PeoRecord& rec : peo[next_peo].second.records) {
                const std::string where = cat({source.path, ":", std::to_string(rec.line)});
                if (const auto merged =
                        std::find_if(kMergedPaths.begin(), kMergedPaths.end(),
                                     [&](const PathPrefix& m) { return m.prefix == rec.path; });
                    merged != kMergedPaths.end()) {
                    if (rec.value.kind == PeoKind::Block) {
                        PeoValue& into = impl.merged[std::string(merged->type)];
                        into.kind = PeoKind::Block;
                        merge_into(into, rec.value);
                    }
                    continue;
                }
                const auto kind = record_type(rec.path);
                if (!kind) {
                    errors.push_back(
                        cat({where, ": record '", rec.path, "' is under no root in the path table"}));
                    continue;
                }
                if (rec.value.kind != PeoKind::Block) {
                    errors.push_back(cat({where, ": ", rec.path, " is not a block"}));
                    continue;
                }
                Incoming in{std::string(kind->first), std::string(kind->second), std::move(rec.value)};
                if (in.type == kItemType) {
                    // As peo_load: the parent's flat ID and the abstract flag become fields, so
                    // resolution reads them as it reads TOML's.
                    const std::string_view up = std::string_view(rec.path).substr(0, rec.path.rfind('.'));
                    if (up != kItemRoot) {
                        const auto parent = item_paths.find(up);
                        if (parent == item_paths.end()) {
                            errors.push_back(
                                cat({where, ": ", rec.path, ": its parent path '", up, "' names no item"}));
                            continue;
                        }
                        set_field(in.value, "parent", string_value(parent->second));
                    }
                    if (rec.abstract) {
                        PeoValue flag;
                        flag.kind = PeoKind::Boolean;
                        flag.boolean = true;
                        set_field(in.value, "abstract", std::move(flag));
                    }
                }
                store(source, std::move(in));
            }
            ++next_peo;
            continue;
        }

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
                    if (std::optional<PeoValue> value = from_toml(*table)) {
                        PeoValue& into = impl.merged[std::string(type)];
                        into.kind = PeoKind::Block;
                        merge_into(into, *value);
                    }
                }
                continue;
            }
            if (!listed(kRecordTypes, type)) {
                errors.push_back(cat({source.path, ": unknown record type [", type, ".*]"}));
                continue;
            }
            const toml::table* records = node.as_table();
            if (records == nullptr) {
                errors.push_back(cat({source.path, ": [", type, "] must be a table of records"}));
                continue;
            }
            for (auto&& [rid, rec] : *records) {
                const std::string_view id = rid.str();
                if (rec.as_table() == nullptr) {
                    errors.push_back(cat({source.path, ": ", type, ".", id, " is not a table"}));
                    continue;
                }
                std::optional<PeoValue> value = from_toml(rec);
                if (!value) {
                    errors.push_back(cat({source.path, ": ", type, ".", id, " holds a date or time"}));
                    continue;
                }
                store(source, Incoming{std::string(type), std::string(id), std::move(*value)});
            }
        }
    }

    // Each remap's target as a flat ID: a string as written, a path through the path table.
    if (const auto remaps = impl.records.find(kRemapType); remaps != impl.records.end()) {
        for (const auto& [old_id, rec] : remaps->second) {
            const PeoValue* to = field(rec, "to");
            if (to == nullptr) {
                continue;
            }
            if (to->kind == PeoKind::String) {
                impl.remap_to.emplace(old_id, to->text);
            } else if (to->kind == PeoKind::Path) {
                if (const auto kind = record_type(to->text)) {
                    impl.remap_to.emplace(old_id, std::string(kind->second));
                }
            }
        }
    }
}

/// derive.resolve_item over every item, in ID order. Each item resolves once: a parent before
/// its children, so a child merges over its parent's resolved block.
void ContentLoader::resolve_items(Impl& impl, std::vector<std::string>& errors) {
    const auto found = impl.records.find(kItemType);
    if (found == impl.records.end()) {
        return;
    }
    const Records& authored = found->second;
    std::vector<const PeoValue*> sources;
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
        const PeoValue* parent = field(*sources[index], "parent");
        if (parent == nullptr) {
            return {};
        }
        // resolve_item: `if parent:`, so an empty string is no parent. A parent that is not a
        // string names no item, so it is reported as unknown.
        return parent->kind == PeoKind::String ? std::string_view(parent->text) : kParentNotAString;
    };

    std::vector<bool> done(impl.items.size(), false);
    std::vector<std::uint32_t> chain;
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
            const PeoValue& own = *sources[index];
            const std::string_view parent = parent_id(index);
            if (parent.empty()) {
                item.resolved = own;
            } else {
                const std::uint32_t up = *index_of(parent);
                item.parent = ItemId{up};
                item.resolved = impl.items[up].resolved;
                erase_field(item.resolved, "abstract");
                merge_into(item.resolved, own);
                // Tags accumulate through inheritance, parent first, first occurrence kept.
                PeoValue tags;
                tags.kind = PeoKind::List;
                const auto add = [&tags](const PeoValue& table) {
                    const PeoValue* list = field(table, "tags");
                    if (list == nullptr || list->kind != PeoKind::List) {
                        return;
                    }
                    for (const PeoValue& tag : list->items) {
                        const std::string* text = text_of(tag);
                        if (text != nullptr &&
                            std::none_of(tags.items.begin(), tags.items.end(),
                                         [&](const PeoValue& seen) { return seen.text == *text; })) {
                            tags.items.push_back(tag);
                        }
                    }
                };
                add(impl.items[up].resolved);
                add(own);
                set_field(item.resolved, "tags", std::move(tags));
            }
            set_field(item.resolved, "id", string_value(item.id));
            done[index] = true;
        }
    }
    if (!errors.empty()) {
        return;
    }
    // No cycles remain, so each climb ends at a root.
    for (Impl::Item& item : impl.items) {
        item.ancestry_begin = static_cast<std::uint32_t>(impl.ancestry.size());
        for (std::optional<ItemId> up = item.parent; up; up = impl.items[up->index].parent) {
            impl.ancestry.push_back(*up);
        }
        item.ancestry_count = static_cast<std::uint32_t>(impl.ancestry.size()) - item.ancestry_begin;
        if (const PeoValue* list = field(item.resolved, "tags");
            list != nullptr && list->kind == PeoKind::List) {
            for (const PeoValue& tag : list->items) {
                if (const std::string* text = text_of(tag)) {
                    item.tags.push_back(*text);
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
