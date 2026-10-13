// PEO-121: what a thing shows from a distance (peo/core/look_label.hpp). Fixture IDs and names
// are test data only, not game content.
#include "peo/core/look_label.hpp"

#include <doctest/doctest.h>

#include <string_view>
#include <vector>

using namespace peo::core;

namespace {

constexpr std::string_view kFixture = R"(
[item.tool_base]
name = "tool"
abstract = true
[item.hammer]
name = "hammer"
parent = "tool_base"
[item.claw_hammer]
name = "claw hammer"
parent = "hammer"
[item.brick]
name = "brick"

[item.cabinet]
name = "cabinet"
compartments = [
  { name = "top drawer", capacity_ml = 12000, purposes = [{ purpose = "junk_drawer", w = 1 }] },
  { name = "cupboard", capacity_ml = 90000, purposes = [{ purpose = "dish_cupboard", w = 1 }] },
]
[item.jug]
name = "jug"
[item.jug.features.container]
capacity_ml = 3785

[item.road_sign]
name = "road sign"
[item.road_sign.features.readable]
kind = "sign"
[item.note]
name = "note"
[item.note.features.readable]
kind = "note"
)";

Content fixture() {
    const std::vector<ContentSource> sources{{"look_label.toml", kFixture}};
    ContentLoad load = load_content(sources);
    REQUIRE(load.errors.empty());
    return std::move(*load.content);
}

ItemId must(const Content& content, std::string_view id) {
    const auto item = content.item(id);
    REQUIRE(item.has_value());
    return *item;
}

} // namespace

TEST_SUITE("look_label") {
    TEST_CASE("an item shows its family's generic, past an abstract root") {
        const Content content = fixture();
        const ItemId hammer = must(content, "hammer");
        CHECK(far_label(content, must(content, "claw_hammer")) == FarLabel{hammer, ThingKind::Item});
        CHECK(far_label(content, hammer) == FarLabel{hammer, ThingKind::Item});
        CHECK(content.name(far_label(content, must(content, "claw_hammer")).label) == "hammer");
    }

    TEST_CASE("an item with no parent labels as itself") {
        const Content content = fixture();
        const ItemId brick = must(content, "brick");
        CHECK(far_label(content, brick) == FarLabel{brick, ThingKind::Item});
    }

    TEST_CASE("a container shows only its own kind, never a compartment") {
        const Content content = fixture();
        const ItemId cabinet = must(content, "cabinet");
        const FarLabel label = far_label(content, cabinet);
        CHECK(label == FarLabel{cabinet, ThingKind::Container});
        CHECK(content.name(label.label) == "cabinet");
        const ItemId jug = must(content, "jug");
        CHECK(far_label(content, jug) == FarLabel{jug, ThingKind::Container});
    }

    TEST_CASE("only a sign reads at range; a note is just an item") {
        const Content content = fixture();
        const ItemId sign = must(content, "road_sign");
        CHECK(far_label(content, sign) == FarLabel{sign, ThingKind::Sign});
        const ItemId note = must(content, "note");
        CHECK(far_label(content, note) == FarLabel{note, ThingKind::Item});
    }

    TEST_CASE("the same archetype gives the same label twice") {
        const Content content = fixture();
        const ItemId claw = must(content, "claw_hammer");
        CHECK(far_label(content, claw) == far_label(content, claw));
    }
}
