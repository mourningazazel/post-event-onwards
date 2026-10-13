// PEO-135: the .peo parser (peo/core/peo_format.hpp), over in-memory sources. Record and field
// names are test data only. tests/content/peo/ holds the cases both parsers must agree on.
#include "peo/core/peo_format.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace peo::core;

namespace {

PeoParse parse(std::string_view text) {
    return parse_peo({"t.peo", text});
}

/// The value at a dotted field path inside a block.
const PeoValue* at(const PeoValue& block, std::string_view path) {
    const PeoValue* cur = &block;
    while (!path.empty()) {
        const std::size_t dot = path.find('.');
        const std::string_view key = path.substr(0, dot);
        const PeoValue* next = nullptr;
        if (cur->kind == PeoKind::List) {
            const auto index = static_cast<std::size_t>(std::stoul(std::string(key)));
            next = index < cur->items.size() ? &cur->items[index] : nullptr;
        } else {
            for (const PeoField& f : cur->fields) {
                if (f.key == key) {
                    next = &f.value;
                }
            }
        }
        if (next == nullptr) {
            return nullptr;
        }
        cur = next;
        path = dot == std::string_view::npos ? std::string_view{} : path.substr(dot + 1);
    }
    return cur;
}

/// The one record of `text`, which must parse with no error.
PeoValue only(std::string_view text) {
    PeoParse p = parse(text);
    REQUIRE(p.errors.empty());
    REQUIRE(p.records.size() == 1);
    return p.records[0].value;
}

void check_quantity(const PeoValue& block, std::string_view path, std::int64_t value,
                    PeoDimension dimension) {
    CAPTURE(path);
    const PeoValue* v = at(block, path);
    REQUIRE(v != nullptr);
    CHECK(v->kind == PeoKind::Quantity);
    CHECK(v->number == value);
    CHECK(v->dimension == dimension);
}

/// One error, at `line:col`, saying `what`; and the record after the bad one parsed.
void check_one_error(std::string_view text, std::string_view where, std::string_view what) {
    CAPTURE(text);
    const PeoParse p = parse(text);
    REQUIRE(p.errors.size() == 1);
    CHECK(p.errors[0].starts_with(std::string("t.peo:") + std::string(where) + ": "));
    CHECK(p.errors[0].find(what) != std::string::npos);
    REQUIRE_FALSE(p.records.empty());
    CHECK(p.records.back().path == "item.after");
}

} // namespace

TEST_SUITE("peo_format") {
    TEST_CASE("each value kind, and records in both forms") {
        const PeoParse p = parse("abstract item.a {\n"
                                 "    n = 7, q = 570 g, r = 1..3, yes_ = yes, no_ = no,\n"
                                 "    s = \"x\", f = \"\"\"y\"\"\", p = item.b.c, e = $x.use(),\n"
                                 "    l = [1, 2], b { k = 1 }\n"
                                 "}\n"
                                 "remap.item.z = item.a\n");
        REQUIRE(p.errors.empty());
        REQUIRE(p.records.size() == 2);
        const PeoRecord& a = p.records[0];
        CHECK(a.path == "item.a");
        CHECK(a.abstract);
        CHECK_FALSE(a.one_line);
        CHECK(a.line == 1);
        CHECK(at(a.value, "n")->kind == PeoKind::Integer);
        CHECK(at(a.value, "n")->number == 7);
        check_quantity(a.value, "q", 570, PeoDimension::Mass);
        CHECK(at(a.value, "r")->kind == PeoKind::Range);
        CHECK(at(a.value, "r")->high == 3);
        CHECK(at(a.value, "yes_")->boolean);
        CHECK_FALSE(at(a.value, "no_")->boolean);
        CHECK(at(a.value, "s")->kind == PeoKind::String);
        CHECK_FALSE(at(a.value, "s")->formatted);
        CHECK(at(a.value, "f")->formatted);
        CHECK(at(a.value, "p")->kind == PeoKind::Path);
        CHECK(at(a.value, "p")->text == "item.b.c");
        CHECK(at(a.value, "e")->kind == PeoKind::Expr);
        CHECK(at(a.value, "l.1")->number == 2);
        CHECK(at(a.value, "b.k")->number == 1);
        const PeoRecord& z = p.records[1];
        CHECK(z.one_line);
        CHECK(z.value.kind == PeoKind::Path);
        CHECK(z.value.text == "item.a");
    }

    TEST_CASE("a dotted lhs nests, and merges with a block of the same name") {
        const PeoValue v = only("item.a {\n"
                                "    features.grip.hands = 2,\n"
                                "    features { grip { girth = 30 mm }, edge { sharpness = 4 } }\n"
                                "}\n");
        CHECK(at(v, "features.grip.hands")->number == 2);
        check_quantity(v, "features.grip.girth", 30, PeoDimension::Length);
        CHECK(at(v, "features.edge.sharpness")->number == 4);
        CHECK(at(v, "features")->fields.size() == 2);
    }

    TEST_CASE("units convert to the base unit, rounding half away from zero") {
        const PeoValue v = only("def.u { a = 1.5 cm, b = 10 lb, c = 1 gal, d = 2 h, e = 212 F, f = 1..3 h,"
                                " g = 12.5%, h = -40 F, i = 950 kg/m3 }");
        check_quantity(v, "a", 15, PeoDimension::Length);
        check_quantity(v, "b", 4536, PeoDimension::Mass);
        check_quantity(v, "c", 3785, PeoDimension::Volume);
        check_quantity(v, "d", 7200, PeoDimension::Time);
        check_quantity(v, "e", 100, PeoDimension::Temperature);
        check_quantity(v, "g", 13, PeoDimension::Percent);
        check_quantity(v, "h", -40, PeoDimension::Temperature);
        check_quantity(v, "i", 950, PeoDimension::Density);
        const PeoValue* f = at(v, "f");
        REQUIRE(f != nullptr);
        CHECK(f->kind == PeoKind::Range);
        CHECK(f->number == 3600); // a range takes the unit of either end
        CHECK(f->high == 10800);
        CHECK(f->dimension == PeoDimension::Time);
    }

    TEST_CASE("a verbatim string keeps what was typed; a triple string reads escapes and trims") {
        const PeoValue v = only("text.a {\n"
                                "    plain = \"<b>bold</b> and a back\\slash\",\n"
                                "    formatted = \"\"\"\n"
                                "        First \\\"quote\\\"\n"
                                "          indented \\\\ and\\ttab\\nsplit\n"
                                "    \"\"\"\n"
                                "}\n");
        CHECK(at(v, "plain")->text == "<b>bold</b> and a back\\slash");
        CHECK(at(v, "formatted")->text == "First \"quote\"\n  indented \\ and\ttab\nsplit");
    }

    TEST_CASE("each error is one error at its line and column, and the next record parses") {
        check_one_error("item.a { dims { l = 6 ft } }\nitem.after { v = 1 }\n", "1:23", "lengths are metric");
        check_one_error("text.a { t = \"\"\"bad \\q\"\"\" }\nitem.after { v = 1 }\n", "1:21",
                        "unknown escape");
        check_one_error("item.a {\n    m = 5 g\n    v = 3\n}\nitem.after { v = 1 }\n", "3:5", "expected ','");
        check_one_error("item.a { v = 3, }\nitem.after { v = 1 }\n", "1:15", "no comma before '}'");
        check_one_error("item.a { t = [a, b,] }\nitem.after { v = 1 }\n", "1:19", "no comma before ']'");
        check_one_error("item.a {\n    v = 3\nitem.after { v = 1 }\n", "3:1", "is the '{' at 1:8 closed?");
        check_one_error("item.a { d { l = 1 mm }, d.l = 2 mm }\nitem.after { v = 1 }\n", "1:28", "set twice");
        check_one_error("item.a { v = 1 }\n@ stray\nitem.after { v = 1 }\n", "2:1", "expected a record path");
    }

    TEST_CASE("an expression is kept as written, balanced, up to the comma that ends it") {
        const PeoValue v =
            only("mechanic.a {\n"
                 "    source = self.carried.**{ material ~ item.material.wood.**, dims.l >= 150 mm },\n"
                 "    effects = [$tool.durability.reduce(1), item.x{ a = \"}\" }.make(self.hands)]\n"
                 "}\n");
        CHECK(at(v, "source")->kind == PeoKind::Expr);
        CHECK(at(v, "source")->text ==
              "self.carried.**{ material ~ item.material.wood.**, dims.l >= 150 mm }");
        CHECK(at(v, "effects.0")->text == "$tool.durability.reduce(1)");
        CHECK(at(v, "effects.1")->text == "item.x{ a = \"}\" }.make(self.hands)");
    }

    TEST_CASE("the same source parses the same twice") {
        constexpr std::string_view kText =
            "item.a { v = 1, l = [x.y, 2 kg], e = $q.r(1) }\nitem.b = item.a\n";
        const PeoParse a = parse(kText);
        const PeoParse b = parse(kText);
        REQUIRE(a.records.size() == b.records.size());
        CHECK(a.errors == b.errors);
        CHECK(at(a.records[0].value, "l.1")->number == at(b.records[0].value, "l.1")->number);
        CHECK(at(a.records[0].value, "e")->text == at(b.records[0].value, "e")->text);
    }
}
