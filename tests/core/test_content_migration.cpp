// PEO-092: content migration on load (peo/core/content_migration.hpp) and the CDIC section.
// Fixture IDs are test data only, not game content.
#include "peo/core/byte_io.hpp"
#include "peo/core/content_migration.hpp"
#include "peo/core/save.hpp"
#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "content_files.hpp"

using namespace peo::core;

namespace {

// tool_base (abstract) -> hammer -> claw_hammer and mallet; crowbar; junk. Remaps: a renamed
// parent, a two-step chain, and a cycle.
constexpr std::string_view kItems = R"(
[item.tool_base]
name = "tool"
abstract = true
[item.hammer]
name = "hammer"
parent = "tool_base"
[item.claw_hammer]
name = "claw hammer"
parent = "hammer"
[item.mallet]
name = "mallet"
parent = "hammer"
[item.crowbar]
name = "crowbar"
[item.junk]
name = "junk"
)";
constexpr std::string_view kRemaps = R"(
[remap.hammer_base_old]
to = "hammer"
why = "renamed"
[remap.pry_bar]
to = "crowbar_v2"
why = "renamed twice"
[remap.crowbar_v2]
to = "crowbar"
why = "renamed"
[remap.loop_a]
to = "loop_b"
why = "a cycle"
[remap.loop_b]
to = "loop_a"
why = "a cycle"
)";

constexpr Seed kSeed = 31;
constexpr std::string_view kBuild = "test-build";
constexpr std::uint32_t kCdic = section_id("CDIC");

Content fixture() {
    const std::vector<ContentSource> sources{{"items.toml", kItems}, {"remaps.toml", kRemaps}};
    ContentLoad load = load_content(sources);
    REQUIRE(load.errors.empty());
    return std::move(*load.content);
}

ItemId must(const Content& content, std::string_view id) {
    const auto item = content.item(id);
    REQUIRE(item.has_value());
    return *item;
}

WorldParams small_params() {
    return {.initial_dead = 10, .stage_width = 32, .stage_height = 20};
}

SaveSection& section(SaveImage& image, std::uint32_t id) {
    const auto it = std::find_if(image.sections.begin(), image.sections.end(),
                                 [id](const SaveSection& sec) { return sec.id == id; });
    REQUIRE(it != image.sections.end());
    return *it;
}

/// A CDIC payload of `entries`, as World::save lays it out.
std::vector<std::byte> dictionary(const std::vector<ContentRef>& entries) {
    ByteWriter w;
    w.u32(static_cast<std::uint32_t>(entries.size()));
    for (const ContentRef& entry : entries) {
        w.string(entry.id);
        w.u16(static_cast<std::uint16_t>(entry.ancestry.size()));
        for (const std::string& ancestor : entry.ancestry) {
            w.string(ancestor);
        }
        w.string(entry.name);
    }
    return w.take();
}

bool names_cdic(const std::vector<SaveIssue>& issues) {
    return std::any_of(issues.begin(), issues.end(),
                       [](const SaveIssue& i) { return i.section_id == kCdic; });
}

} // namespace

TEST_SUITE("content_migration") {
    TEST_CASE("an item still present resolves Kept to itself, the same twice") {
        const Content content = fixture();
        const ContentRef ref = content_ref(content, must(content, "claw_hammer"));
        const Resolved first = resolve(ref, content);
        CHECK(first == Resolved{must(content, "claw_hammer"), Migration::Kept});
        CHECK(resolve(ref, content) == first);
    }

    TEST_CASE("a renamed item resolves Remapped; a two-step chain to its end; a cycle falls through") {
        const Content content = fixture();
        CHECK(resolve({"crowbar_v2", {}, "crowbar"}, content) ==
              Resolved{must(content, "crowbar"), Migration::Remapped});
        CHECK(resolve({"pry_bar", {}, "pry bar"}, content) ==
              Resolved{must(content, "crowbar"), Migration::Remapped});
        CHECK(resolve({"loop_a", {"hammer"}, "loop"}, content) ==
              Resolved{must(content, "hammer"), Migration::StandIn});
    }

    TEST_CASE("a removed item stands in as its nearest spawnable ancestor") {
        const Content content = fixture();
        const ItemId hammer = must(content, "hammer");
        // A removed hammer kind, with the chain a save recorded for it.
        CHECK(resolve({"lump_hammer", {"hammer", "tool_base"}, "lump hammer"}, content) ==
              Resolved{hammer, Migration::StandIn});
        // An abstract nearest ancestor is skipped for the next spawnable one.
        CHECK(resolve({"gone", {"tool_base", "hammer"}, "gone"}, content) ==
              Resolved{hammer, Migration::StandIn});
        // A parent that was itself renamed still counts, through its remap.
        CHECK(resolve({"old_mallet", {"hammer_base_old"}, "old mallet"}, content) ==
              Resolved{hammer, Migration::StandIn});
    }

    TEST_CASE("a removed item with no present ancestor resolves Junk; without a junk archetype, no item") {
        const Content content = fixture();
        CHECK(resolve({"gizmo", {"gadget_base"}, "gizmo"}, content) ==
              Resolved{must(content, "junk"), Migration::Junk});
        const std::vector<ContentSource> bare{{"items.toml", "[item.cup]\nname = \"cup\"\n"}};
        const ContentLoad no_junk = load_content(bare);
        REQUIRE(no_junk.content.has_value());
        CHECK(resolve({"gizmo", {}, "gizmo"}, *no_junk.content) == Resolved{std::nullopt, Migration::Junk});
    }

    TEST_CASE("content_ref of a present item gives its ID, parent chain nearest first, and base name") {
        const Content content = fixture();
        const ContentRef ref = content_ref(content, must(content, "claw_hammer"));
        CHECK(ref.id == "claw_hammer");
        CHECK(ref.ancestry == std::vector<std::string>{"hammer", "tool_base"});
        CHECK(ref.name == "claw hammer");
        CHECK(content_ref(content, must(content, "crowbar")).ancestry.empty());
    }

    TEST_CASE("a saved and restored World has an empty report and summary") {
        const Content content = fixture();
        World w(kSeed, small_params());
        w.step(Action::wait());
        World loaded(kSeed + 1, small_params());
        loaded.set_content(&content);
        std::vector<SaveIssue> issues;
        MigrationReport report;
        REQUIRE(loaded.restore(w.save(kBuild), issues, report) == SaveStatus::Ok);
        CHECK(issues.empty());
        CHECK(report.lines.empty());
        CHECK(migration_summary(report).empty());
        CHECK(World::equivalent(loaded, w));
    }

    TEST_CASE("a four-entry CDIC restores Ok with three report lines in order and one summary") {
        const Content content = fixture();
        const World w(kSeed, small_params());
        SaveImage image = w.save(kBuild);
        section(image, kCdic).payload = dictionary({content_ref(content, must(content, "claw_hammer")),
                                                    {"pry_bar", {}, "pry bar"},
                                                    {"lump_hammer", {"hammer", "tool_base"}, "lump hammer"},
                                                    {"gizmo", {"gadget_base"}, "gizmo"}});
        World loaded(kSeed + 1, small_params());
        loaded.set_content(&content);
        std::vector<SaveIssue> issues;
        MigrationReport report;
        REQUIRE(loaded.restore(image, issues, report) == SaveStatus::Ok);
        REQUIRE(report.lines.size() == 3);
        CHECK(report.lines[0].old_id == "pry_bar");
        CHECK(report.lines[0].to == Resolved{must(content, "crowbar"), Migration::Remapped});
        CHECK(report.lines[1].old_id == "lump_hammer");
        CHECK(report.lines[1].to_name == "hammer");
        CHECK(report.lines[2].old_name == "gizmo"); // junk keeps the old name
        CHECK(report.lines[2].to.how == Migration::Junk);
        CHECK(migration_summary(report) == "Since this save: pry bar is now a crowbar; lump hammer is now a "
                                           "hammer; 1 old thing is now junk.");
    }

    TEST_CASE("every malformed CDIC is Corrupt naming CDIC and changes nothing") {
        const Content content = fixture();
        const World w(kSeed, small_params());
        const SaveImage image = w.save(kBuild);
        std::vector<ContentRef> deep{{"deep", std::vector<std::string>(600, "hammer"), "deep"}};
        std::vector<std::byte> truncated = dictionary({{"claw_hammer", {"hammer"}, "claw hammer"}});
        truncated.resize(truncated.size() - 3); // the name's bytes cut short
        ByteWriter huge_count;
        huge_count.u32(1'000'000);
        const struct {
            const char* name;
            std::vector<std::byte> payload;
        } forges[] = {
            {"ancestry count 600", dictionary(deep)},
            {"a string cut off", truncated},
            {"an empty id", dictionary({{"", {}, "nothing"}})},
            {"an id over 64 bytes", dictionary({{std::string(65, 'x'), {}, "long"}})},
            {"a name over 128 bytes", dictionary({{"cup", {}, std::string(129, 'n')}})},
            {"a count past the bytes left", huge_count.take()},
        };
        World target(kSeed, small_params());
        target.set_content(&content);
        target.step(Action::wait());
        const std::vector<std::byte> before = write_save(target.save(kBuild));
        for (const auto& forge : forges) {
            CAPTURE(forge.name);
            SaveImage forged = image;
            section(forged, kCdic).payload = forge.payload;
            std::vector<SaveIssue> issues;
            MigrationReport report;
            CHECK(target.restore(forged, issues, report) == SaveStatus::Corrupt);
            CHECK(names_cdic(issues));
            CHECK(report.lines.empty());
            CHECK(write_save(target.save(kBuild)) == before);
        }
    }

    TEST_CASE("content entries with no content set are refused; an image without CDIC restores") {
        const Content content = fixture();
        const World w(kSeed, small_params());
        SaveImage image = w.save(kBuild);
        section(image, kCdic).payload = dictionary({content_ref(content, must(content, "hammer"))});
        World target(kSeed + 1, small_params());
        std::vector<SaveIssue> issues;
        MigrationReport report;
        CHECK(target.restore(image, issues, report) == SaveStatus::Corrupt);
        CHECK(names_cdic(issues));

        SaveImage old = w.save(kBuild);
        std::erase_if(old.sections, [](const SaveSection& s) { return s.id == kCdic; });
        issues.clear();
        CHECK(target.restore(old, issues, report) == SaveStatus::Ok);
        CHECK(report.lines.empty());
    }
}

TEST_SUITE("scenario: content_migration") {
    TEST_CASE("real content: every spawnable item is Kept, and a removed claw_hammer becomes a hammer") {
        std::string error;
        const auto files = peo::app::read_content_files(PEO_CONTENT_DIR, error);
        REQUIRE_MESSAGE(files.has_value(), error);
        const ContentLoad full = load_content(files->sources());
        REQUIRE(full.content.has_value());
        const Content& content = *full.content;
        std::size_t kept = 0;
        for (std::uint32_t i = 0; i < content.item_count(); ++i) {
            const ItemId item{i};
            if (content.abstract(item)) {
                continue;
            }
            CAPTURE(content.item_id(item));
            CHECK(resolve(content_ref(content, item), content) == Resolved{item, Migration::Kept});
            ++kept;
        }
        CHECK(kept > 0);

        // Remove claw_hammer: rename its records so content no longer has the ID.
        const ContentRef saved = content_ref(content, must(content, "claw_hammer"));
        std::vector<std::string> texts = files->texts;
        for (std::string& text : texts) {
            for (std::size_t at = text.find("[item.claw_hammer"); at != std::string::npos;
                 at = text.find("[item.claw_hammer", at)) {
                text.replace(at, std::string_view("[item.claw_hammer").size(), "[item.removed_claw_hammer");
            }
        }
        std::vector<ContentSource> sources;
        for (std::size_t i = 0; i < texts.size(); ++i) {
            sources.push_back({files->paths[i], texts[i]});
        }
        const ContentLoad removed = load_content(sources);
        REQUIRE(removed.content.has_value());
        REQUIRE_FALSE(removed.content->item("claw_hammer").has_value());
        CHECK(resolve(saved, *removed.content) ==
              Resolved{must(*removed.content, "hammer"), Migration::StandIn});
    }
}
