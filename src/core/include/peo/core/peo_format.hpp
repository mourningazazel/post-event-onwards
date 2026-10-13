#pragma once

// PEO-135: a .peo content file parsed into records (D-057, docs/design/content-format.md
// 'Grammar'). tools/content/peo.py is the same parser in Python; tests/content/peo/ holds the
// cases both must agree on. Parsing only: what a header path's prefix means (the parent chain)
// and what an Expr says are resolution (PEO-136) and PEO-132. Core parses text it is handed
// and opens no file.

#include "peo/core/content.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace peo::core {

enum class PeoKind : std::uint8_t {
    Integer,  ///< a plain integer
    Quantity, ///< an integer in its dimension's base unit
    Range,    ///< two Integers, or two Quantities of one dimension
    Boolean,  ///< yes / no
    String,   ///< its text after escapes and indent trimming; `formatted` when """..."""
    Path,     ///< a bare dotted name
    Expr,     ///< a query, call or $role, kept as written (PEO-132 parses it)
    List,
    Block, ///< fields in file order
};

/// A quantity's dimension and its base unit: mm, mm2, g, ml, s, whole C, kg/m3, %.
enum class PeoDimension : std::uint8_t { Length, Area, Mass, Volume, Time, Temperature, Density, Percent };

/// Its name in canonical lines and messages ("length").
[[nodiscard]] const char* dimension_name(PeoDimension dimension) noexcept;

struct PeoField;

/// One value. Only the members its kind names are set.
struct PeoValue {
    PeoKind kind = PeoKind::Integer;
    /// Integer and Quantity: the value; Range: the low end.
    std::int64_t number = 0;
    /// Range: the high end.
    std::int64_t high = 0;
    /// Quantity, and a Range of Quantities.
    bool has_dimension = false;
    PeoDimension dimension = PeoDimension::Length;
    bool boolean = false;
    /// String, Path and Expr.
    std::string text;
    /// A String written as """...""".
    bool formatted = false;
    std::vector<PeoValue> items;
    std::vector<PeoField> fields;
};

struct PeoField {
    std::string key;
    PeoValue value;
};

struct PeoRecord {
    /// The header path, whole.
    std::string path;
    bool abstract = false;
    /// A Block, or any value for the one-line `path = value` form.
    PeoValue value;
    /// Written as `path = value`.
    bool one_line = false;
    std::uint32_t line = 0;
};

struct PeoParse {
    std::vector<PeoRecord> records;
    /// `<path>:<line>:<col>: <message>`, sorted by position. A record with an error is left
    /// out and parsing goes on at the next record (the next word in column 1).
    std::vector<std::string> errors;
};

/// Parse one .peo source. Columns count characters, not bytes. Deterministic; no I/O.
[[nodiscard]] PeoParse parse_peo(const ContentSource& source);

} // namespace peo::core
