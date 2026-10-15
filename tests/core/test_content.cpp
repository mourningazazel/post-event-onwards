// PEO-008: loading and resolving content text (peo/core/content.hpp). Sources are string
// literals; item IDs here are test data only, not game content.
#include "peo/core/content.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace peo::core;

namespace {

constexpr std::string_view kFamily = R"(
[item.tool_base]
name = "tool"
abstract = true
why = "family template"
tags = ["cat.tool", "mat.metal"]
dims_mm = [300, 40, 20]
grip = "plain"
[item.tool_base.features.edge]
sharpness = 2
angle = 30

[item.hammer]
name = "hammer"
parent = "tool_base"
tags = ["cat.tool.hammer", "mat.metal", "mat.wood"]
dims_mm = [330, 120, 30]
[item.hammer.features.edge]
sharpness = 1
[item.hammer.features.readable]
kind = "sign"

[item.claw_hammer]
name = "claw hammer"
parent = "hammer"
why = "pulls nails"
)";

ContentLoad load(std::initializer_list<ContentSource> sources) {
    return load_content(std::span<const ContentSource>(sources.begin(), sources.size()));
}

ItemId must(const Content& content, std::string_view id) {
    const auto item = content.item(id);
    REQUIRE(item.has_value());
    return *item;
}

/// Whether some error contains every one of `parts`.
bool reported(const std::vector<std::string>& errors, std::initializer_list<std::string_view> parts) {
    return std::any_of(errors.begin(), errors.end(), [&](const std::string& error) {
        return std::all_of(parts.begin(), parts.end(),
                           [&](std::string_view part) { return error.find(part) != std::string::npos; });
    });
}

/// Far deeper than any real family, to show a chain has no length limit.
constexpr int kDeepChain = 100;

/// A chain of `length` items, each the parent of the next: link0 <- link1 <- ...
std::string chain_text(int length) {
    std::string text;
    for (int i = 0; i < length; ++i) {
        text += "[item.link" + std::to_string(i) + "]\nname = \"link\"\n";
        if (i > 0) {
            text += "parent = \"link" + std::to_string(i - 1) + "\"\n";
        }
    }
    return text;
}

} // namespace

TEST_SUITE("content") {
    TEST_CASE("a child merges tables over its parent and replaces arrays and scalars") {
        const ContentLoad loaded = load({{"items.toml", kFamily}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        const ItemId hammer = must(content, "hammer");

        CHECK(content.name(hammer) == "hammer");
        CHECK(content.int_at(hammer, "dims_mm.0") == 330); // array replaced
        CHECK(content.int_at(hammer, "dims_mm.1") == 120);
        CHECK(content.int_at(hammer, "features.edge.sharpness") == 1); // scalar overridden
        CHECK(content.int_at(hammer, "features.edge.angle") == 30);    // table merged
        CHECK(content.string_at(hammer, "features.readable.kind") == "sign");
        CHECK(content.string_at(hammer, "grip") == "plain");
        CHECK(content.string_at(hammer, "id") == "hammer");
    }

    TEST_CASE("tags accumulate parent first with repeats dropped") {
        const ContentLoad loaded = load({{"items.toml", kFamily}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        const auto tags = content.tags(must(content, "hammer"));
        const std::vector<std::string> expected{"cat.tool", "mat.metal", "cat.tool.hammer", "mat.wood"};
        CHECK(std::vector<std::string>(tags.begin(), tags.end()) == expected);
        // A grandchild with no tags of its own still carries the whole list.
        CHECK(content.tags(must(content, "claw_hammer")).size() == expected.size());
    }

    TEST_CASE("abstract is not inherited; why is replaced by the child's or kept") {
        const ContentLoad loaded = load({{"items.toml", kFamily}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        CHECK(content.abstract(must(content, "tool_base")));
        CHECK_FALSE(content.abstract(must(content, "hammer")));
        CHECK_FALSE(content.has(must(content, "hammer"), "abstract"));
        CHECK(content.string_at(must(content, "hammer"), "why") == "family template");
        CHECK(content.string_at(must(content, "claw_hammer"), "why") == "pulls nails");
    }

    TEST_CASE("parents and handles: nearest parent, IDs sorted whatever the file order") {
        const ContentLoad loaded = load({{"b.toml", "[item.zeta]\nname = \"z\"\n"}, {"items.toml", kFamily}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        REQUIRE(content.item_count() == 4);
        CHECK(content.item_id(ItemId{0}) == "claw_hammer");
        CHECK(content.item_id(ItemId{3}) == "zeta");
        CHECK(content.parent(must(content, "claw_hammer")) == content.item("hammer"));
        CHECK(content.parent(must(content, "hammer")) == content.item("tool_base"));
        CHECK_FALSE(content.parent(must(content, "tool_base")).has_value());
        const auto chain = content.ancestry(must(content, "claw_hammer"));
        REQUIRE(chain.size() == 2);
        CHECK(content.item_id(chain[0]) == "hammer");
        CHECK(content.item_id(chain[1]) == "tool_base");
        CHECK_FALSE(content.item("crowbar").has_value());
    }

    TEST_CASE("an unknown parent and a cycle give errors and no content") {
        const ContentLoad unknown = load({{"a.toml", "[item.orphan]\nparent = \"nobody\"\n"}});
        CHECK_FALSE(unknown.content.has_value());
        CHECK(reported(unknown.errors, {"item.orphan", "unknown parent 'nobody'", "orphan -> nobody"}));

        const ContentLoad cycle =
            load({{"a.toml", "[item.ping]\nparent = \"pong\"\n[item.pong]\nparent = \"ping\"\n"}});
        CHECK_FALSE(cycle.content.has_value());
        CHECK(reported(cycle.errors, {"inheritance cycle", "ping -> pong -> ping"}));

        // A cycle beside a long chain is still found, and the chain alone is fine.
        constexpr std::string_view kLoop =
            "[item.loop_a]\nparent = \"loop_b\"\n[item.loop_b]\nparent = \"loop_a\"\n";
        const std::string chain = chain_text(kDeepChain);
        const ContentLoad deep_cycle = load({{"chain.toml", chain}, {"loop.toml", kLoop}});
        CHECK_FALSE(deep_cycle.content.has_value());
        CHECK(reported(deep_cycle.errors, {"inheritance cycle", "loop_a -> loop_b -> loop_a"}));
    }

    TEST_CASE("a chain has no length limit; ancestry lists it nearest first") {
        // IDs sort as link0, link1, link10, ... so resolving meets chains half resolved.
        const ContentLoad loaded = load({{"chain.toml", chain_text(kDeepChain)}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        const auto last = content.ancestry(must(content, "link" + std::to_string(kDeepChain - 1)));
        REQUIRE(last.size() == static_cast<std::size_t>(kDeepChain - 1));
        CHECK(content.item_id(last.front()) == "link" + std::to_string(kDeepChain - 2));
        CHECK(content.item_id(last.back()) == "link0");
        CHECK(content.ancestry(must(content, "link10")).size() == 10);
        CHECK(content.ancestry(must(content, "link0")).empty());
    }

    TEST_CASE("load errors name their sources: a duplicate, an unknown type, a syntax error") {
        const ContentLoad duplicate = load({{"first.toml", "[item.cup]\nname = \"cup\"\n"},
                                            {"second.toml", "[item.cup]\nname = \"mug\"\n"}});
        CHECK_FALSE(duplicate.content.has_value());
        CHECK(reported(duplicate.errors, {"second.toml: duplicate item.cup (first in first.toml)"}));

        const ContentLoad unknown = load({{"odd.toml", "[gadget.x]\nname = \"x\"\n"}});
        CHECK_FALSE(unknown.content.has_value());
        CHECK(reported(unknown.errors, {"odd.toml: unknown record type [gadget.*]"}));

        const ContentLoad broken = load({{"good.toml", kFamily}, {"broken.toml", "[item.cup\nname = 1\n"}});
        CHECK_FALSE(broken.content.has_value());
        CHECK(reported(broken.errors, {"broken.toml: TOML parse error", "line 1"}));
    }

    TEST_CASE("a merged type split over two sources merges; records of every type are kept") {
        const ContentLoad loaded = load({{"a.toml", "[meta]\nversion = 1\n[material.oak]\nname = \"oak\"\n"},
                                         {"b.toml", "[meta]\nauthor = \"x\"\n[item.cup]\nname = \"cup\"\n"}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        CHECK(content.has_record("material", "oak"));
        CHECK(content.has_record("item", "cup"));
        CHECK_FALSE(content.has_record("material", "ash"));
        CHECK_FALSE(content.has_record("gadget", "oak"));

        // Two halves of a merged table in different files do not clash as duplicates.
        const ContentLoad halves = load({{"a.toml", "[meta.x]\na = 1\n"}, {"b.toml", "[meta.x]\nb = 2\n"}});
        CHECK(halves.errors.empty());
    }

    TEST_CASE("dotted reads: nested tables, array indices, and missing or mistyped paths") {
        const ContentLoad loaded = load({{"items.toml", kFamily}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        const ItemId hammer = must(content, "hammer");

        CHECK(content.has(hammer, "features.edge"));
        CHECK(content.has(hammer, "dims_mm.2"));
        CHECK_FALSE(content.has(hammer, "dims_mm.3"));
        CHECK_FALSE(content.has(hammer, "dims_mm.x"));
        CHECK_FALSE(content.has(hammer, "features.container"));
        CHECK_FALSE(content.has(hammer, ""));
        CHECK_FALSE(content.has(hammer, "name.first"));
        CHECK_FALSE(content.int_at(hammer, "name").has_value());
        CHECK_FALSE(content.string_at(hammer, "dims_mm.0").has_value());
        CHECK_FALSE(content.int_at(hammer, "features.edge").has_value());
        CHECK(content.string_at(hammer, "tags.2") == "cat.tool.hammer");
    }

    TEST_CASE("a remap gives the ID its old item now has, one step, or nullopt") {
        const ContentLoad loaded = load({{"remaps.toml", "[remap.old]\nto = \"new\"\nwhy = \"renamed\"\n"
                                                         "[remap.odd]\nto = 7\nwhy = \"not a name\"\n"},
                                         {"items.toml", "[item.new]\nname = \"new\"\n"}});
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        CHECK(content.remap("old") == "new");
        CHECK(content.has_record("remap", "old"));
        CHECK_FALSE(content.remap("new").has_value()); // an item with no remap of its own
        CHECK_FALSE(content.remap("odd").has_value()); // `to` is not a string
        CHECK_FALSE(load({{"items.toml", kFamily}}).content->remap("old").has_value()); // no remaps at all
    }

    TEST_CASE("the same sources in the same order load the same content twice") {
        const std::array<ContentSource, 2> sources{
            {{"items.toml", kFamily}, {"z.toml", "[item.zeta]\nname = \"z\"\ntags = [\"a\"]\n"}}};
        const ContentLoad first = load_content(sources);
        const ContentLoad second = load_content(sources);
        REQUIRE(first.content.has_value());
        REQUIRE(second.content.has_value());
        REQUIRE(first.content->item_count() == second.content->item_count());
        for (std::uint32_t i = 0; i < first.content->item_count(); ++i) {
            const ItemId item{i};
            CHECK(first.content->item_id(item) == second.content->item_id(item));
            CHECK(first.content->name(item) == second.content->name(item));
            CHECK(first.content->parent(item) == second.content->parent(item));
            CHECK(first.content->tags(item).size() == second.content->tags(item).size());
        }
    }
}

// PEO-142: .peo sources, typed and parented by the path table. Paths and names are test data.
TEST_SUITE("content") {
    TEST_CASE(".peo: a parent from the header path, abstract, quantities, ranges, paths and tags") {
        constexpr std::string_view kItems =
            R"(abstract item.tool { tags = [def.tag.cat.tool, def.tag.use.tool] }
item.tool.hammer {
    name = "hammer",
    mass = 1 kg,
    count = 2..5,
    shape = def.shape.rod,
    tags = [def.tag.use.tool, def.tag.use.weapon]
}
item.material.steel { name = "steel" }
)";
        const std::vector<ContentSource> sources{{"items.peo", kItems}};
        const ContentLoad loaded = load_content(sources);
        REQUIRE(loaded.errors.empty());
        const Content& content = *loaded.content;
        const ItemId hammer = must(content, "hammer");
        CHECK(content.parent(hammer) == content.item("tool"));
        CHECK(content.abstract(must(content, "tool")));
        CHECK_FALSE(content.abstract(hammer));
        CHECK(content.int_at(hammer, "mass") == 1000); // base unit: grams
        CHECK(content.int_at(hammer, "count.0") == 2);
        CHECK(content.int_at(hammer, "count.1") == 5);
        CHECK_FALSE(content.has(hammer, "count.2"));
        CHECK(content.string_at(hammer, "shape") == "def.shape.rod");
        const auto tags = content.tags(hammer);
        const std::vector<std::string> expected{"def.tag.cat.tool", "def.tag.use.tool", "def.tag.use.weapon"};
        CHECK(std::vector<std::string>(tags.begin(), tags.end()) == expected);
        // item.material.* is a material, not an item.
        CHECK(content.has_record("material", "steel"));
        CHECK_FALSE(content.item("steel").has_value());
    }

    TEST_CASE(".peo: a header whose prefix names no item is rejected") {
        const std::vector<ContentSource> sources{
            {"items.peo", "item.nowhere.hammer { name = \"hammer\" }\n"}};
        const ContentLoad loaded = load_content(sources);
        CHECK_FALSE(loaded.content.has_value());
        CHECK(reported(loaded.errors, {"items.peo:1", "'item.nowhere' names no item"}));
    }

    TEST_CASE(".peo: a remap's path gives a flat ID") {
        const std::vector<ContentSource> sources{
            {"items.peo", "item.tool { name = \"tool\" }\nitem.tool.hammer { name = \"hammer\" }\n"},
            {"remaps.peo", "remap.item.hammer_old { to = item.tool.hammer, why = \"renamed\" }\n"}};
        const ContentLoad loaded = load_content(sources);
        REQUIRE(loaded.errors.empty());
        CHECK(loaded.content->remap("hammer_old") == "hammer");
    }

    TEST_CASE("the same flat ID from a .toml and a .peo source is rejected naming both") {
        const std::vector<ContentSource> sources{{"a.toml", "[item.cup]\nname = \"cup\"\n"},
                                                 {"b.peo", "item.cup { name = \"mug\" }\n"}};
        const ContentLoad loaded = load_content(sources);
        CHECK_FALSE(loaded.content.has_value());
        CHECK(reported(loaded.errors, {"b.peo: duplicate item.cup (first in a.toml)"}));
    }
}
