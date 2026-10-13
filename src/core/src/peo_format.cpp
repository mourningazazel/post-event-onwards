// PEO-135: the .peo parser (peo/core/peo_format.hpp). tools/content/peo.py is the same parser,
// function for function; keep the two in step (tests/content/peo/ checks they agree).
#include "peo/core/peo_format.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace peo::core {

const char* dimension_name(PeoDimension dimension) noexcept {
    switch (dimension) {
    case PeoDimension::Length:
        return "length";
    case PeoDimension::Area:
        return "area";
    case PeoDimension::Mass:
        return "mass";
    case PeoDimension::Volume:
        return "volume";
    case PeoDimension::Time:
        return "time";
    case PeoDimension::Temperature:
        return "temperature";
    case PeoDimension::Density:
        return "density";
    case PeoDimension::Percent:
        return "percent";
    }
    return "?";
}

namespace {

// ---------------------------------------------------------------- units
struct Unit {
    std::string_view name;
    PeoDimension dimension;
    std::int64_t num; ///< exact factor to the base unit, reduced
    std::int64_t den;
};
// As tools/content/peo.py UNITS. F is (x - 32) * 5 / 9, handled apart.
constexpr std::array<Unit, 23> kUnits{{
    {"mm", PeoDimension::Length, 1, 1},
    {"cm", PeoDimension::Length, 10, 1},
    {"m", PeoDimension::Length, 1000, 1},
    {"km", PeoDimension::Length, 1000000, 1},
    {"mm2", PeoDimension::Area, 1, 1},
    {"cm2", PeoDimension::Area, 100, 1},
    {"m2", PeoDimension::Area, 1000000, 1},
    {"g", PeoDimension::Mass, 1, 1},
    {"kg", PeoDimension::Mass, 1000, 1},
    {"oz", PeoDimension::Mass, 226796185, 8000000},
    {"lb", PeoDimension::Mass, 45359237, 100000},
    {"ml", PeoDimension::Volume, 1, 1},
    {"l", PeoDimension::Volume, 1000, 1},
    {"fl_oz", PeoDimension::Volume, 473176473, 16000000},
    {"gal", PeoDimension::Volume, 473176473, 125000},
    {"s", PeoDimension::Time, 1, 1},
    {"min", PeoDimension::Time, 60, 1},
    {"h", PeoDimension::Time, 3600, 1},
    {"d", PeoDimension::Time, 86400, 1},
    {"C", PeoDimension::Temperature, 1, 1},
    {"F", PeoDimension::Temperature, 5, 9},
    {"kg/m3", PeoDimension::Density, 1, 1},
    {"%", PeoDimension::Percent, 1, 1},
}};
constexpr std::array<std::string_view, 4> kImperialLengths{"ft", "in", "yd", "mi"};
// A number with a unit has fewer digits than this and at most this many decimals, so its
// conversion stays inside int64.
constexpr std::int64_t kMaxUnitMantissa = 1000000000;
constexpr int kMaxUnitDecimals = 6;
constexpr std::int64_t kFahrenheitOffset = 32;

const Unit* find_unit(std::string_view name) noexcept {
    for (const Unit& unit : kUnits) {
        if (unit.name == name) {
            return &unit;
        }
    }
    return nullptr;
}

/// num / den rounded half away from zero (derive.rnd), den > 0.
std::int64_t round_half_away(std::int64_t num, std::int64_t den) noexcept {
    if (num >= 0) {
        return (2 * num + den) / (2 * den);
    }
    return -((-2 * num + den) / (2 * den));
}

std::int64_t pow10(int n) noexcept {
    std::int64_t out = 1;
    for (int i = 0; i < n; ++i) {
        out *= 10;
    }
    return out;
}

// ---------------------------------------------------------------- lexer
enum class Tok : std::uint8_t { Word, Number, String, Punct, Eof, Bad };

struct Token {
    Tok kind;
    std::string text;
    std::uint32_t line;
    std::uint32_t col;
    std::size_t start;
    std::size_t end;
    std::string string_text; ///< a String token's text
    bool formatted = false;
};

struct Error {
    std::uint32_t line;
    std::uint32_t col;
    std::string text;
};

bool is_word_start(char c) noexcept {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool is_word_char(char c) noexcept {
    return is_word_start(c) || (c >= '0' && c <= '9');
}
bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}
bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
bool is_blank(std::string_view s) noexcept {
    return std::all_of(s.begin(), s.end(), [](char c) { return c == ' ' || c == '\t'; });
}
std::string_view leading_space(std::string_view s) noexcept {
    std::size_t n = 0;
    while (n < s.size() && (s[n] == ' ' || s[n] == '\t')) {
        ++n;
    }
    return s.substr(0, n);
}

/// Drop a line break right after the opening quotes and a whitespace-only last line, then the
/// leading whitespace every non-blank line shares.
std::string trim_indent(std::string_view raw) {
    if (!raw.empty() && raw.front() == '\n') {
        raw.remove_prefix(1);
    }
    std::vector<std::string_view> lines;
    std::size_t from = 0;
    while (true) {
        const std::size_t at = raw.find('\n', from);
        if (at == std::string_view::npos) {
            lines.push_back(raw.substr(from));
            break;
        }
        lines.push_back(raw.substr(from, at - from));
        from = at + 1;
    }
    if (lines.size() > 1 && is_blank(lines.back())) {
        lines.pop_back();
    }
    std::optional<std::string_view> common;
    for (const std::string_view line : lines) {
        if (is_blank(line)) {
            continue;
        }
        const std::string_view indent = leading_space(line);
        if (!common) {
            common = indent;
            continue;
        }
        while (indent.substr(0, common->size()) != *common) {
            common->remove_suffix(1);
        }
    }
    const std::size_t cut = common ? common->size() : 0;
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += '\n';
        }
        if (!is_blank(lines[i])) {
            out += lines[i].substr(cut);
        }
    }
    return out;
}

/// Read \n, \t, \\ and \" (the lexer has refused any other escape).
std::string unescape(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            const char c = text[++i];
            out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
        } else {
            out += text[i];
        }
    }
    return out;
}

class Lexer {
public:
    Lexer(std::string_view path, std::string text, std::vector<Error>& errors)
        : path_(path), src_(std::move(text)), errors_(errors) {}

    [[nodiscard]] const std::string& source() const noexcept { return src_; }

    std::vector<Token> tokens() {
        std::vector<Token> out;
        while (true) {
            skip_space_and_comments();
            if (pos_ >= src_.size()) {
                out.push_back({Tok::Eof, "", line_, col_, pos_, pos_, {}, false});
                return out;
            }
            out.push_back(next_token());
        }
    }

private:
    void error(std::uint32_t line, std::uint32_t col, std::string_view message) {
        errors_.push_back({line, col,
                           std::string(path_) + ":" + std::to_string(line) + ":" + std::to_string(col) +
                               ": " + std::string(message)});
    }

    /// Move one character (a whole UTF-8 sequence), so columns count characters.
    void advance() {
        if (src_[pos_] == '\n') {
            ++line_;
            col_ = 1;
            ++pos_;
            return;
        }
        ++col_;
        ++pos_;
        while (pos_ < src_.size() && (static_cast<unsigned char>(src_[pos_]) & 0xC0U) == 0x80U) {
            ++pos_;
        }
    }
    void advance(int n) {
        for (int i = 0; i < n && pos_ < src_.size(); ++i) {
            advance();
        }
    }
    [[nodiscard]] char peek(std::size_t k = 0) const noexcept {
        return pos_ + k < src_.size() ? src_[pos_ + k] : '\0';
    }
    [[nodiscard]] bool starts_with(std::string_view s) const noexcept {
        return src_.compare(pos_, s.size(), s) == 0;
    }

    void skip_space_and_comments() {
        while (pos_ < src_.size()) {
            const char c = src_[pos_];
            if (is_space(c)) {
                advance();
            } else if (c == '#') {
                while (pos_ < src_.size() && src_[pos_] != '\n') {
                    advance();
                }
            } else {
                return;
            }
        }
    }

    Token make(Tok kind, std::uint32_t line, std::uint32_t col, std::size_t start) const {
        return {kind, src_.substr(start, pos_ - start), line, col, start, pos_, {}, false};
    }

    Token next_token() {
        const std::uint32_t line = line_;
        const std::uint32_t col = col_;
        const std::size_t start = pos_;
        const char c = peek();
        if (is_word_start(c)) {
            while (is_word_char(peek())) {
                advance();
            }
            // A unit like kg/m3: a slash and more word characters right after the word.
            if (peek() == '/' && is_word_char(peek(1))) {
                advance();
                while (is_word_char(peek())) {
                    advance();
                }
            }
            return make(Tok::Word, line, col, start);
        }
        if (is_digit(c)) {
            while (is_digit(peek())) {
                advance();
            }
            if (peek() == '.' && is_digit(peek(1))) {
                advance();
                while (is_digit(peek())) {
                    advance();
                }
            }
            return make(Tok::Number, line, col, start);
        }
        if (c == '"') {
            return starts_with("\"\"\"") ? triple_string(line, col, start)
                                         : verbatim_string(line, col, start);
        }
        if (c == '.' && peek(1) == '.') {
            advance(2);
            return make(Tok::Punct, line, col, start);
        }
        advance();
        return make(Tok::Punct, line, col, start);
    }

    Token verbatim_string(std::uint32_t line, std::uint32_t col, std::size_t start) {
        advance();
        const std::size_t body = pos_;
        while (pos_ < src_.size() && src_[pos_] != '"' && src_[pos_] != '\n') {
            advance();
        }
        if (pos_ >= src_.size() || src_[pos_] == '\n') {
            error(line, col, "a \"...\" string ends at the end of its line; use \"\"\"...\"\"\" for more");
            return make(Tok::Bad, line, col, start);
        }
        std::string text = src_.substr(body, pos_ - body);
        advance();
        Token t = make(Tok::String, line, col, start);
        t.string_text = std::move(text);
        return t;
    }

    Token triple_string(std::uint32_t line, std::uint32_t col, std::size_t start) {
        advance(3);
        const std::size_t body = pos_;
        bool bad = false;
        while (true) {
            if (pos_ >= src_.size()) {
                error(line, col, "a \"\"\" string is never closed");
                return make(Tok::Bad, line, col, start);
            }
            const char c = src_[pos_];
            if (c == '\\') {
                const char next = peek(1);
                if (next != 'n' && next != 't' && next != '\\' && next != '"') {
                    std::string shown;
                    if (next != '\0' && next != '\n') {
                        // The whole character after the backslash, as Python shows it.
                        std::size_t n = 1;
                        while (pos_ + 1 + n < src_.size() &&
                               (static_cast<unsigned char>(src_[pos_ + 1 + n]) & 0xC0U) == 0x80U) {
                            ++n;
                        }
                        shown = src_.substr(pos_ + 1, n);
                    }
                    error(line_, col_, "unknown escape '\\" + shown + "' in a \"\"\" string");
                    bad = true;
                }
                advance(next != '\0' ? 2 : 1);
                continue;
            }
            if (starts_with("\"\"\"")) {
                const std::string_view raw = std::string_view(src_).substr(body, pos_ - body);
                advance(3);
                if (bad) {
                    return make(Tok::Bad, line, col, start);
                }
                Token t = make(Tok::String, line, col, start);
                t.string_text = unescape(trim_indent(raw));
                t.formatted = true;
                return t;
            }
            advance();
        }
    }

    std::string_view path_;
    std::string src_;
    std::vector<Error>& errors_;
    std::size_t pos_ = 0;
    std::uint32_t line_ = 1;
    std::uint32_t col_ = 1;
};

// ---------------------------------------------------------------- parser
bool is_name(std::string_view text) noexcept {
    if (text.empty() || !(text[0] == '_' || (text[0] >= 'a' && text[0] <= 'z'))) {
        return false;
    }
    return std::all_of(text.begin(), text.end(),
                       [](char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); });
}

bool is_continuation(std::string_view p) noexcept {
    constexpr std::array<std::string_view, 19> kContinuation{
        ".", "..", "{", "(", "[", "*", "~", "<", ">", "=", "!", "|", "&", "+", "-", "/", "%", ":", "?"};
    return std::find(kContinuation.begin(), kContinuation.end(), p) != kContinuation.end();
}

std::string_view closer_of(std::string_view opener) noexcept {
    return opener == "(" ? ")" : opener == "[" ? "]" : "}";
}

enum class Context : std::uint8_t { Top, Block, List };

/// One end of a quantity before conversion.
struct NumberEnd {
    std::int64_t sign = 1;
    std::int64_t mantissa = 0;
    bool huge = false; ///< more than int64 holds
    int decimals = 0;
    const Unit* unit = nullptr;
    std::size_t token = 0; ///< the '-' or number token, for errors
};

class Parser {
public:
    Parser(std::string_view path, std::vector<Token> tokens, const std::string& src,
           std::vector<Error>& errors)
        : path_(path), toks_(std::move(tokens)), src_(src), errors_(errors) {}

    std::vector<PeoRecord> parse() {
        std::vector<PeoRecord> records;
        std::vector<std::pair<std::string, std::uint32_t>> seen;
        while (!at(Tok::Eof)) {
            const std::size_t first = i_;
            failure_.reset();
            std::optional<PeoRecord> rec = record();
            if (rec) {
                const auto dup = std::find_if(seen.begin(), seen.end(),
                                              [&](const auto& s) { return s.first == rec->path; });
                if (dup != seen.end()) {
                    fail(first, "record '" + rec->path + "' is already defined at line " +
                                    std::to_string(dup->second));
                    rec.reset();
                } else {
                    seen.emplace_back(rec->path, rec->line);
                    records.push_back(std::move(*rec));
                    continue;
                }
            }
            if (failure_ && !failure_->silent) {
                errors_.push_back({failure_->line, failure_->col,
                                   std::string(path_) + ":" + std::to_string(failure_->line) + ":" +
                                       std::to_string(failure_->col) + ": " + failure_->message});
            }
            resync(first, failure_ ? failure_->line : 0, failure_ ? failure_->col : 0);
        }
        return records;
    }

private:
    struct Failure {
        std::uint32_t line;
        std::uint32_t col;
        std::string message;
        bool silent;
    };

    // -- helpers
    [[nodiscard]] const Token& tok(std::size_t k = 0) const noexcept {
        return toks_[std::min(i_ + k, toks_.size() - 1)];
    }
    [[nodiscard]] bool at(Tok kind, std::string_view text = {}, std::size_t k = 0) const noexcept {
        const Token& t = tok(k);
        return t.kind == kind && (text.empty() || t.text == text);
    }
    [[nodiscard]] bool punct(std::string_view text, std::size_t k = 0) const noexcept {
        return at(Tok::Punct, text, k);
    }

    /// Record the first failure at token `index`; a Bad token's error is already reported.
    void fail(std::size_t index, std::string message) {
        const Token& t = toks_[index];
        fail_at(t.line, t.col, t.kind == Tok::Bad ? std::string() : std::move(message), t.kind == Tok::Bad);
    }
    void fail_at(std::uint32_t line, std::uint32_t col, std::string message, bool silent = false) {
        if (!failure_) {
            failure_ = Failure{line, col, std::move(message), silent};
        }
    }

    [[nodiscard]] std::string describe(const Token& t) const {
        if (t.kind == Tok::Eof) {
            return "the end of the file";
        }
        if (t.kind == Tok::String) {
            return "a string";
        }
        return "'" + t.text + "'";
    }

    /// Skip to the next record: the first word at column 1, at or after the error and past
    /// the failed record's first token.
    void resync(std::size_t first, std::uint32_t line, std::uint32_t col) {
        std::size_t i = first + 1;
        while (true) {
            const Token& t = toks_[i];
            if (t.kind == Tok::Eof ||
                (t.kind == Tok::Word && t.col == 1 && (t.line > line || (t.line == line && t.col >= col)))) {
                break;
            }
            ++i;
        }
        i_ = i;
    }

    // -- records
    std::optional<PeoRecord> record() {
        bool abstract = false;
        if (at(Tok::Word, "abstract") && at(Tok::Word, {}, 1) && tok(1).line == tok().line) {
            abstract = true;
            ++i_;
        }
        const std::uint32_t line = tok().line;
        std::optional<std::string> path = path_text("a record path");
        if (!path) {
            return std::nullopt;
        }
        if (punct("{")) {
            std::optional<PeoValue> value = block();
            if (!value) {
                return std::nullopt;
            }
            return PeoRecord{std::move(*path), abstract, std::move(*value), false, line};
        }
        if (punct("=")) {
            ++i_;
            std::optional<PeoValue> value = this->value(Context::Top, std::nullopt);
            if (!value) {
                return std::nullopt;
            }
            return PeoRecord{std::move(*path), abstract, std::move(*value), true, line};
        }
        fail(i_, "expected '{' or '=' after '" + *path + "', found " + describe(tok()));
        return std::nullopt;
    }

    std::optional<std::string> path_text(std::string_view what) {
        if (tok().kind != Tok::Word) {
            fail(i_, "expected " + std::string(what) + ", found " + describe(tok()));
            return std::nullopt;
        }
        if (!name(i_)) {
            return std::nullopt;
        }
        std::string out = tok().text;
        ++i_;
        while (punct(".") && at(Tok::Word, {}, 1)) {
            ++i_;
            if (!name(i_)) {
                return std::nullopt;
            }
            out += '.';
            out += tok().text;
            ++i_;
        }
        return out;
    }

    bool name(std::size_t index) {
        if (!is_name(toks_[index].text)) {
            fail(index, "a name is lowercase letters, digits and _: '" + toks_[index].text + "'");
            return false;
        }
        return true;
    }

    // -- blocks and lists
    std::optional<PeoValue> block() {
        const std::size_t open = i_;
        ++i_;
        PeoValue out;
        out.kind = PeoKind::Block;
        if (punct("}")) {
            ++i_;
            return out;
        }
        while (true) {
            if (!field_into(out, open)) {
                return std::nullopt;
            }
            if (punct(",")) {
                const std::size_t comma = i_;
                ++i_;
                if (punct("}")) {
                    fail(comma, "no comma before '}'");
                    return std::nullopt;
                }
                continue;
            }
            if (punct("}")) {
                ++i_;
                return out;
            }
            fail_separator(i_, open, "}");
            return std::nullopt;
        }
    }

    void fail_separator(std::size_t index, std::size_t open, std::string_view closer) {
        const Token& t = toks_[index];
        const Token& o = toks_[open];
        if (t.kind == Tok::Eof) {
            fail_at(o.line, o.col, "'" + o.text + "' is never closed");
            return;
        }
        std::string hint;
        if (t.col == 1 && t.kind == Tok::Word) {
            hint = " (is the '" + o.text + "' at " + std::to_string(o.line) + ":" + std::to_string(o.col) +
                   " closed?)";
        }
        fail(index, "expected ',' or '" + std::string(closer) + "', found " + describe(t) + hint);
    }

    bool field_into(PeoValue& out, std::size_t open) {
        const Token& t = tok();
        if (t.kind == Tok::Eof) {
            const Token& o = toks_[open];
            fail_at(o.line, o.col, "'" + o.text + "' is never closed");
            return false;
        }
        if (t.kind != Tok::Word) {
            fail(i_, "expected a field name, found " + describe(t));
            return false;
        }
        if (!name(i_)) {
            return false;
        }
        std::vector<std::string> keys{t.text};
        std::vector<std::size_t> key_toks{i_};
        ++i_;
        while (punct(".") && at(Tok::Word, {}, 1)) {
            ++i_;
            key_toks.push_back(i_);
            if (!name(i_)) {
                return false;
            }
            keys.push_back(tok().text);
            ++i_;
        }
        std::optional<PeoValue> value;
        if (punct("=")) {
            ++i_;
            value = this->value(Context::Block, open);
        } else if (punct("{")) {
            value = block();
        } else {
            fail(i_,
                 "expected '=' or '{' after '" + joined(keys, keys.size()) + "', found " + describe(tok()));
            return false;
        }
        if (!value) {
            return false;
        }
        return set_path(out, keys, key_toks, std::move(*value));
    }

    static std::string joined(const std::vector<std::string>& keys, std::size_t n) {
        std::string out;
        for (std::size_t k = 0; k < n; ++k) {
            if (k > 0) {
                out += '.';
            }
            out += keys[k];
        }
        return out;
    }

    static PeoValue* find_field(PeoValue& block, std::string_view key) noexcept {
        for (PeoField& f : block.fields) {
            if (f.key == key) {
                return &f.value;
            }
        }
        return nullptr;
    }

    bool set_path(PeoValue& out, const std::vector<std::string>& keys,
                  const std::vector<std::size_t>& key_toks, PeoValue value) {
        PeoValue* cur = &out;
        for (std::size_t depth = 0; depth + 1 < keys.size(); ++depth) {
            PeoValue* next = find_field(*cur, keys[depth]);
            if (next == nullptr) {
                PeoValue nested;
                nested.kind = PeoKind::Block;
                cur->fields.push_back({keys[depth], std::move(nested)});
                next = &cur->fields.back().value;
            } else if (next->kind != PeoKind::Block) {
                fail(key_toks[depth], "'" + joined(keys, depth + 1) + "' is not a block");
                return false;
            }
            cur = next;
        }
        PeoValue* existing = find_field(*cur, keys.back());
        if (existing == nullptr) {
            cur->fields.push_back({keys.back(), std::move(value)});
            return true;
        }
        if (existing->kind == PeoKind::Block && value.kind == PeoKind::Block) {
            return merge(*existing, std::move(value), joined(keys, keys.size()), key_toks.back());
        }
        fail(key_toks.back(), "'" + joined(keys, keys.size()) + "' is set twice in this block");
        return false;
    }

    bool merge(PeoValue& into, PeoValue over, const std::string& prefix, std::size_t key_tok) {
        for (PeoField& f : over.fields) {
            PeoValue* existing = find_field(into, f.key);
            const std::string path = prefix + "." + f.key;
            if (existing == nullptr) {
                into.fields.push_back(std::move(f));
            } else if (existing->kind == PeoKind::Block && f.value.kind == PeoKind::Block) {
                if (!merge(*existing, std::move(f.value), path, key_tok)) {
                    return false;
                }
            } else {
                fail(key_tok, "'" + path + "' is set twice in this block");
                return false;
            }
        }
        return true;
    }

    std::optional<PeoValue> list_value() {
        const std::size_t open = i_;
        ++i_;
        PeoValue out;
        out.kind = PeoKind::List;
        if (punct("]")) {
            ++i_;
            return out;
        }
        while (true) {
            if (at(Tok::Eof)) {
                const Token& o = toks_[open];
                fail_at(o.line, o.col, "'[' is never closed");
                return std::nullopt;
            }
            std::optional<PeoValue> item = value(Context::List, open);
            if (!item) {
                return std::nullopt;
            }
            out.items.push_back(std::move(*item));
            if (punct(",")) {
                const std::size_t comma = i_;
                ++i_;
                if (punct("]")) {
                    fail(comma, "no comma before ']'");
                    return std::nullopt;
                }
                continue;
            }
            if (punct("]")) {
                ++i_;
                return out;
            }
            fail_separator(i_, open, "]");
            return std::nullopt;
        }
    }

    // -- values
    [[nodiscard]] bool ends_value(Context context, const Token& last) const noexcept {
        const Token& t = tok();
        if (context == Context::Block) {
            return t.kind == Tok::Eof || (t.kind == Tok::Punct && (t.text == "," || t.text == "}"));
        }
        if (context == Context::List) {
            return t.kind == Tok::Eof || (t.kind == Tok::Punct && (t.text == "," || t.text == "]"));
        }
        return t.kind == Tok::Eof || t.line > last.line;
    }

    std::optional<PeoValue> value(Context context, std::optional<std::size_t> opener) {
        const std::size_t start = i_;
        const Token& t = tok();
        if (t.kind == Tok::Bad) {
            fail(i_, {});
            return std::nullopt;
        }
        if (t.kind == Tok::Eof ||
            (t.kind == Tok::Punct && (t.text == "," || t.text == "}" || t.text == "]"))) {
            fail(i_, "expected a value, found " + describe(t));
            return std::nullopt;
        }
        // A one-line record's value starts on the line of its '=', so it never takes the next
        // record for its value.
        if (context == Context::Top && t.line > toks_[start - 1].line) {
            fail(i_, "expected a value on the line of '=', found " + describe(t));
            return std::nullopt;
        }
        std::optional<PeoValue> structured;
        if (t.kind == Tok::Number || (punct("-") && at(Tok::Number, {}, 1))) {
            structured = quantity_or_range();
        } else if (t.kind == Tok::String) {
            PeoValue v;
            v.kind = PeoKind::String;
            v.text = t.string_text;
            v.formatted = t.formatted;
            ++i_;
            structured = std::move(v);
        } else if (t.kind == Tok::Word) {
            if (t.text == "yes" || t.text == "no") {
                PeoValue v;
                v.kind = PeoKind::Boolean;
                v.boolean = t.text == "yes";
                ++i_;
                structured = std::move(v);
            } else if (is_name(t.text)) {
                std::optional<std::string> path = path_text("a value");
                if (path) {
                    PeoValue v;
                    v.kind = PeoKind::Path;
                    v.text = std::move(*path);
                    structured = std::move(v);
                }
            } else {
                return expr(start, context);
            }
        } else if (punct("[")) {
            structured = list_value();
        } else if (punct("{")) {
            structured = block();
        } else {
            return expr(start, context);
        }
        if (!structured) {
            return std::nullopt;
        }
        const Token& last = toks_[i_ - 1];
        if (ends_value(context, last)) {
            return structured;
        }
        const Token& next = tok();
        if (next.kind == Tok::Punct && is_continuation(next.text)) {
            i_ = start;
            return expr(start, context);
        }
        if (context == Context::Top) {
            fail(i_, "expected a new record after the value, found " + describe(next));
            return std::nullopt;
        }
        fail_separator(i_, *opener, context == Context::Block ? "}" : "]");
        return std::nullopt;
    }

    std::optional<PeoValue> expr(std::size_t start, Context context) {
        std::vector<std::size_t> stack;
        std::size_t i = start;
        while (true) {
            const Token& t = toks_[i];
            if (t.kind == Tok::Bad) {
                i_ = i;
                fail(i, {});
                return std::nullopt;
            }
            if (t.kind == Tok::Eof) {
                if (!stack.empty()) {
                    const Token& o = toks_[stack.back()];
                    fail_at(o.line, o.col, "'" + o.text + "' in an expression is never closed");
                    return std::nullopt;
                }
                break;
            }
            if (stack.empty()) {
                if (context != Context::Top && t.kind == Tok::Punct) {
                    if (t.text == "," || (context == Context::Block && t.text == "}") ||
                        (context == Context::List && t.text == "]")) {
                        break;
                    }
                }
                if (context == Context::Top && i > start && t.line > toks_[i - 1].line) {
                    break;
                }
            }
            if (t.kind == Tok::Punct && (t.text == "(" || t.text == "[" || t.text == "{")) {
                stack.push_back(i);
            } else if (t.kind == Tok::Punct && (t.text == ")" || t.text == "]" || t.text == "}")) {
                if (stack.empty() || closer_of(toks_[stack.back()].text) != t.text) {
                    i_ = i;
                    fail(i, "unbalanced '" + t.text + "' in an expression");
                    return std::nullopt;
                }
                stack.pop_back();
            }
            ++i;
        }
        const Token& first = toks_[start];
        const Token& last = toks_[i - 1];
        i_ = i;
        PeoValue v;
        v.kind = PeoKind::Expr;
        v.text = src_.substr(first.start, last.end - first.start);
        return v;
    }

    std::optional<NumberEnd> number_end() {
        NumberEnd end;
        end.token = i_;
        if (punct("-")) {
            end.sign = -1;
            ++i_;
        }
        const Token& t = tok();
        bool point = false;
        for (const char c : t.text) {
            if (c == '.') {
                point = true;
                continue;
            }
            if (point) {
                ++end.decimals;
            }
            const std::int64_t digit = c - '0';
            if (end.huge || end.mantissa > (INT64_MAX - digit) / 10) {
                end.huge = true;
            } else {
                end.mantissa = end.mantissa * 10 + digit;
            }
        }
        ++i_;
        const Token& u = tok();
        if (u.kind == Tok::Punct && u.text == "%") {
            end.unit = find_unit("%");
            ++i_;
        } else if (u.kind == Tok::Word && u.line == t.line) {
            if (const Unit* unit = find_unit(u.text)) {
                end.unit = unit;
                ++i_;
            } else if (std::find(kImperialLengths.begin(), kImperialLengths.end(), u.text) !=
                       kImperialLengths.end()) {
                fail(i_, "lengths are metric: use mm, cm, m or km, not '" + u.text + "'");
                return std::nullopt;
            } else if (!(punct("=", 1) || punct("{", 1) || punct(".", 1))) {
                fail(i_, "unknown unit '" + u.text + "'");
                return std::nullopt;
            }
        }
        return end;
    }

    std::optional<PeoValue> convert(const NumberEnd& end, const Unit& unit) {
        if (end.huge || end.mantissa >= kMaxUnitMantissa || end.decimals > kMaxUnitDecimals) {
            fail(end.token, "a number with a unit has at most 9 digits and 6 decimals");
            return std::nullopt;
        }
        const std::int64_t scale = pow10(end.decimals);
        PeoValue v;
        v.kind = PeoKind::Quantity;
        v.has_dimension = true;
        v.dimension = unit.dimension;
        if (unit.name == "F") {
            v.number = round_half_away((end.sign * end.mantissa - kFahrenheitOffset * scale) * 5, 9 * scale);
        } else {
            v.number = round_half_away(end.sign * end.mantissa * unit.num, scale * unit.den);
        }
        return v;
    }

    std::optional<PeoValue> integer(const NumberEnd& end) {
        if (end.decimals > 0) {
            fail(end.token, "a number with a decimal point needs a unit");
            return std::nullopt;
        }
        if (end.huge) {
            fail(end.token, "a number is too large");
            return std::nullopt;
        }
        PeoValue v;
        v.kind = PeoKind::Integer;
        v.number = end.sign * end.mantissa;
        return v;
    }

    std::optional<PeoValue> quantity_or_range() {
        const std::optional<NumberEnd> low = number_end();
        if (!low) {
            return std::nullopt;
        }
        std::optional<NumberEnd> high;
        if (punct("..")) {
            ++i_;
            if (!(at(Tok::Number) || (punct("-") && at(Tok::Number, {}, 1)))) {
                fail(i_, "expected a number after '..', found " + describe(tok()));
                return std::nullopt;
            }
            high = number_end();
            if (!high) {
                return std::nullopt;
            }
        }
        if (!high) {
            return low->unit != nullptr ? convert(*low, *low->unit) : integer(*low);
        }
        const Unit* unit_low = low->unit != nullptr ? low->unit : high->unit;
        const Unit* unit_high = high->unit != nullptr ? high->unit : low->unit;
        PeoValue range;
        range.kind = PeoKind::Range;
        if (unit_low == nullptr) {
            const std::optional<PeoValue> a = integer(*low);
            if (!a) {
                return std::nullopt;
            }
            const std::optional<PeoValue> b = integer(*high);
            if (!b) {
                return std::nullopt;
            }
            range.number = a->number;
            range.high = b->number;
            return range;
        }
        const std::optional<PeoValue> a = convert(*low, *unit_low);
        if (!a) {
            return std::nullopt;
        }
        const std::optional<PeoValue> b = convert(*high, *unit_high);
        if (!b) {
            return std::nullopt;
        }
        if (a->dimension != b->dimension) {
            fail(high->token, std::string("a range's ends are different dimensions (") +
                                  dimension_name(a->dimension) + " and " + dimension_name(b->dimension) +
                                  ")");
            return std::nullopt;
        }
        range.number = a->number;
        range.high = b->number;
        range.has_dimension = true;
        range.dimension = a->dimension;
        return range;
    }

    std::string_view path_;
    std::vector<Token> toks_;
    const std::string& src_;
    std::vector<Error>& errors_;
    std::size_t i_ = 0;
    std::optional<Failure> failure_;
};

/// CRLF read as LF, as the Python parser does.
std::string normalised(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            continue;
        }
        out += text[i];
    }
    return out;
}

} // namespace

PeoParse parse_peo(const ContentSource& source) {
    std::vector<Error> errors;
    Lexer lexer(source.path, normalised(source.text), errors);
    std::vector<Token> tokens = lexer.tokens();
    Parser parser(source.path, std::move(tokens), lexer.source(), errors);
    PeoParse out;
    out.records = parser.parse();
    std::stable_sort(errors.begin(), errors.end(), [](const Error& a, const Error& b) {
        return a.line != b.line ? a.line < b.line : a.col < b.col;
    });
    for (Error& e : errors) {
        out.errors.push_back(std::move(e.text));
    }
    return out;
}

} // namespace peo::core
