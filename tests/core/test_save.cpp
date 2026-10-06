#include "peo/core/byte_io.hpp"
#include "peo/core/rng.hpp"
#include "peo/core/save.hpp"
#include "peo/core/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

using namespace peo::core;

namespace {
constexpr Seed kSeed = 31;
constexpr std::string_view kBuild = "test-build";
constexpr std::size_t kMagicBytes = 8;
constexpr std::size_t kSectionHeaderBytes = 20; // id, version, scope, codec, length, crc
// HORD's layout: u32 count, per Dead Vec2i + u16, u32 count, per move u32 + Vec2i, u64 count.
constexpr std::size_t kCountBytes = 4;
constexpr std::size_t kDeadBytes = 10;
constexpr std::size_t kMoveBytes = 12;
constexpr std::size_t kVectorCountBytes = 8;

WorldParams small_params(int dead = 30) {
    return {.initial_dead = dead, .stage_width = 32, .stage_height = 20};
}

std::vector<std::byte> payload_of(std::initializer_list<int> values) {
    std::vector<std::byte> out;
    for (const int v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

/// Where section `n`'s payload starts in write_save's bytes for `image`.
std::size_t payload_offset(const SaveImage& image, std::size_t n) {
    std::size_t at = kMagicBytes + sizeof(std::uint32_t) + sizeof(std::uint16_t) + image.build.size() +
                     sizeof(std::uint32_t);
    for (std::size_t i = 0; i < n; ++i) {
        at += kSectionHeaderBytes + image.sections[i].payload.size();
    }
    return at + kSectionHeaderBytes;
}

bool has_issue(const std::vector<SaveIssue>& issues, std::uint32_t id) {
    return std::any_of(issues.begin(), issues.end(), [&](const SaveIssue& i) { return i.section_id == id; });
}

/// Read bytes, then restore them into `w` only when the read is Ok: what a caller does.
SaveStatus load(World& w, std::span<const std::byte> bytes, std::vector<SaveIssue>& issues) {
    SaveRead r = read_save(bytes);
    issues = r.issues;
    return r.status == SaveStatus::Ok ? w.restore(r.image, issues) : r.status;
}

/// A mixed action: mostly walking, some running and waiting, any direction.
Action mixed(Rng& pick) {
    constexpr int kWaitOneIn = 4;
    constexpr int kRunOneIn = 3;
    const Vec2i dir = kNeighbours4[pick.range(0, 3)];
    if (pick.range(1, kWaitOneIn) == 1) {
        return Action::wait();
    }
    return Action::step(dir, pick.range(1, kRunOneIn) == 1 ? kRunStepSubsteps : kStepSubsteps);
}

/// A 40 x 30 open stage with a border wall and a walled 7 x 5 building open on its west
/// side; the player inside, a ring of the Dead outside (PEO-088's kind of layout).
struct Layout {
    Stage stage;
    Vec2i player;
    std::vector<Dead> horde;
};
Layout small_layout() {
    constexpr int kW = 40;
    constexpr int kH = 30;
    constexpr int kX0 = 16;
    constexpr int kY0 = 12;
    constexpr int kX1 = 22;
    constexpr int kY1 = 16;
    Layout l;
    l.stage.blocked = Grid<bool>(kW, kH, false);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const bool border = x == 0 || y == 0 || x == kW - 1 || y == kH - 1;
            const bool wall = (x == kX0 || x == kX1 || y == kY0 || y == kY1) && x >= kX0 && x <= kX1 &&
                              y >= kY0 && y <= kY1;
            l.stage.blocked.at(x, y) = border || wall;
        }
    }
    l.stage.blocked.at(kX0, (kY0 + kY1) / 2) = false;
    l.stage.entry = {kX1 - 1, (kY0 + kY1) / 2};
    l.player = l.stage.entry;
    for (int x = 3; x < kW - 3; x += 2) {
        for (const int y : {3, kH - 4}) {
            l.horde.push_back(
                {.pos = {x, y}, .step_substeps = static_cast<std::uint16_t>(kUpdatePeriodSubsteps)});
        }
    }
    return l;
}

/// Save `a` after `before` mixed actions, restore it into a fresh World, then step both
/// through `after` more: equivalent after every one. With `speculating`, the restored
/// World commits a speculation each turn while the original steps.
void check_continuation(World& a, int before, int after, bool speculating) {
    Rng pick(kSeed);
    for (int i = 0; i < before; ++i) {
        a.step(mixed(pick));
    }
    const std::vector<std::byte> bytes = write_save(a.save(kBuild));
    World b(kSeed + 1, small_params(0));
    std::vector<SaveIssue> issues;
    REQUIRE(load(b, bytes, issues) == SaveStatus::Ok);
    REQUIRE(issues.empty());
    REQUIRE(World::equivalent(a, b));
    CHECK(write_save(b.save(kBuild)) == bytes);
    Speculation spec;
    for (int i = 0; i < after; ++i) {
        const Action act = mixed(pick);
        a.step(act);
        if (speculating) {
            b.speculate(spec);
            b.commit(spec, act);
        } else {
            b.step(act);
        }
        if (!World::equivalent(a, b)) {
            FAIL("diverged at action " << i << " after the restore");
        }
    }
}
} // namespace

TEST_SUITE("save") {
    TEST_CASE("crc32 is the IEEE one") {
        constexpr std::uint32_t kCheck = 0xCBF43926U; // the standard check value
        const std::string text = "123456789";
        CHECK(crc32(std::as_bytes(std::span(text.data(), text.size()))) == kCheck);
        CHECK(crc32({}) == 0);
    }

    TEST_CASE("an image of three sections, one per scope, reads back field for field") {
        const SaveImage image{.format = kSaveFormat,
                              .build = std::string(kBuild),
                              .sections = {{section_id("AAAA"), 1, SyncScope::World, payload_of({1, 2, 3})},
                                           {section_id("BBBB"), 7, SyncScope::Character, payload_of({})},
                                           {section_id("CCCC"), 2, SyncScope::Local, payload_of({255, 0})}}};
        const SaveRead r = read_save(write_save(image));
        REQUIRE(r.status == SaveStatus::Ok);
        CHECK(r.issues.empty());
        CHECK(r.image.format == kSaveFormat);
        CHECK(r.image.build == kBuild);
        REQUIRE(r.image.sections.size() == image.sections.size());
        for (std::size_t i = 0; i < image.sections.size(); ++i) {
            CAPTURE(i);
            CHECK(r.image.sections[i].id == image.sections[i].id);
            CHECK(r.image.sections[i].version == image.sections[i].version);
            CHECK(r.image.sections[i].scope == image.sections[i].scope);
            CHECK(r.image.sections[i].payload == image.sections[i].payload);
        }
        CHECK(section_name(section_id("WRLD")) == "WRLD");
    }

    TEST_CASE("a flipped payload byte in a World section makes the image Corrupt") {
        const World w(kSeed, small_params());
        const SaveImage image = w.save(kBuild);
        std::vector<std::byte> bytes = write_save(image);
        bytes[payload_offset(image, 0)] ^= std::byte{1};
        const SaveRead r = read_save(bytes);
        CHECK(r.status == SaveStatus::Corrupt);
        CHECK(has_issue(r.issues, image.sections[0].id));
        World loaded = w;
        std::vector<SaveIssue> issues;
        CHECK(load(loaded, bytes, issues) == SaveStatus::Corrupt);
        CHECK(World::equivalent(loaded, w));
    }

    TEST_CASE("a corrupt Local section is dropped with an issue and the image stays Ok") {
        const SaveImage image{.format = kSaveFormat,
                              .build = std::string(kBuild),
                              .sections = {{section_id("AAAA"), 1, SyncScope::World, payload_of({1})},
                                           {section_id("LOCL"), 1, SyncScope::Local, payload_of({4, 5, 6})}}};
        std::vector<std::byte> bytes = write_save(image);
        bytes[payload_offset(image, 1) + 1] ^= std::byte{0x40};
        const SaveRead r = read_save(bytes);
        CHECK(r.status == SaveStatus::Ok);
        REQUIRE(r.image.sections.size() == 1);
        CHECK(r.image.sections[0].id == section_id("AAAA"));
        CHECK(has_issue(r.issues, section_id("LOCL")));
    }

    TEST_CASE("a cut-off image, a bad magic, a newer format and a length past the end") {
        const World w(kSeed, small_params());
        const SaveImage image = w.save(kBuild);
        const std::vector<std::byte> good = write_save(image);
        const auto status = [&](const std::vector<std::byte>& bytes) {
            World loaded = w;
            std::vector<SaveIssue> issues;
            const SaveStatus s = load(loaded, bytes, issues);
            CHECK(World::equivalent(loaded, w)); // refused: nothing changed
            return s;
        };
        std::vector<std::byte> cut(good.begin(), good.end() - 5);
        CHECK(status(cut) == SaveStatus::Truncated);
        CHECK(status(std::vector<std::byte>(good.begin(), good.begin() + 4)) == SaveStatus::Truncated);
        std::vector<std::byte> magic = good;
        magic[0] ^= std::byte{1};
        CHECK(status(magic) == SaveStatus::BadHeader);
        SaveImage newer = image;
        newer.format = kSaveFormat + 1;
        CHECK(status(write_save(newer)) == SaveStatus::NewerFormat);
        // The first section's u64 length, just before its crc, made enormous: refused as
        // Truncated before anything is allocated for it.
        std::vector<std::byte> huge = good;
        const std::size_t length_at =
            payload_offset(image, 0) - sizeof(std::uint32_t) - sizeof(std::uint64_t);
        std::fill(huge.begin() + static_cast<std::ptrdiff_t>(length_at),
                  huge.begin() + static_cast<std::ptrdiff_t>(length_at + sizeof(std::uint64_t)),
                  std::byte{0xFF});
        CHECK(status(huge) == SaveStatus::Truncated);
    }

    TEST_CASE("an unknown section id is skipped with an issue") {
        World w(kSeed, small_params());
        w.step(Action::wait());
        SaveImage image = w.save(kBuild);
        image.sections.push_back({section_id("ZZZZ"), 1, SyncScope::World, payload_of({9, 9})});
        World loaded(kSeed + 1, small_params(0));
        std::vector<SaveIssue> issues;
        REQUIRE(load(loaded, write_save(image), issues) == SaveStatus::Ok);
        CHECK(has_issue(issues, section_id("ZZZZ")));
        CHECK(World::equivalent(loaded, w));
    }

    TEST_CASE("World::save is deterministic, scoped, and a restored World saves the same bytes") {
        constexpr int kActions = 7;
        World w(kSeed, small_params());
        Rng pick(kSeed);
        for (int i = 0; i < kActions; ++i) {
            w.step(mixed(pick));
        }
        const SaveImage image = w.save(kBuild);
        const std::vector<std::byte> bytes = write_save(image);
        CHECK(write_save(w.save(kBuild)) == bytes);
        for (const SaveSection& s : image.sections) {
            CAPTURE(section_name(s.id));
            CHECK(s.scope == (s.id == section_id("CHAR") ? SyncScope::Character : SyncScope::World));
        }
        World loaded(kSeed + 1, small_params(0));
        std::vector<SaveIssue> issues;
        REQUIRE(load(loaded, bytes, issues) == SaveStatus::Ok);
        CHECK(write_save(loaded.save(kBuild)) == bytes);
    }

    TEST_CASE("an image whose player stands on a wall is Corrupt and changes nothing") {
        const World w(kSeed, small_params());
        SaveImage image = w.save(kBuild);
        const Grid<bool>& blocked = w.stage().blocked;
        Vec2i wall{};
        for (int y = 0; y < blocked.height() && wall == Vec2i{}; ++y) {
            for (int x = 0; x < blocked.width(); ++x) {
                if (blocked.at(x, y)) {
                    wall = {x, y};
                    break;
                }
            }
        }
        REQUIRE(blocked.at(wall));
        for (SaveSection& s : image.sections) {
            if (s.id == section_id("CHAR")) {
                ByteWriter c;
                c.u32(1);
                c.vec2i(wall);
                s.payload = c.take();
            }
        }
        World target(kSeed + 1, small_params());
        const World before = target;
        std::vector<SaveIssue> issues;
        CHECK(target.restore(image, issues) == SaveStatus::Corrupt);
        CHECK(has_issue(issues, section_id("CHAR")));
        CHECK(World::equivalent(target, before));
    }

    TEST_CASE("every forged World value is refused and changes nothing") {
        // PEO-091: each case changes one field of a real save at its offset in the
        // section's payload; restore must say Corrupt, name the section, and leave the
        // World as it was. The dead_cycle case once divided by zero in the constructor.
        World w(kSeed, small_params());
        w.step(Action::step({1, 0}, kRunStepSubsteps)); // mid-period: the log holds a tile
        const SaveImage image = w.save(kBuild);
        const auto put32 = [](std::vector<std::byte>& p, std::size_t at, std::uint32_t v) {
            for (std::size_t i = 0; i < sizeof v; ++i) {
                p[at + i] = static_cast<std::byte>((v >> (8 * i)) & 0xFFU);
            }
        };
        const auto section = [&](const char* id) -> const SaveSection& {
            return *std::find_if(image.sections.begin(), image.sections.end(),
                                 [&](const SaveSection& s) { return s.id == section_id(id); });
        };
        // Where the first closed move of an open cell next to a wall sits in HORD.
        const auto closed_move = [&]() -> std::size_t {
            const std::vector<std::byte>& p = section("HORD").payload;
            ByteReader r(p);
            const std::uint32_t units = r.u32();
            r.bytes(units * kDeadBytes);
            const std::uint32_t pending = r.u32();
            const std::size_t odds =
                kCountBytes + units * kDeadBytes + kCountBytes + pending * kMoveBytes + kVectorCountBytes;
            const Grid<bool>& b = w.stage().blocked;
            for (int y = 1; y < b.height() - 1; ++y) {
                for (int x = 1; x < b.width() - 1; ++x) {
                    for (std::size_t d = 0; d < std::size(kNeighbours8) && !b.at(x, y); ++d) {
                        if (b.at(Vec2i{x, y} + kNeighbours8[d])) {
                            return odds + (static_cast<std::size_t>(y * b.width() + x) * kDrawChoices) + d;
                        }
                    }
                }
            }
            return 0;
        };
        struct Forgery {
            const char* name;
            const char* section;
            std::size_t at;
            std::uint32_t value;
            bool byte = false;
        };
        const Forgery forgeries[] = {
            {"dead_cycle near the max", "WRLD", 48, 0xFFFFFFFFU},
            {"dead_cycle odd", "WRLD", 48, 19},
            {"update_period huge", "WRLD", 44, 0xFFFFFFFEU},
            {"stage width huge", "WRLD", 36, 1U << 20},
            {"scent speed 0", "WRLD", 24, 0},
            {"one of the Dead off the stage", "HORD", 4, 0xFFFFFFFFU},
            {"a step of an odd substep count", "HORD", 12, 3, true},
            {"a scent value above the strongest", "SCNT", 12, 0x7FFFFFFFU},
            {"a scent update count off the clock", "SCNT", 0, 5},
            {"logged substeps off the clock", "OCCL", 12, 7},
            {"two characters", "CHAR", 0, 2},
            {"a closed move offered", "HORD", closed_move(), 0, true},
        };
        for (const Forgery& f : forgeries) {
            CAPTURE(f.name);
            REQUIRE((f.at > 0 || !f.byte));
            SaveImage forged = image;
            for (SaveSection& s : forged.sections) {
                if (s.id == section_id(f.section)) {
                    if (f.byte) {
                        s.payload[f.at] = static_cast<std::byte>(f.value);
                    } else {
                        put32(s.payload, f.at, f.value);
                    }
                }
            }
            World target(kSeed + 1, small_params());
            const World before = target;
            std::vector<SaveIssue> issues;
            CHECK(target.restore(forged, issues) == SaveStatus::Corrupt);
            CHECK(has_issue(issues, section_id(f.section)));
            CHECK(World::equivalent(target, before));
        }
    }

    TEST_CASE("a speculation from before a restore never commits after it") {
        // PEO-091: same stage, same update, but another epoch: commit falls back to step.
        World w(kSeed, small_params());
        const SaveImage image = w.save(kBuild);
        Speculation stale = w.speculate();
        std::vector<SaveIssue> issues;
        REQUIRE(w.restore(image, issues) == SaveStatus::Ok);
        World plain = w;
        w.commit(stale, Action::wait());
        plain.step(Action::wait());
        CHECK(World::equivalent(w, plain));
        CHECK(stale.update == 0); // untouched: it was never consumed
    }

    TEST_CASE("a hand-built stage saves whole and restores") {
        const Layout l = small_layout();
        World w(kSeed, small_params(0));
        w.load_layout(l.stage, l.player, l.horde);
        w.step(Action::wait());
        const SaveImage image = w.save(kBuild);
        CHECK(std::any_of(image.sections.begin(), image.sections.end(),
                          [](const SaveSection& s) { return s.id == section_id("STAG"); }));
        World loaded(kSeed + 1, small_params());
        std::vector<SaveIssue> issues;
        REQUIRE(loaded.restore(image, issues) == SaveStatus::Ok);
        CHECK(World::equivalent(loaded, w));
        CHECK(loaded.stage().exit == kNoExit);
    }
}

TEST_SUITE("scenario: save") {
    TEST_CASE("a restored world continues bit-identical to the one never saved") {
        // PEO-091: a generated stage with wind and 240 Dead, committing speculations
        // after the restore; and a hand-built stage, stepping.
        constexpr int kBefore = 40;
        constexpr int kAfter = 40;
        constexpr int kDead = 240;
        WorldParams windy{.initial_dead = kDead, .stage_width = 40, .stage_height = 24};
        windy.wind_max = kWindFull;
        windy.scent.gust = 2;
        World a(kSeed, windy);
        REQUIRE(a.wind().intensity > 0);
        check_continuation(a, kBefore, kAfter, true);
        const Layout l = small_layout();
        World layout(kSeed, small_params(0));
        layout.load_layout(l.stage, l.player, l.horde);
        check_continuation(layout, kBefore, kAfter, false);
    }
}
