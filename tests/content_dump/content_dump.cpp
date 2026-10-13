// PEO-008: peo_content_dump <content_dir> prints every resolved item as canonical lines,
// `<item id>\t<dotted path>\t<value>`, items by ID, keys sorted, arrays by index.
// tools/content/golden.py prints the same lines from derive.resolve_item and diffs the two
// (ctest content_golden). Core opens no file, so reading content/ is this tool's job.
#include "peo/core/content.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kExitError = 1;

/// Backslash, tab and newline escaped, so a value never breaks a line or a column.
std::string escaped(std::string_view text) {
    std::string out;
    out.reserve(text.size());
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

/// Python's repr of a float for the values content holds: shortest round trip, ".0" kept.
std::string float_text(double value) {
    std::array<char, 64> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    std::string text(buffer.data(), error == std::errc{} ? end : buffer.data());
    if (text.find_first_of(".eni") == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::string value_text(const peo::core::ContentLeaf& leaf) {
    using Kind = peo::core::ContentLeaf::Kind;
    switch (leaf.kind) {
    case Kind::Integer:
        return std::to_string(leaf.integer);
    case Kind::Boolean:
        return leaf.boolean ? "true" : "false";
    case Kind::String:
        return escaped(leaf.text);
    case Kind::Float:
        return float_text(leaf.floating);
    case Kind::Other:
        break;
    }
    return "?";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: peo_content_dump <content_dir>\n");
        return kExitError;
    }
    const fs::path dir = argv[1];
    std::error_code error;
    if (!fs::is_directory(dir, error)) {
        std::fprintf(stderr, "peo_content_dump: %s is not a directory\n", dir.string().c_str());
        return kExitError;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".toml") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    std::vector<std::string> paths;
    std::vector<std::string> texts;
    paths.reserve(files.size());
    texts.reserve(files.size());
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        paths.push_back(file.generic_string());
        texts.push_back(text.str());
    }
    std::vector<peo::core::ContentSource> sources;
    sources.reserve(files.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        sources.push_back({paths[i], texts[i]});
    }

    const peo::core::ContentLoad load = peo::core::load_content(sources);
    if (!load.content) {
        for (const std::string& message : load.errors) {
            std::fprintf(stderr, "%s\n", message.c_str());
        }
        return kExitError;
    }
    const peo::core::Content& content = *load.content;
    std::string line;
    for (std::uint32_t i = 0; i < content.item_count(); ++i) {
        const peo::core::ItemId item{i};
        const std::string id = escaped(content.item_id(item));
        content.for_each_leaf(item, [&](const peo::core::ContentLeaf& leaf) {
            line.assign(id);
            line += '\t';
            line += escaped(leaf.path);
            line += '\t';
            line += value_text(leaf);
            line += '\n';
            std::fwrite(line.data(), 1, line.size(), stdout);
        });
    }
    return 0;
}
