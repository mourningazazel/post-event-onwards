"""The .peo content format parser (D-057, PEO-135), to docs/design/content-format.md 'Grammar'.

The engine has the same parser in C++ (peo/core/peo_format.hpp); tests/content/peo/ holds the
conformance cases both must pass, compared as canonical lines (print_lines). Keep the two in
step: same tree, same rules, same error text.

    python3 tools/content/peo.py FILE.peo...    # print canonical lines and errors; exit 1 on any
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field

# ---------------------------------------------------------------- values


@dataclass
class Integer:
    value: int


@dataclass
class Quantity:
    value: int          # in the base unit of its dimension
    dimension: str


@dataclass
class Range:
    low: Integer | Quantity
    high: Integer | Quantity


@dataclass
class Boolean:
    value: bool


@dataclass
class String:
    text: str
    formatted: bool     # written as """...""": escapes read, indentation trimmed


@dataclass
class Path:
    text: str


@dataclass
class Expr:
    text: str           # a query, call or $role, kept as written for PEO-132


@dataclass
class List:
    items: list


@dataclass
class Block:
    fields: list = field(default_factory=list)   # (key, value) in file order

    def get(self, key: str):
        for k, v in self.fields:
            if k == key:
                return v
        return None


@dataclass
class Record:
    path: str
    abstract: bool
    value: object       # a Block, or any value for the one-line `path = value` form
    line: int
    one_line: bool = False


@dataclass
class Parse:
    records: list
    errors: list


# ---------------------------------------------------------------- units
# Base unit per dimension; factors are exact reduced fractions (numerator, denominator).
UNITS = {
    "mm": ("length", 1, 1), "cm": ("length", 10, 1), "m": ("length", 1000, 1), "km": ("length", 1000000, 1),
    "mm2": ("area", 1, 1), "cm2": ("area", 100, 1), "m2": ("area", 1000000, 1),
    "g": ("mass", 1, 1), "kg": ("mass", 1000, 1),
    "oz": ("mass", 226796185, 8000000), "lb": ("mass", 45359237, 100000),
    "ml": ("volume", 1, 1), "l": ("volume", 1000, 1),
    "fl_oz": ("volume", 473176473, 16000000), "gal": ("volume", 473176473, 125000),
    "s": ("time", 1, 1), "min": ("time", 60, 1), "h": ("time", 3600, 1), "d": ("time", 86400, 1),
    "C": ("temperature", 1, 1), "F": ("temperature", 5, 9),   # F: (x - 32) * 5 / 9
    "kg/m3": ("density", 1, 1),
    "%": ("percent", 1, 1),
}
IMPERIAL_LENGTHS = ("ft", "in", "yd", "mi")
MAX_UNIT_MANTISSA = 10 ** 9      # digits a number with a unit may have, so conversion stays in int64
MAX_UNIT_DECIMALS = 6
INT64_MAX = 2 ** 63 - 1


def round_half_away(num: int, den: int) -> int:
    """num / den rounded half away from zero (derive.rnd), den > 0."""
    if num >= 0:
        return (2 * num + den) // (2 * den)
    return -((-2 * num + den) // (2 * den))


# ---------------------------------------------------------------- lexer
# Token kinds: word, number, string, punct, eof, bad (a lexer error already reported).
CONTINUATION = {".", "..", "{", "(", "[", "*", "~", "<", ">", "=", "!", "|", "&", "+", "-", "/", "%", ":", "?"}
OPENERS = {"(": ")", "[": "]", "{": "}"}
CLOSERS = {")", "]", "}"}


@dataclass
class Token:
    kind: str
    text: str
    line: int
    col: int
    start: int
    end: int
    value: object = None    # String for a string token


class PeoError(Exception):
    def __init__(self, tok_or_pos, message: str, silent: bool = False):
        super().__init__(message)
        self.line, self.col = tok_or_pos
        self.message = message
        self.silent = silent


def is_word_start(c: str) -> bool:
    return c == "_" or ("a" <= c <= "z") or ("A" <= c <= "Z")


def is_word_char(c: str) -> bool:
    return is_word_start(c) or ("0" <= c <= "9")


def is_digit(c: str) -> bool:
    return "0" <= c <= "9"


def is_space(c: str) -> bool:
    return c in " \t\r\n"


class Lexer:
    def __init__(self, path: str, text: str, errors: list) -> None:
        self.path = path
        self.src = text.replace("\r\n", "\n")
        self.errors = errors
        self.pos = 0
        self.line = 1
        self.col = 1

    def error(self, line: int, col: int, message: str) -> None:
        self.errors.append((line, col, f"{self.path}:{line}:{col}: {message}"))

    def advance(self, n: int = 1) -> None:
        for _ in range(n):
            if self.src[self.pos] == "\n":
                self.line += 1
                self.col = 1
            else:
                self.col += 1
            self.pos += 1

    def peek(self, k: int = 0) -> str:
        i = self.pos + k
        return self.src[i] if i < len(self.src) else ""

    def tokens(self) -> list[Token]:
        out = []
        while True:
            self.skip_space_and_comments()
            if self.pos >= len(self.src):
                out.append(Token("eof", "", self.line, self.col, self.pos, self.pos))
                return out
            out.append(self.next_token())

    def skip_space_and_comments(self) -> None:
        while self.pos < len(self.src):
            c = self.src[self.pos]
            if is_space(c):
                self.advance()
            elif c == "#":
                while self.pos < len(self.src) and self.src[self.pos] != "\n":
                    self.advance()
            else:
                return

    def next_token(self) -> Token:
        line, col, start = self.line, self.col, self.pos
        c = self.peek()
        if is_word_start(c):
            while is_word_char(self.peek()):
                self.advance()
            # A unit like kg/m3: a slash and more word characters right after the word.
            if self.peek() == "/" and is_word_char(self.peek(1)):
                self.advance()
                while is_word_char(self.peek()):
                    self.advance()
            return Token("word", self.src[start:self.pos], line, col, start, self.pos)
        if is_digit(c):
            while is_digit(self.peek()):
                self.advance()
            if self.peek() == "." and is_digit(self.peek(1)):
                self.advance()
                while is_digit(self.peek()):
                    self.advance()
            return Token("number", self.src[start:self.pos], line, col, start, self.pos)
        if c == '"':
            if self.src.startswith('"""', self.pos):
                return self.triple_string(line, col, start)
            return self.verbatim_string(line, col, start)
        if c == "." and self.peek(1) == ".":
            self.advance(2)
            return Token("punct", "..", line, col, start, self.pos)
        self.advance()
        return Token("punct", c, line, col, start, self.pos)

    def verbatim_string(self, line: int, col: int, start: int) -> Token:
        self.advance()
        body = self.pos
        while self.pos < len(self.src) and self.src[self.pos] not in '"\n':
            self.advance()
        if self.pos >= len(self.src) or self.src[self.pos] == "\n":
            self.error(line, col, 'a "..." string ends at the end of its line; use """...""" for more')
            return Token("bad", "", line, col, start, self.pos)
        text = self.src[body:self.pos]
        self.advance()
        return Token("string", self.src[start:self.pos], line, col, start, self.pos, String(text, False))

    def triple_string(self, line: int, col: int, start: int) -> Token:
        self.advance(3)
        body = self.pos
        bad = False
        while True:
            if self.pos >= len(self.src):
                self.error(line, col, 'a """ string is never closed')
                return Token("bad", "", line, col, start, self.pos)
            c = self.src[self.pos]
            if c == "\\":
                nxt = self.peek(1)
                if nxt not in ("n", "t", "\\", '"'):
                    shown = nxt if nxt and nxt != "\n" else ""
                    self.error(self.line, self.col, f"unknown escape '\\{shown}' in a \"\"\" string")
                    bad = True
                self.advance(2 if nxt else 1)
                continue
            if self.src.startswith('"""', self.pos):
                raw = self.src[body:self.pos]
                self.advance(3)
                if bad:
                    return Token("bad", "", line, col, start, self.pos)
                return Token("string", self.src[start:self.pos], line, col, start, self.pos,
                             String(unescape(trim_indent(raw)), True))
            self.advance()


def trim_indent(raw: str) -> str:
    """Drop a line break right after the opening quotes and a whitespace-only last line, then
    the leading whitespace every non-blank line shares."""
    if raw.startswith("\n"):
        raw = raw[1:]
    lines = raw.split("\n")
    if len(lines) > 1 and lines[-1].strip(" \t") == "":
        lines.pop()
    indents = [ln[:len(ln) - len(ln.lstrip(" \t"))] for ln in lines if ln.strip(" \t")]
    common = indents[0] if indents else ""
    for ind in indents[1:]:
        while not ind.startswith(common):
            common = common[:-1]
    return "\n".join(ln[len(common):] if ln.strip(" \t") else "" for ln in lines)


def unescape(text: str) -> str:
    out, i = [], 0
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text):
            out.append({"n": "\n", "t": "\t", "\\": "\\", '"': '"'}[text[i + 1]])
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


# ---------------------------------------------------------------- parser


def is_name(text: str) -> bool:
    if not text or not (text[0] == "_" or "a" <= text[0] <= "z"):
        return False
    return all(c == "_" or "a" <= c <= "z" or "0" <= c <= "9" for c in text)


class Parser:
    def __init__(self, path: str, tokens: list[Token], src: str, errors: list) -> None:
        self.path = path
        self.toks = tokens
        self.src = src
        self.errors = errors
        self.i = 0

    # -- helpers
    def tok(self, k: int = 0) -> Token:
        return self.toks[min(self.i + k, len(self.toks) - 1)]

    def at(self, kind: str, text: str | None = None, k: int = 0) -> bool:
        t = self.tok(k)
        return t.kind == kind and (text is None or t.text == text)

    def fail(self, t: Token, message: str):
        if t.kind == "bad":
            raise PeoError((t.line, t.col), "", silent=True)
        raise PeoError((t.line, t.col), message)

    def describe(self, t: Token) -> str:
        if t.kind == "eof":
            return "the end of the file"
        if t.kind == "string":
            return "a string"
        return f"'{t.text}'"

    # -- file
    def parse(self) -> list[Record]:
        records: list[Record] = []
        seen: dict[str, int] = {}
        while not self.at("eof"):
            first = self.i
            try:
                rec = self.record()
                if rec.path in seen:
                    t = self.toks[first]
                    raise PeoError((t.line, t.col), f"record '{rec.path}' is already defined at line {seen[rec.path]}")
                seen[rec.path] = rec.line
                records.append(rec)
            except PeoError as e:
                if not e.silent:
                    self.errors.append((e.line, e.col, f"{self.path}:{e.line}:{e.col}: {e.message}"))
                self.resync(first, e.line, e.col)
        return records

    def resync(self, first: int, line: int, col: int) -> None:
        """Skip to the next record: the first word at column 1, at or after the error and past
        the failed record's first token (so a record its predecessor left open still parses)."""
        i = first + 1
        while True:
            t = self.toks[i]
            if t.kind == "eof" or (t.kind == "word" and t.col == 1 and (t.line, t.col) >= (line, col)):
                break
            i += 1
        self.i = i

    def record(self) -> Record:
        abstract = False
        if self.at("word", "abstract") and self.at("word", k=1) and self.tok(1).line == self.tok().line:
            abstract = True
            self.i += 1
        start = self.tok()
        path = self.path_text("a record path")
        if self.at("punct", "{"):
            return Record(path, abstract, self.block(), start.line)
        if self.at("punct", "="):
            self.i += 1
            return Record(path, abstract, self.value("top"), start.line, one_line=True)
        self.fail(self.tok(), f"expected '{{' or '=' after '{path}', found {self.describe(self.tok())}")

    def path_text(self, what: str) -> str:
        """name { "." name }, as one string."""
        t = self.tok()
        if t.kind != "word":
            self.fail(t, f"expected {what}, found {self.describe(t)}")
        parts = [self.name(t)]
        self.i += 1
        while self.at("punct", ".") and self.at("word", k=1):
            self.i += 1
            parts.append(self.name(self.tok()))
            self.i += 1
        return ".".join(parts)

    def name(self, t: Token) -> str:
        if not is_name(t.text):
            self.fail(t, f"a name is lowercase letters, digits and _: '{t.text}'")
        return t.text

    # -- blocks and lists
    def block(self) -> Block:
        open_tok = self.tok()
        self.i += 1
        out = Block()
        if self.at("punct", "}"):
            self.i += 1
            return out
        while True:
            self.field_into(out, open_tok)
            t = self.tok()
            if t.kind == "punct" and t.text == ",":
                self.i += 1
                if self.at("punct", "}"):
                    self.fail(t, "no comma before '}'")
                continue
            if t.kind == "punct" and t.text == "}":
                self.i += 1
                return out
            self.fail_separator(t, open_tok, "}")

    def fail_separator(self, t: Token, open_tok: Token, closer: str):
        if t.kind == "eof":
            raise PeoError((open_tok.line, open_tok.col), f"'{open_tok.text}' is never closed")
        hint = ""
        if t.col == 1 and t.kind == "word":
            hint = f" (is the '{open_tok.text}' at {open_tok.line}:{open_tok.col} closed?)"
        self.fail(t, f"expected ',' or '{closer}', found {self.describe(t)}{hint}")

    def field_into(self, out: Block, open_tok: Token) -> None:
        t = self.tok()
        if t.kind == "eof":
            raise PeoError((open_tok.line, open_tok.col), f"'{open_tok.text}' is never closed")
        if t.kind != "word":
            self.fail(t, f"expected a field name, found {self.describe(t)}")
        keys = [self.name(t)]
        key_toks = [t]
        self.i += 1
        while self.at("punct", ".") and self.at("word", k=1):
            self.i += 1
            key_toks.append(self.tok())
            keys.append(self.name(self.tok()))
            self.i += 1
        if self.at("punct", "="):
            self.i += 1
            value = self.value("block", open_tok)
        elif self.at("punct", "{"):
            value = self.block()
        else:
            self.fail(self.tok(), f"expected '=' or '{{' after '{'.'.join(keys)}', found {self.describe(self.tok())}")
        self.set_path(out, keys, key_toks, value)

    def set_path(self, out: Block, keys: list[str], key_toks: list[Token], value) -> None:
        cur = out
        for depth, key in enumerate(keys[:-1]):
            nxt = cur.get(key)
            if nxt is None:
                nxt = Block()
                cur.fields.append((key, nxt))
            elif not isinstance(nxt, Block):
                self.fail(key_toks[depth], f"'{'.'.join(keys[:depth + 1])}' is not a block")
            cur = nxt
        last = keys[-1]
        existing = cur.get(last)
        if existing is None:
            cur.fields.append((last, value))
        elif isinstance(existing, Block) and isinstance(value, Block):
            self.merge(existing, value, keys, key_toks)
        else:
            self.fail(key_toks[-1], f"'{'.'.join(keys)}' is set twice in this block")

    def merge(self, into: Block, over: Block, keys: list[str], key_toks: list[Token]) -> None:
        for key, value in over.fields:
            existing = into.get(key)
            if existing is None:
                into.fields.append((key, value))
            elif isinstance(existing, Block) and isinstance(value, Block):
                self.merge(existing, value, keys + [key], key_toks)
            else:
                self.fail(key_toks[-1], f"'{'.'.join(keys + [key])}' is set twice in this block")

    def list_value(self) -> List:
        open_tok = self.tok()
        self.i += 1
        items = []
        if self.at("punct", "]"):
            self.i += 1
            return List(items)
        while True:
            if self.at("eof"):
                raise PeoError((open_tok.line, open_tok.col), "'[' is never closed")
            items.append(self.value("list", open_tok))
            t = self.tok()
            if t.kind == "punct" and t.text == ",":
                self.i += 1
                if self.at("punct", "]"):
                    self.fail(t, "no comma before ']'")
                continue
            if t.kind == "punct" and t.text == "]":
                self.i += 1
                return List(items)
            self.fail_separator(t, open_tok, "]")

    # -- values
    def ends_value(self, context: str, last: Token) -> bool:
        t = self.tok()
        if context == "block":
            return t.kind == "eof" or (t.kind == "punct" and t.text in (",", "}"))
        if context == "list":
            return t.kind == "eof" or (t.kind == "punct" and t.text in (",", "]"))
        return t.kind == "eof" or t.line > last.line

    def value(self, context: str, opener: Token | None = None):
        start = self.i
        t = self.tok()
        if t.kind == "bad":
            self.fail(t, "")
        if t.kind == "eof" or (t.kind == "punct" and t.text in (",", "}", "]")):
            self.fail(t, f"expected a value, found {self.describe(t)}")
        # A one-line record's value starts on the line of its '=', so it never takes the next
        # record for its value.
        if context == "top" and t.line > self.toks[start - 1].line:
            self.fail(t, f"expected a value on the line of '=', found {self.describe(t)}")
        structured = None
        if t.kind == "number" or (t.kind == "punct" and t.text == "-" and self.at("number", k=1)):
            structured = self.quantity_or_range()
        elif t.kind == "string":
            self.i += 1
            structured = t.value
        elif t.kind == "word":
            if t.text in ("yes", "no"):
                self.i += 1
                structured = Boolean(t.text == "yes")
            elif is_name(t.text):
                structured = Path(self.path_text("a value"))
            else:
                return self.expr(start, context)
        elif t.kind == "punct" and t.text == "[":
            structured = self.list_value()
        elif t.kind == "punct" and t.text == "{":
            structured = self.block()
        else:
            return self.expr(start, context)
        last = self.toks[self.i - 1]
        if self.ends_value(context, last):
            return structured
        nxt = self.tok()
        if nxt.kind == "punct" and nxt.text in CONTINUATION:
            self.i = start
            return self.expr(start, context)
        if context == "top":
            self.fail(nxt, f"expected a new record after the value, found {self.describe(nxt)}")
        self.fail_separator(nxt, opener, "}" if context == "block" else "]")

    def expr(self, start: int, context: str) -> Expr:
        stack: list[Token] = []
        i = start
        first = self.toks[start]
        while True:
            t = self.toks[i]
            if t.kind == "bad":
                self.i = i
                self.fail(t, "")
            if t.kind == "eof":
                if stack:
                    o = stack[-1]
                    raise PeoError((o.line, o.col), f"'{o.text}' in an expression is never closed")
                break
            if not stack:
                if context in ("block", "list") and t.kind == "punct" and t.text in (",", "}", "]"):
                    if t.text == ",":
                        break
                    if (context == "block" and t.text == "}") or (context == "list" and t.text == "]"):
                        break
                if context == "top" and i > start and t.line > self.toks[i - 1].line:
                    break
            if t.kind == "punct" and t.text in OPENERS:
                stack.append(t)
            elif t.kind == "punct" and t.text in CLOSERS:
                if not stack or OPENERS[stack[-1].text] != t.text:
                    self.i = i
                    self.fail(t, f"unbalanced '{t.text}' in an expression")
                stack.pop()
            i += 1
        last = self.toks[i - 1]
        self.i = i
        return Expr(self.src[first.start:last.end])

    def number_end(self):
        """One end of a quantity: (sign, mantissa, decimals, unit or None, token)."""
        sign = 1
        first = self.tok()
        if self.at("punct", "-"):
            sign = -1
            self.i += 1
        t = self.tok()
        text = t.text
        digits = text.replace(".", "")
        decimals = len(text) - text.index(".") - 1 if "." in text else 0
        mantissa = int(digits)
        self.i += 1
        unit = None
        u = self.tok()
        if u.kind == "punct" and u.text == "%":
            unit = "%"
            self.i += 1
        elif u.kind == "word" and u.line == t.line:
            if u.text in UNITS:
                unit = u.text
                self.i += 1
            elif u.text in IMPERIAL_LENGTHS:
                self.fail(u, f"lengths are metric: use mm, cm, m or km, not '{u.text}'")
            elif not (self.at("punct", "=", k=1) or self.at("punct", "{", k=1) or self.at("punct", ".", k=1)):
                self.fail(u, f"unknown unit '{u.text}'")
        return sign, mantissa, decimals, unit, first

    def convert(self, end, unit: str):
        sign, mantissa, decimals, _, tok = end
        if mantissa >= MAX_UNIT_MANTISSA or decimals > MAX_UNIT_DECIMALS:
            self.fail(tok, "a number with a unit has at most 9 digits and 6 decimals")
        dimension, num, den = UNITS[unit]
        scale = 10 ** decimals
        if unit == "F":
            return Quantity(round_half_away((sign * mantissa - 32 * scale) * 5, 9 * scale), dimension)
        return Quantity(round_half_away(sign * mantissa * num, scale * den), dimension)

    def integer(self, end) -> Integer:
        sign, mantissa, decimals, _, tok = end
        if decimals:
            self.fail(tok, "a number with a decimal point needs a unit")
        if mantissa > INT64_MAX:
            self.fail(tok, "a number is too large")
        return Integer(sign * mantissa)

    def quantity_or_range(self):
        low = self.number_end()
        high = None
        if self.at("punct", ".."):
            self.i += 1
            if not (self.at("number") or (self.at("punct", "-") and self.at("number", k=1))):
                self.fail(self.tok(), f"expected a number after '..', found {self.describe(self.tok())}")
            high = self.number_end()
        if high is None:
            return self.convert(low, low[3]) if low[3] else self.integer(low)
        unit_low, unit_high = low[3] or high[3], high[3] or low[3]
        if unit_low is None:
            return Range(self.integer(low), self.integer(high))
        a, b = self.convert(low, unit_low), self.convert(high, unit_high)
        if a.dimension != b.dimension:
            self.fail(high[4], f"a range's ends are different dimensions ({a.dimension} and {b.dimension})")
        return Range(a, b)


def parse_peo(path: str, text: str) -> Parse:
    """Parse one .peo source. Errors read '<path>:<line>:<col>: <message>', sorted by position."""
    raw_errors: list = []
    lexer = Lexer(path, text, raw_errors)
    tokens = lexer.tokens()
    parser = Parser(path, tokens, lexer.src, raw_errors)
    records = parser.parse()
    raw_errors.sort(key=lambda e: (e[0], e[1]))
    return Parse(records, [e[2] for e in raw_errors])


# ---------------------------------------------------------------- canonical lines


def escaped(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def scalar_text(v) -> str:
    if isinstance(v, Integer):
        return str(v.value)
    if isinstance(v, Quantity):
        return f"{v.value} {v.dimension}"
    raise TypeError(v)


def value_lines(record: str, path: str, v, out: list[str]) -> None:
    def line(kind: str, text: str) -> None:
        out.append(f"{record}\t{path}\t{kind}\t{text}")

    def child(key: str) -> str:
        return f"{path}.{key}" if path else key

    if isinstance(v, Block):
        if not v.fields:
            line("block", "{}")
        for key, sub in v.fields:
            value_lines(record, child(key), sub, out)
    elif isinstance(v, List):
        if not v.items:
            line("list", "[]")
        for index, sub in enumerate(v.items):
            value_lines(record, child(str(index)), sub, out)
    elif isinstance(v, Integer):
        line("integer", str(v.value))
    elif isinstance(v, Quantity):
        line("quantity", scalar_text(v))
    elif isinstance(v, Range):
        if isinstance(v.low, Quantity):
            line("range", f"{v.low.value}..{v.high.value} {v.low.dimension}")
        else:
            line("range", f"{v.low.value}..{v.high.value}")
    elif isinstance(v, Boolean):
        line("boolean", "yes" if v.value else "no")
    elif isinstance(v, String):
        line("formatted" if v.formatted else "string", escaped(v.text))
    elif isinstance(v, Path):
        line("path", v.text)
    elif isinstance(v, Expr):
        line("expr", escaped(v.text))
    else:
        raise TypeError(v)


def print_lines(parsed: Parse) -> list[str]:
    out: list[str] = []
    for rec in parsed.records:
        out.append(f"{rec.path}\t\trecord\t{'abstract' if rec.abstract else 'concrete'}")
        value_lines(rec.path, "=" if rec.one_line else "", rec.value, out)
    out += [f"error\t{e}" for e in parsed.errors]
    return out


def main(argv: list[str]) -> int:
    failed = False
    for name in argv:
        with open(name, encoding="utf-8") as fh:
            parsed = parse_peo(name, fh.read())
        for line in print_lines(parsed):
            print(line)
        failed = failed or bool(parsed.errors)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
