// PEO-123: HUD lines wrapped to the window's width (src/app/hud.hpp, header-only, no SDL).
#include <doctest/doctest.h>

#include <array>
#include <string>
#include <string_view>

#include "hud.hpp"

using namespace peo::app;

namespace {

constexpr std::size_t kSpans = 8;
using Spans = std::array<HudSpan, kSpans>;

/// The text of span `s` of `line`.
std::string_view text(std::string_view line, HudSpan s) {
    return line.substr(static_cast<std::size_t>(s.begin), static_cast<std::size_t>(s.length));
}

/// `line` with every space removed: what wrapping must keep, in order.
std::string without_spaces(std::string_view line) {
    std::string out;
    for (const char c : line) {
        if (c != ' ') {
            out += c;
        }
    }
    return out;
}

// The constexpr contract: usable at compile time, like camera.hpp.
constexpr int compile_time_rows() {
    std::array<HudSpan, 2> out{};
    return hud_wrap("aa bb cc", 5, out);
}
static_assert(compile_time_rows() == 2);

} // namespace

TEST_SUITE("hud") {
    TEST_CASE("a line that fits is one span covering it all") {
        Spans out{};
        constexpr std::string_view line = "abc def";
        CHECK(hud_wrap(line, 10, out) == 1);
        CHECK(text(line, out[0]) == line);
        CHECK(hud_wrap(line, static_cast<int>(line.size()), out) == 1); // exactly cols long
        CHECK(text(line, out[0]) == line);
    }

    TEST_CASE("words break at the space, which is dropped") {
        Spans out{};
        constexpr std::string_view line = "aa bb cc";
        REQUIRE(hud_wrap(line, 5, out) == 2);
        CHECK(text(line, out[0]) == "aa bb");
        CHECK(text(line, out[1]) == "cc");
    }

    TEST_CASE("a double space is kept inside a row and dropped whole at a break") {
        Spans out{};
        constexpr std::string_view inside = "aa  bb";
        REQUIRE(hud_wrap(inside, 6, out) == 1);
        CHECK(text(inside, out[0]) == "aa  bb");
        constexpr std::string_view at_break = "aa bb  cc";
        REQUIRE(hud_wrap(at_break, 6, out) == 2);
        CHECK(text(at_break, out[0]) == "aa bb");
        CHECK(text(at_break, out[1]) == "cc");
    }

    TEST_CASE("a word longer than the row is broken at cols") {
        Spans out{};
        constexpr std::string_view line = "abcdefg";
        REQUIRE(hud_wrap(line, 3, out) == 3);
        CHECK(text(line, out[0]) == "abc");
        CHECK(text(line, out[1]) == "def");
        CHECK(text(line, out[2]) == "g");
    }

    TEST_CASE("cols below 1 behaves as 1; an empty line is one empty row") {
        Spans zero{};
        Spans one{};
        constexpr std::string_view line = "ab c";
        const int rows = hud_wrap(line, 0, zero);
        CHECK(rows == hud_wrap(line, 1, one));
        CHECK(rows == 3);
        for (int r = 0; r < rows; ++r) {
            CHECK(text(line, zero[static_cast<std::size_t>(r)]) ==
                  text(line, one[static_cast<std::size_t>(r)]));
        }
        Spans out{};
        CHECK(hud_wrap("", 10, out) == 1);
        CHECK(out[0].length == 0);
    }

    TEST_CASE("the row count is the whole line's even when out is shorter") {
        std::array<HudSpan, 2> out{};
        constexpr std::string_view line = "aa bb cc dd";
        CHECK(hud_wrap(line, 2, out) == 4);
        CHECK(text(line, out[0]) == "aa");
        CHECK(text(line, out[1]) == "bb");
    }

    TEST_CASE("every width keeps every character in order, and no row passes cols or edges on a space") {
        const std::string_view line =
            "stage 12  turn 3456  walk  dead 266  wind to SW 4  spec:hit 11.854ms x";
        REQUIRE(line.size() == 70);
        std::array<HudSpan, 80> out{};
        for (int cols = 1; cols <= 80; ++cols) {
            CAPTURE(cols);
            const int rows = hud_wrap(line, cols, out);
            REQUIRE(rows <= static_cast<int>(out.size()));
            std::string joined;
            for (int r = 0; r < rows; ++r) {
                const std::string_view row = text(line, out[static_cast<std::size_t>(r)]);
                CHECK(static_cast<int>(row.size()) <= cols);
                REQUIRE_FALSE(row.empty());
                CHECK(row.front() != ' ');
                CHECK(row.back() != ' ');
                joined += row;
                if (r + 1 < rows) {
                    // Greedy: the row could not also take the next row's first word.
                    const HudSpan here = out[static_cast<std::size_t>(r)];
                    const auto next = static_cast<std::size_t>(out[static_cast<std::size_t>(r) + 1].begin);
                    const std::size_t word_end = std::min(line.find(' ', next), line.size());
                    CHECK(word_end - static_cast<std::size_t>(here.begin) > static_cast<std::size_t>(cols));
                }
            }
            CHECK(without_spaces(joined) == without_spaces(line));
        }
    }
}
