// PEO-135: peo_peo_dump <file.peo>... prints each file's records as canonical lines,
// `<record path>\t<field path>\t<kind>\t<value>`, then its errors as `error\t<message>`; exit 1
// on any error. tools/content/peo.py print_lines prints the same, and
// tools/content/peo_conformance.py diffs both against tests/content/peo/*.expect.
#include "peo/core/peo_format.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using peo::core::PeoKind;
using peo::core::PeoValue;

constexpr int kExitError = 1;

/// Backslash, tab and newline escaped, as golden.py and peo.py do.
std::string escaped(const std::string& text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\n':
            out += "\\n";
            break;
        default:
            out += c;
        }
    }
    return out;
}

void value_lines(const std::string& record, const std::string& path, const PeoValue& v,
                 std::vector<std::string>& out) {
    const auto line = [&](const char* kind, const std::string& text) {
        out.push_back(record + "\t" + path + "\t" + kind + "\t" + text);
    };
    const auto child = [&](const std::string& key) { return path.empty() ? key : path + "." + key; };
    switch (v.kind) {
    case PeoKind::Block:
        if (v.fields.empty()) {
            line("block", "{}");
        }
        for (const peo::core::PeoField& f : v.fields) {
            value_lines(record, child(f.key), f.value, out);
        }
        return;
    case PeoKind::List:
        if (v.items.empty()) {
            line("list", "[]");
        }
        for (std::size_t i = 0; i < v.items.size(); ++i) {
            value_lines(record, child(std::to_string(i)), v.items[i], out);
        }
        return;
    case PeoKind::Integer:
        line("integer", std::to_string(v.number));
        return;
    case PeoKind::Quantity:
        line("quantity", std::to_string(v.number) + " " + peo::core::dimension_name(v.dimension));
        return;
    case PeoKind::Range: {
        std::string text = std::to_string(v.number) + ".." + std::to_string(v.high);
        if (v.has_dimension) {
            text += std::string(" ") + peo::core::dimension_name(v.dimension);
        }
        line("range", text);
        return;
    }
    case PeoKind::Boolean:
        line("boolean", v.boolean ? "yes" : "no");
        return;
    case PeoKind::String:
        line(v.formatted ? "formatted" : "string", escaped(v.text));
        return;
    case PeoKind::Path:
        line("path", v.text);
        return;
    case PeoKind::Expr:
        line("expr", escaped(v.text));
        return;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: peo_peo_dump <file.peo>...\n");
        return kExitError;
    }
    bool failed = false;
    for (int a = 1; a < argc; ++a) {
        std::ifstream in(argv[a], std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "peo_peo_dump: %s cannot be read\n", argv[a]);
            return kExitError;
        }
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const peo::core::PeoParse parsed = peo::core::parse_peo({argv[a], text});
        std::vector<std::string> lines;
        for (const peo::core::PeoRecord& rec : parsed.records) {
            lines.push_back(rec.path + "\t\trecord\t" + (rec.abstract ? "abstract" : "concrete"));
            value_lines(rec.path, rec.one_line ? "=" : "", rec.value, lines);
        }
        for (const std::string& e : parsed.errors) {
            lines.push_back("error\t" + e);
        }
        for (const std::string& line : lines) {
            std::fwrite(line.data(), 1, line.size(), stdout);
            std::fputc('\n', stdout);
        }
        failed = failed || !parsed.errors.empty();
    }
    return failed ? kExitError : 0;
}
