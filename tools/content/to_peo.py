#!/usr/bin/env python3
"""One-time converter of the TOML content tree to .peo (D-057, PEO-136).

    python3 tools/content/to_peo.py --out DIR [--content DIR]

Writes one .peo per TOML file at the same relative path, records in the file's order, fields
in key order. The rules are the tables below and docs/design/content-format.md 'Converting
from TOML': the path table (record type -> path prefix), the field table (unit suffixes, dims,
ranges, compartments) and the value table (floats, booleans, references). It refuses, naming
file, record and field, and writes nothing, rather than guess. Output is the same bytes run to
run. golden.py --peo checks the conversion loses nothing.
"""

from __future__ import annotations

import argparse
import re
import sys
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402

# ---------------------------------------------------------------- the path table
PATH_PREFIX = {
    "material": "item.material", "substance": "substance",
    "property": "def.property", "feature": "def.feature", "capability": "def.capability",
    "stimulus": "def.stimulus", "namespace": "def.namespace", "fastener": "def.fastener",
    "scent": "def.scent", "shape": "def.shape", "schema": "def.schema", "tag": "def.tag",
    "action": "mechanic.action", "skill": "mechanic.skill", "modifier": "mechanic.modifier",
    "detail_table": "mechanic.detail_table",
    "setting": "world.setting", "building": "world.building", "room": "world.room",
    "purpose": "world.purpose", "loot": "world.loot", "extras": "world.extras",
    "outdoor_set": "world.outdoor_set", "detail": "world.detail", "detail_loot": "world.detail_loot",
    "company": "brand.company", "brand": "brand.brand", "store_chain": "brand.store_chain",
    "profile": "unit.human", "remap": "remap.item",
}
ITEM_ROOT = "item"
# Merged tables become one record each.
MERGED_PATH = {
    "meta": "def.meta", "status": "def.fastener_status", "dominance": "def.scent_dominance",
    "default": "mechanic.reaction.default",
}
# Keys a merged table renames because they are not grammar names.
MERGED_KEY_RENAMES = {"dominance": {"D": "d"}}

# ---------------------------------------------------------------- the field table
# A key ending in one of these loses it when its value is a number (or numbers), and the value
# carries the unit. Longest first, so fuel_value_kj_g is never read as _g.
UNIT_SUFFIXES = [
    ("_mg_cm3", "kg/m3"), ("_mg_ml", "kg/m3"), ("_days", "d"), ("_pct", "%"), ("_mm2", "mm2"),
    ("_min", "min"), ("_mm", "mm"), ("_ml", "ml"), ("_m2", "m2"), ("_m", "m"), ("_g", "g"),
    ("_s", "s"), ("_c", "C"),
]
# Suffixes the parser has no unit for: these keys stay plain integers under their names.
PLAIN_SUFFIXES = ("_kj_g", "_clo10", "_wh", "_w")
# Keys whose ending looks like a unit but is part of the name (a minimum, not minutes).
NOT_UNITS = {"rigidity_min", "param_or_dim_min"}
# Two-number lists derive and lint read as low and high (derive._range_mean, schemas 'range').
RANGE_KEYS = {"count", "rolls", "floors", "density", "fill_pct", "lot_m", "size_m2"}
DIMS_KEY = "dims_mm"
DIMS_AXES = ("l", "w", "h")

# ---------------------------------------------------------------- the value table: references
# By record type, the keys whose strings name records, and of which types (the first type that
# has the ID wins). A key applies wherever it sits in the record.
REFS: dict[str, dict[str, tuple[str, ...]]] = {
    "item": {"material": ("material",), "shape": ("shape",), "tags": ("tag",), "modifiers": ("modifier",),
             "details": ("detail_table",), "rests_on": ("item",), "purpose": ("purpose",),
             "substance": ("substance",), "items": ("item", "loot"), "item": ("item",), "joints": ("fastener",)},
    "room": {"item": ("item",), "contains": ("loot", "item"), "loose": ("loot", "item"), "purpose": ("purpose",),
             "tags": ("tag",)},
    "building": {"room": ("room",), "profile": ("profile",), "outdoor": ("outdoor_set",),
                 "store_chains": ("store_chain",), "exterior": ("material",), "roof": ("material",),
                 "walls": ("material",), "context": ("tag",), "tags": ("tag",)},
    "setting": {"building": ("building",), "set": ("outdoor_set",), "tags": ("tag",)},
    "loot": {"item": ("item", "loot"), "mods": ("modifier",), "substance": ("substance",)},
    "extras": {"item": ("item", "loot"), "mods": ("modifier",), "ctx": ("tag",), "kind": ("tag",)},
    "detail": {"loot": ("loot",)},
    "detail_loot": {"loot": ("loot",), "detail": ("tag",), "kind": ("tag",)},
    "purpose": {"loot": ("loot",)},
    "outdoor_set": {"item": ("item", "loot")},
    "profile": {k: ("item", "loot") for k in (
        "top", "bottom", "footwear", "socks", "underwear", "outer", "headwear", "eyes", "hands", "full_body",
        "accessory", "carried", "pockets")} | {"tags": ("tag",)},
    "brand": {"company": ("company",), "own_label_of": ("store_chain",)},
    "store_chain": {"own_label": ("brand",), "type": ("building",)},
    "modifier": {"tags_any": ("tag",), "tags_all": ("tag",), "items": ("item",), "shapes": ("shape",),
                 "add_tags": ("tag",), "remove_tags": ("tag",), "skill": ("skill",), "substance": ("substance",)},
    "detail_table": {"tags_any": ("tag",), "tags_all": ("tag",), "items": ("item",), "shapes": ("shape",),
                     "add_tags": ("tag",), "remove_tags": ("tag",), "modifier": ("modifier",)},
    "substance": {"attacks": ("material",), "channel": ("scent",)},
    "action": {"skill": ("skill",), "feature": ("feature",), "capability": ("capability",), "tag": ("tag",),
               "tag_prefix": ("tag",), "shape_in": ("shape",), "prop": ("property",)},
    "fastener": {"weaknesses": ("stimulus",)},
}
# Words a profile slot uses for "nothing" or "anything": kept as strings.
SENTINELS = {"none", "any"}
# Floats are factors; each must be a whole percent.
PERCENT = 100
NAME = re.compile(r"^[a-z_][a-z0-9_]*$")
LINE_LIMIT = 100
INDENT = "    "


class ConvertError(Exception):
    pass


def segment(text: str) -> str:
    """A path segment for an ID part: one that starts with a digit gets a leading '_'."""
    return f"_{text}" if text[:1].isdigit() else text


def snake(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


class Converter:
    def __init__(self, db: common.Db) -> None:
        self.db = db
        self.where = ""

    def fail(self, message: str):
        raise ConvertError(f"{self.where}: {message}")

    # -- paths
    def item_path(self, iid: str) -> str:
        chain, seen = [], set()
        cur = iid
        while cur:
            if cur in seen:
                self.fail(f"inheritance cycle at item {cur}")
            seen.add(cur)
            chain.append(cur)
            cur = (self.db.get("item", cur) or {}).get("parent")
        return ".".join([ITEM_ROOT] + [self.checked(x) for x in reversed(chain)])

    def checked(self, part: str) -> str:
        out = segment(part)
        if not NAME.match(out):
            self.fail(f"ID part '{part}' is not a valid path segment")
        return out

    def record_path(self, rtype: str, rid: str) -> str:
        if rtype == "item":
            return self.item_path(rid)
        return ".".join([PATH_PREFIX[rtype]] + [self.checked(p) for p in rid.split(".")])

    def ref(self, targets: tuple[str, ...], value: str) -> str:
        for target in targets:
            if target == "tag":
                if value.split(".", 1)[0] in self.db["namespace"]:
                    return self.record_path("tag", value)
                continue
            if self.db.get(target, value) is not None:
                return self.record_path(target, value)
        self.fail(f"reference '{value}' names no {' or '.join(targets)}")

    # -- values
    def scalar(self, rtype: str, key: str, value):
        """A value as .peo text (not a dict)."""
        if isinstance(value, bool):
            return "yes" if value else "no"
        if isinstance(value, int):
            return str(value)
        if isinstance(value, float):
            return self.percent(value)
        if isinstance(value, str):
            targets = REFS.get(rtype, {}).get(key)
            if targets and not (rtype == "profile" and value in SENTINELS):
                return self.ref(targets, value)
            return quoted(value)
        if isinstance(value, list):
            return [self.value(rtype, key, x) for x in value]
        self.fail(f"{key}: a value of type {type(value).__name__}")

    def percent(self, value: float) -> str:
        scaled = round(value * PERCENT)
        if abs(value * PERCENT - scaled) > 1e-9:
            self.fail(f"float {value} is not a whole percent")
        return f"{scaled}%"

    def value(self, rtype: str, key: str, value):
        if isinstance(value, dict):
            return self.block(rtype, value, parent_key=key)
        return self.scalar(rtype, key, value)

    def with_unit(self, number, unit: str) -> str:
        if isinstance(number, bool) or not isinstance(number, int):
            self.fail(f"a {unit} value {number!r} is not an integer")
        return f"{number}%" if unit == "%" else f"{number} {unit}"

    def field(self, rtype: str, key: str, value, parent_key: str = ""):
        """(new key, converted value) for one TOML key."""
        numeric = (isinstance(value, (int, float)) and not isinstance(value, bool)) or (
            isinstance(value, list) and value and all(isinstance(x, (int, float)) and not isinstance(x, bool)
                                                      for x in value))
        if key == DIMS_KEY:
            if not (isinstance(value, list) and len(value) == 3 and numeric):
                self.fail(f"{key} is not three numbers")
            return "dims", Block([(axis, self.with_unit(v, "mm")) for axis, v in zip(DIMS_AXES, value)])
        if key == "compartments" and rtype == "item" and isinstance(value, list):
            fields = []
            for comp in value:
                name = snake(str(comp.get("name", "")))
                if not NAME.match(name):
                    self.fail(f"compartment name {comp.get('name')!r} gives no valid key")
                fields.append((name, self.block(rtype, comp, parent_key="compartment")))
            return "compartments", self.unique(Block(fields))
        if numeric and key not in NOT_UNITS and not key.endswith(PLAIN_SUFFIXES):
            for suffix, unit in UNIT_SUFFIXES:
                if key.endswith(suffix) and len(key) > len(suffix):
                    base = key[: -len(suffix)]
                    if key in RANGE_KEYS and isinstance(value, list) and len(value) == 2:
                        return base, f"{self.with_unit(value[0], unit)}..{self.with_unit(value[1], unit)}"
                    if isinstance(value, list):
                        return base, [self.with_unit(v, unit) for v in value]
                    return base, self.with_unit(value, unit)
        if key in RANGE_KEYS and isinstance(value, list) and len(value) == 2 and numeric:
            return key, f"{self.scalar(rtype, key, value[0])}..{self.scalar(rtype, key, value[1])}"
        return key, self.value(rtype, key, value)

    def block(self, rtype: str, table: dict, parent_key: str = "") -> "Block":
        fields = []
        for key, value in table.items():
            if key == "ops" and isinstance(value, list):
                fields.append(("ops", [self.op(rtype, op) for op in value]))
                continue
            if parent_key == "purposes" and not NAME.match(key):
                new = snake(key)  # a room's purpose override, keyed by compartment name
                fields.append((new, self.value(rtype, "purposes_override", value)))
                continue
            if not NAME.match(key):
                self.fail(f"key '{key}' is not a name")
            fields.append(self.field(rtype, key, value, parent_key))
        return self.unique(Block(fields))

    def unique(self, block: "Block") -> "Block":
        keys = [k for k, _ in block.fields]
        for k in keys:
            if keys.count(k) > 1:
                self.fail(f"two keys become '{k}'")
        return block

    def op(self, rtype: str, op: dict) -> "Block":
        """A modifier op: its path renamed segment by segment, its value in the renamed field's
        unit for set and add, a factor for mul."""
        path = op.get("path", "")
        kind = op.get("op")
        segments = path.split(".")
        last = segments[-1]
        unit = None
        renamed = []
        for index, seg in enumerate(segments):
            new = seg
            if seg == DIMS_KEY:
                new = "dims"
            elif seg not in NOT_UNITS and not seg.endswith(PLAIN_SUFFIXES):
                for suffix, u in UNIT_SUFFIXES:
                    if seg.endswith(suffix) and len(seg) > len(suffix):
                        new = seg[: -len(suffix)]
                        if index == len(segments) - 1:
                            unit = u
                        break
            renamed.append(new)
        fields = []
        for key, value in op.items():
            if key == "path":
                fields.append(("path", quoted(".".join(renamed))))
            elif key == "value":
                if kind == "mul":
                    if isinstance(value, bool) or not isinstance(value, (int, float)):
                        self.fail(f"a mul value {value!r} is not a number")
                    fields.append(("value", self.percent(float(value))))  # a factor, as a percent
                elif last == DIMS_KEY and isinstance(value, list) and len(value) == 3:
                    fields.append(("value", Block([(a, self.with_unit(v, "mm")) for a, v in zip(DIMS_AXES, value)])))
                elif unit and kind in ("set", "add"):
                    fields.append(("value", self.with_unit(value, unit)))
                else:
                    fields.append(("value", self.value(rtype, last, value)))
            else:
                fields.append(self.field(rtype, key, value))
        return Block(fields)


class Block:
    def __init__(self, fields: list) -> None:
        self.fields = fields


def quoted(text: str) -> str:
    if '"' in text or "\n" in text:
        if text[:1] in (" ", "\t"):
            raise ConvertError(f"a string starting with whitespace needs a quote or line break kept: {text!r}")
        body = text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t")
        return f'"""{body}"""'
    return f'"{text}"'


# ---------------------------------------------------------------- writing


def render(value, indent: str) -> str:
    """A value as text, at `indent`; blocks and long lists span lines."""
    if isinstance(value, Block):
        return render_block(value, indent)
    if isinstance(value, list):
        items = [render(v, indent + INDENT) for v in value]
        one = "[" + ", ".join(items) + "]"
        if "\n" not in one and len(indent) + len(one) <= LINE_LIMIT:
            return one
        inner = ",\n".join(indent + INDENT + item for item in items)
        return "[\n" + inner + "\n" + indent + "]"
    return value


def render_block(block: Block, indent: str) -> str:
    if not block.fields:
        return "{}"
    inline = [f"{k} = {render(v, indent)}" if not isinstance(v, Block) else None for k, v in block.fields]
    if None not in inline:
        one = "{ " + ", ".join(inline) + " }"
        if "\n" not in one and len(indent) + len(one) <= LINE_LIMIT:
            return one
    lines = []
    for key, value in block.fields:
        if isinstance(value, Block):
            lines.append(f"{indent}{INDENT}{key} {render_block(value, indent + INDENT)}")
        else:
            lines.append(f"{indent}{INDENT}{key} = {render(value, indent + INDENT)}")
    return "{\n" + ",\n".join(lines) + "\n" + indent + "}"


def convert_file(conv: Converter, path: Path, content_dir: Path) -> str:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    rel = path.relative_to(content_dir)
    out = []
    for rtype, recs in data.items():
        if rtype in MERGED_PATH:
            conv.where = f"{rel}: [{rtype}]"
            renames = MERGED_KEY_RENAMES.get(rtype, {})
            table = {renames.get(k, k): v for k, v in recs.items()}
            out.append(f"{MERGED_PATH[rtype]} {render_block(conv.block(rtype, table), '')}")
            continue
        for rid, rec in recs.items():
            conv.where = f"{rel}: {rtype}.{rid}"
            record = dict(rec)
            abstract = False
            if rtype == "item":
                abstract = bool(record.pop("abstract", False))
                record.pop("parent", None)
            header = ("abstract " if abstract else "") + conv.record_path(rtype, rid)
            out.append(f"{header} {render_block(conv.block(rtype, record), '')}")
    return "\n".join(out) + ("\n" if out else "")


def convert(content_dir: Path) -> dict[Path, str]:
    """Every .peo file's text by relative path; raises ConvertError on the first refusal."""
    db = common.load(content_dir)
    if db.errors:
        raise ConvertError("; ".join(db.errors))
    conv = Converter(db)
    files = {}
    for path in sorted(content_dir.rglob("*.toml")):
        files[path.relative_to(content_dir).with_suffix(".peo")] = convert_file(conv, path, content_dir)
    return files


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--content", type=Path, default=common.CONTENT)
    args = ap.parse_args()
    try:
        files = convert(args.content)
    except ConvertError as e:
        print(f"to_peo: {e}", file=sys.stderr)
        return 1
    for rel, text in files.items():
        target = args.out / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
    print(f"to_peo: wrote {len(files)} files to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
