// PEO-050: where things are (peo/core/item_store.hpp), against in-memory fixture content.
// Record names are test data only, not game content.
#include "peo/core/item_store.hpp"

#include <doctest/doctest.h>

#include <limits>
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
[item.cabinet]
name = "cabinet"
compartments = [
  { name = "drawer", capacity_ml = 12000 },
  { name = "cupboard", capacity_ml = 90000 },
]
[item.jug]
name = "jug"
[item.jug.features.container]
capacity_ml = 3785
)";

constexpr Vec2i kCell{3, 4};
constexpr Vec2i kOther{5, 6};

Content fixture() {
    const std::vector<ContentSource> sources{{"items.toml", kFixture}};
    ContentLoad load = load_content(sources);
    REQUIRE(load.errors.empty());
    return std::move(*load.content);
}

ItemId must(const Content& content, std::string_view id) {
    const auto item = content.item(id);
    REQUIRE(item.has_value());
    return *item;
}

ThingId put(ItemStore& store, const Content& content, std::string_view id, Location at) {
    ThingId out{};
    REQUIRE(store.place(content, must(content, id), 0, at, out) == Placed::Ok);
    return out;
}

} // namespace

TEST_SUITE("items") {
    TEST_CASE("things on the ground are listed in order, with IDs from 1 never reused") {
        const Content content = fixture();
        ItemStore store;
        const ThingId a = put(store, content, "hammer", Location::ground(kCell));
        const ThingId b = put(store, content, "hammer", Location::ground(kCell));
        const ThingId c = put(store, content, "jug", Location::ground(kCell));
        CHECK(a.value == 1);
        CHECK(b.value == 2);
        CHECK(c.value == 3);
        REQUIRE(store.at(kCell).size() == 3);
        CHECK(store.thing_at(store.at(kCell)[1].index).id == b);
        REQUIRE(store.remove(b));
        CHECK(store.thing(b) == nullptr);
        CHECK(put(store, content, "hammer", Location::ground(kCell)).value == 4);
        CHECK(store.at(kOther).empty());
    }

    TEST_CASE("a thing in a compartment is listed there, not on the cell; bad targets are refused") {
        const Content content = fixture();
        ItemStore store;
        const ThingId cabinet = put(store, content, "cabinet", Location::ground(kCell));
        const ThingId hammer = put(store, content, "hammer", Location::in(cabinet, 1));
        CHECK(store.compartments(content, cabinet) == 2);
        CHECK(store.in(cabinet, 1).size() == 1);
        CHECK(store.at(kCell).size() == 1); // the cabinet only
        ThingId out{};
        CHECK(store.place(content, must(content, "hammer"), 0, Location::in(cabinet, 2), out) ==
              Placed::NoCompartment);
        CHECK(store.place(content, must(content, "hammer"), 0, Location::in(hammer, 0), out) ==
              Placed::NoContainer);
        CHECK(store.place(content, must(content, "hammer"), 0, Location::in(ThingId{99}, 0), out) ==
              Placed::NoThing);
        CHECK(store.thing_count() == 2);
    }

    TEST_CASE("moving a container keeps its contents and empties the old cell") {
        const Content content = fixture();
        ItemStore store;
        const ThingId cabinet = put(store, content, "cabinet", Location::ground(kCell));
        const ThingId hammer = put(store, content, "hammer", Location::in(cabinet, 0));
        REQUIRE(store.move(content, cabinet, Location::ground(kOther)) == Placed::Ok);
        CHECK(store.at(kCell).empty());
        CHECK(store.at(kOther).size() == 1);
        CHECK(store.thing(hammer)->at == Location::in(cabinet, 0));
        CHECK(store.in(cabinet, 0).size() == 1);
    }

    TEST_CASE("a jug goes into a cabinet, but nothing goes into itself or its own contents") {
        const Content content = fixture();
        ItemStore store;
        const ThingId cabinet = put(store, content, "cabinet", Location::ground(kCell));
        const ThingId jug = put(store, content, "jug", Location::ground(kCell));
        REQUIRE(store.move(content, jug, Location::in(cabinet, 0)) == Placed::Ok);
        CHECK(store.move(content, cabinet, Location::in(jug, 0)) == Placed::WouldNest);
        CHECK(store.move(content, cabinet, Location::in(cabinet, 1)) == Placed::WouldNest);
        CHECK(store.thing(cabinet)->at == Location::ground(kCell));
        CHECK(store.in(cabinet, 0).size() == 1);
    }

    TEST_CASE("a full location refuses new entries, never a line's growth, and saturates") {
        const Content content = fixture();
        ItemStore store(ItemStoreParams{.max_entries = 3});
        const Location ground = Location::ground(kCell);
        REQUIRE(store.place_line(content, must(content, "hammer"), 7, 5, 500, 100, ground) == Placed::Ok);
        put(store, content, "hammer", ground);
        put(store, content, "jug", ground);
        const ItemStore before = store;
        ThingId out{};
        CHECK(store.place(content, must(content, "hammer"), 0, ground, out) == Placed::Full);
        CHECK(store.place_line(content, must(content, "jug"), 0, 1, 0, 0, ground) == Placed::Full);
        CHECK(store == before);
        store.grow(ground, 0, 10);
        CHECK(store.line_at(store.at(kCell)[0].index).count == 15);
        store.grow(ground, 0, std::numeric_limits<std::uint32_t>::max());
        CHECK(store.line_at(store.at(kCell)[0].index).count == std::numeric_limits<std::uint32_t>::max());
        store.shrink(ground, 0, std::numeric_limits<std::uint32_t>::max());
        CHECK(store.at(kCell).size() == 2); // shrunk to zero, the line is gone
    }

    TEST_CASE("a line and a thing in one location list in insertion order") {
        const Content content = fixture();
        ItemStore store;
        const Location ground = Location::ground(kCell);
        const ThingId first = put(store, content, "hammer", ground);
        REQUIRE(store.place_line(content, must(content, "jug"), 3, 4, 900, 50, ground) == Placed::Ok);
        const ThingId last = put(store, content, "hammer", ground);
        const auto listing = store.at(kCell);
        REQUIRE(listing.size() == 3);
        CHECK(listing[0].kind == Entry::Kind::Thing);
        CHECK(store.thing_at(listing[0].index).id == first);
        CHECK(listing[1].kind == Entry::Kind::Line);
        CHECK(store.line_at(listing[1].index).count == 4);
        CHECK(store.line_at(listing[1].index).quality_mean == 900);
        CHECK(store.thing_at(listing[2].index).id == last);
    }

    TEST_CASE("remove refuses a container that still holds anything") {
        const Content content = fixture();
        ItemStore store;
        const ThingId cabinet = put(store, content, "cabinet", Location::ground(kCell));
        const ThingId hammer = put(store, content, "hammer", Location::in(cabinet, 1));
        CHECK_FALSE(store.remove(cabinet));
        CHECK(store.thing(cabinet) != nullptr);
        REQUIRE(store.remove(hammer));
        CHECK(store.remove(cabinet));
        CHECK(store.empty());
    }
}
