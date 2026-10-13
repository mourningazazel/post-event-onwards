#pragma once

// PEO-008: content data loaded and resolved (R6, content/README.md). Core parses TOML text it
// is handed and never opens a file: the frontend and the tools read content/ and pass each
// file's path and text. Every item is resolved through its parent chain once, at load, exactly
// as tools/content/derive.py resolve_item does, so nothing deep-merges at runtime. The parser
// stays behind the pimpl; nothing of it appears here.

#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace peo::core {

/// One content file: its path (only named in errors) and its TOML text. Neither needs to
/// outlive load_content.
struct ContentSource {
    std::string_view path;
    std::string_view text;
};

/// An item archetype: its index in the items sorted by ID, so the same content gives the same
/// handles whatever order the files came in.
struct ItemId {
    std::uint32_t index;

    friend constexpr bool operator==(ItemId, ItemId) noexcept = default;
    friend constexpr auto operator<=>(ItemId, ItemId) noexcept = default;
};

/// Longest parent chain an item may have; a longer one is a load error (content is depth 1
/// today). A bound, so a walk up the chain never runs away.
inline constexpr std::uint32_t kMaxItemAncestry = 16;

/// Content as loaded: every record type kept by ID and every item resolved. Immutable; it owns
/// all it read.
class Content {
public:
    Content(Content&&) noexcept;
    Content& operator=(Content&&) noexcept;
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;
    ~Content();

    /// The item with this ID, or nullopt.
    [[nodiscard]] std::optional<ItemId> item(std::string_view id) const noexcept;
    [[nodiscard]] std::size_t item_count() const noexcept;
    /// The item's content ID (asserts `item` is a handle of this Content).
    [[nodiscard]] std::string_view item_id(ItemId item) const noexcept;

private:
    struct Impl;
    explicit Content(std::unique_ptr<Impl> impl) noexcept;
    friend struct ContentLoader;

    std::unique_ptr<Impl> impl_;
};

/// The result of a load: content, or every error found (then no content).
struct ContentLoad {
    std::optional<Content> content;
    std::vector<std::string> errors;
};

/// Load `sources` in the order given, with tools/content/common.py load's rules: each record
/// type is a table of records by ID, unique per type; the merged types (dominance, meta,
/// status, default) merge across files. Then resolve every item as resolve_item does: tables
/// merge, arrays and scalars replace, tags are the parent's then its own with repeats dropped,
/// and `abstract` is never inherited. Any error (a TOML syntax error, an unknown record type, a
/// duplicate ID, an unknown parent, a cycle, a chain over kMaxItemAncestry) means no Content.
[[nodiscard]] ContentLoad load_content(std::span<const ContentSource> sources);

} // namespace peo::core
