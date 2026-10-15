#pragma once

// PEO-008: the frontend reads content/ for core, which opens no file. Every *.toml under the
// directory, whole, in sorted path order (as tools/content/common.py load), so the same tree
// gives the same Content.

#include "peo/core/content.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace peo::app {

/// The files read, owning their paths and texts; sources() views them for load_content.
struct ContentFiles {
    std::vector<std::string> paths;
    std::vector<std::string> texts;

    [[nodiscard]] std::vector<core::ContentSource> sources() const {
        std::vector<core::ContentSource> out;
        out.reserve(paths.size());
        for (std::size_t i = 0; i < paths.size(); ++i) {
            out.push_back({paths[i], texts[i]});
        }
        return out;
    }
};

/// Read every file ending in `extension` under `dir` (*.toml unless told otherwise; PEO-142
/// reads *.peo too), or nullopt with `error` set when `dir` is not a directory or a file cannot
/// be read.
[[nodiscard]] inline std::optional<ContentFiles> read_content_files(const std::filesystem::path& dir,
                                                                    std::string& error,
                                                                    std::string_view extension = ".toml") {
    namespace fs = std::filesystem;
    std::error_code code;
    if (!fs::is_directory(dir, code)) {
        error = dir.generic_string() + " is not a directory";
        return std::nullopt;
    }
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(dir, code), end; !code && it != end; it.increment(code)) {
        if (it->is_regular_file(code) && it->path().extension() == extension) {
            files.push_back(it->path());
        }
    }
    if (code) {
        error = dir.generic_string() + ": " + code.message();
        return std::nullopt;
    }
    std::sort(files.begin(), files.end());
    ContentFiles out;
    out.paths.reserve(files.size());
    out.texts.reserve(files.size());
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            error = file.generic_string() + " cannot be read";
            return std::nullopt;
        }
        // Through a stringstream, not istreambuf_iterator: GCC 13 at -O2 flags the iterator
        // with -Wnull-dereference inside libstdc++.
        std::ostringstream text;
        text << in.rdbuf();
        out.paths.push_back(file.generic_string());
        out.texts.push_back(std::move(text).str());
    }
    return out;
}

} // namespace peo::app
