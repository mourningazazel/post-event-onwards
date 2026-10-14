#!/usr/bin/env python3
"""Golden check of the engine's content resolution against derive.resolve_item (PEO-008).

Prints every resolved item as canonical lines, `<item id>\\t<dotted path>\\t<value>`: items by
ID, keys sorted, arrays by index, backslash, tab and newline escaped, integers in decimal,
booleans true/false. An empty table or array prints nothing. peo_content_dump prints the same
lines from the C++ Content.

    python3 tools/content/golden.py [--content DIR]                 # print the lines
    python3 tools/content/golden.py --engine build/.../peo_content_dump   # diff, exit 1 on any
    python3 tools/content/golden.py --unchanged-from before.txt   # no existing item changed

--unchanged-from (PEO-116) proves a content change added items without changing any that
existed: for every item in FILE, a line added, removed or changed is a difference, except an
added `parent` line and reordered `tags.N` lines with the same set of tags.
"""

from __future__ import annotations

import argparse
import difflib
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402
import derive  # noqa: E402

SHOWN_DIFFERENCES = 20


def escaped(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def value_text(value) -> str:
    if isinstance(value, bool):  # before int: bool is an int in Python
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return escaped(value)
    if isinstance(value, float):
        return repr(value)
    return "?"


def walk(item_id: str, value, path: str, out: list[str]) -> None:
    if isinstance(value, dict):
        for key in sorted(value):
            walk(item_id, value[key], f"{path}.{key}" if path else key, out)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            walk(item_id, child, f"{path}.{index}" if path else str(index), out)
    else:
        out.append(f"{escaped(item_id)}\t{escaped(path)}\t{value_text(value)}")


def golden_lines(content_dir: Path) -> list[str]:
    db = common.load(content_dir)
    if db.errors:
        for error in db.errors:
            print(error, file=sys.stderr)
        sys.exit(1)
    out: list[str] = []
    for item_id in sorted(db["item"]):
        walk(item_id, derive.resolve_item(db, item_id), "", out)
    return out


def by_item(lines: list[str]) -> dict[str, dict[str, str]]:
    """item ID -> dotted path -> value, from canonical lines."""
    out: dict[str, dict[str, str]] = {}
    for line in lines:
        item_id, path, value = line.split("\t", 2)
        out.setdefault(item_id, {})[path] = value
    return out


def is_tag_path(path: str) -> bool:
    return path.startswith("tags.") and path[5:].isdigit()


def unchanged_differences(before: list[str], now: list[str]) -> tuple[list[str], list[str]]:
    """(differences, IDs of items new since `before`)."""
    old, new = by_item(before), by_item(now)
    differences: list[str] = []
    for item_id, old_paths in old.items():
        new_paths = new.get(item_id)
        if new_paths is None:
            differences.append(f"- {item_id}: removed")
            continue
        old_tags = {v for k, v in old_paths.items() if is_tag_path(k)}
        new_tags = {v for k, v in new_paths.items() if is_tag_path(k)}
        same_tags = old_tags == new_tags
        for path in sorted(old_paths.keys() | new_paths.keys()):
            if same_tags and is_tag_path(path):
                continue
            if path == "parent" and path not in old_paths:
                continue
            was, now_value = old_paths.get(path), new_paths.get(path)
            if was != now_value:
                differences.append(f"  {item_id}\t{path}\t{was} -> {now_value}")
    added = sorted(new.keys() - old.keys())
    return differences, added


# ---------------------------------------------------------------- .peo round trip (PEO-136)
# back_to_toml is the inverse of tools/content/to_peo.py, written from the tables in
# docs/design/content-format.md 'Converting from TOML', not by calling to_peo, so a mistake in
# either shows as a difference.
PEO_PREFIXES = {  # path prefix -> record type (an item is any other path under item.)
    "item.material": "material", "substance": "substance", "def.property": "property",
    "def.feature": "feature", "def.capability": "capability", "def.stimulus": "stimulus",
    "def.namespace": "namespace", "def.fastener": "fastener", "def.scent": "scent", "def.shape": "shape",
    "def.schema": "schema", "def.tag": "tag", "mechanic.action": "action", "mechanic.skill": "skill",
    "mechanic.modifier": "modifier", "mechanic.detail_table": "detail_table", "world.setting": "setting",
    "world.building": "building", "world.room": "room", "world.purpose": "purpose", "world.loot": "loot",
    "world.extras": "extras", "world.outdoor_set": "outdoor_set", "world.detail": "detail",
    "world.detail_loot": "detail_loot", "brand.company": "company", "brand.brand": "brand",
    "brand.store_chain": "store_chain", "unit.human": "profile", "remap.item": "remap",
}
# A key that lost a unit suffix, with the suffix and how many base units one of its units is.
SUFFIX_BACK = {
    **{k: ("_pct", 1) for k in ("basement", "chance", "crash_chance", "empty_chance", "fill", "gain_chance",
                                "power_base", "water_base")},
    **{k: ("_mm2", 1) for k in ("area", "contact_area")},
    **{k: ("_min", 60) for k in ("duration", "read_time")},
    **{k: ("_days", 86400) for k in ("clock_min", "clock_max")},
    **{k: ("_mm", 1) for k in ("girth", "height", "inner_diameter", "length", "max_attach_size", "max_dim",
                               "min_girth", "min_piece", "min_thickness", "opening", "width", "wrap_girth")},
    **{k: ("_ml", 1) for k in ("capacity", "hydration")},
    "size": ("_m2", 1_000_000),
    **{k: ("_m", 1000) for k in ("lot", "radius")},
    **{k: ("_g", 1) for k in ("load", "mass", "max_attach_mass", "max_load", "max_mass")},
    **{k: ("_s", 1) for k in ("set_time", "time")},
    **{k: ("_c", 1) for k in ("flash_point", "freezes", "melts")},
}
# Where the same key took a different suffix by record type.
SUFFIX_BACK_BY_TYPE = {
    ("material", "density"): ("_mg_cm3", 1), ("substance", "density"): ("_mg_ml", 1),
    ("substance", "hydration"): ("_pct", 1),
}
# Plain keys that share a name with a renamed one in another record type: never renamed back.
NO_SUFFIX_BY_TYPE = {("shape", "fill")}
PERCENT_FLOAT_KEYS = {"tool_substitution"}
MERGED_BACK = {"def.meta": "meta", "def.fastener_status": "status", "def.scent_dominance": "dominance",
               "mechanic.reaction.default": "default"}
MERGED_KEYS_BACK = {"dominance": {"d": "D"}}


def ref_id(path: str) -> str:
    """The flat ID a full path names (its prefix's type decides; an item's is its last segment)."""
    for prefix, _ in sorted(PEO_PREFIXES.items(), key=lambda kv: -len(kv[0])):
        if path.startswith(prefix + "."):
            rest = path[len(prefix) + 1:]
            break
    else:
        rest = path.rsplit(".", 1)[1]
    return ".".join(s[1:] if s[:1] == "_" and s[1:2].isdigit() else s for s in rest.split("."))


def is_number_like(v) -> bool:
    return (isinstance(v, int) and not isinstance(v, bool)) or (
        isinstance(v, list) and bool(v) and all(isinstance(x, int) and not isinstance(x, bool) for x in v))


def suffix_back(rtype: str, key: str):
    if (rtype, key) in NO_SUFFIX_BY_TYPE:
        return None
    return SUFFIX_BACK_BY_TYPE.get((rtype, key)) or SUFFIX_BACK.get(key)


def scaled(v, scale: int):
    if isinstance(v, list):
        return [scaled(x, scale) for x in v]
    if v % scale:
        raise ValueError(f"{v} is not a whole number of units of {scale}")
    return v // scale


def back_value(rtype: str, key: str, v, peo_db):
    import peo_load  # noqa: PLC0415
    if isinstance(v, peo_load.Ref):
        return ref_id(str(v))
    if isinstance(v, dict):
        return back_table(rtype, v, key, peo_db)
    if isinstance(v, list):
        if key == "ops":
            return [back_op(rtype, op, peo_db) for op in v]
        return [back_value(rtype, key, x, peo_db) for x in v]
    if key in PERCENT_FLOAT_KEYS and isinstance(v, int):
        return v / 100
    return v


def back_op(rtype: str, op: dict, peo_db) -> dict:
    segments = op.get("path", "").split(".")
    last = segments[-1]
    if last == "dims":
        segments[-1] = "dims_mm"
    elif suffix_back(rtype, last):
        segments[-1] = last + suffix_back(rtype, last)[0]
    out = {}
    for key, v in op.items():
        if key == "path":
            out[key] = ".".join(segments)
        elif key == "value" and op.get("op") == "mul":
            out[key] = v / 100
        elif key == "value" and last == "dims" and isinstance(v, dict):
            out[key] = [v["l"], v["w"], v["h"]]
        elif key == "value" and op.get("op") in ("set", "add") and suffix_back(rtype, last) and is_number_like(v):
            out[key] = scaled(v, suffix_back(rtype, last)[1])
        else:
            out[key] = back_value(rtype, key, v, peo_db)
    return out


def back_table(rtype: str, table: dict, parent_key: str, peo_db) -> dict:
    out = {}
    for key, v in table.items():
        if key == "dims" and isinstance(v, dict) and set(v) == {"l", "w", "h"}:
            out["dims_mm"] = [v["l"], v["w"], v["h"]]
        elif key == "compartments" and rtype == "item" and isinstance(v, dict):
            out["compartments"] = [back_table(rtype, c, "compartment", peo_db) for c in v.values()]
        elif suffix_back(rtype, key) and is_number_like(v):
            suffix, scale = suffix_back(rtype, key)
            out[key + suffix] = scaled(v, scale)
        else:
            out[key] = back_value(rtype, key, v, peo_db)
    return out


def back_room_purposes(room: dict, peo_db) -> None:
    """A room object's purpose overrides are keyed by its item's compartment names."""
    import to_peo  # noqa: PLC0415 - only its snake_case rule, the naming the docs give
    for obj in room.get("objects", []):
        overrides = obj.get("purposes")
        if not isinstance(overrides, dict):
            continue
        item = peo_db.get("item", obj.get("item", "")) or {}
        names = {to_peo.snake(c.get("name", "")): c.get("name", "") for c in (item.get("compartments") or {}).values()}
        obj["purposes"] = {names.get(k, k): v for k, v in overrides.items()}


def back_to_toml(peo_db) -> "common.Db":
    out = common.Db()
    for rtype, recs in peo_db.records.items():
        for rid, rec in recs.items():
            back = back_table(rtype, rec, "", peo_db)
            if rtype == "room":
                back_room_purposes(back, peo_db)
            out.records[rtype][rid] = back
    for rtype, table in peo_db.merged.items():
        renames = MERGED_KEYS_BACK.get(rtype, {})
        out.merged[rtype] = {renames.get(k, k): v for k, v in back_table(rtype, table, "", peo_db).items()}
    return out


def first_difference(a, b, where: str = "") -> str | None:
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                return f"{where}.{k}: {'missing after' if k not in b else 'added after'} the round trip"
            d = first_difference(a[k], b[k], f"{where}.{k}")
            if d:
                return d
        return None
    if isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        for i, (x, y) in enumerate(zip(a, b)):
            d = first_difference(x, y, f"{where}.{i}")
            if d:
                return d
        return None
    return None if a == b else f"{where}: {a!r} -> {b!r}"


def peo_round_trip(content_dir: Path) -> list[str]:
    """Every difference between content_dir and its .peo round trip."""
    import tempfile  # noqa: PLC0415

    import peo_load  # noqa: PLC0415
    import to_peo  # noqa: PLC0415

    original = common.load(content_dir)
    with tempfile.TemporaryDirectory() as tmp:
        for rel, text in to_peo.convert(content_dir).items():
            target = Path(tmp) / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding="utf-8")
        peo_db = peo_load.load(Path(tmp))
    if peo_db.errors:
        return [f"peo_load: {e}" for e in peo_db.errors]
    back = back_to_toml(peo_db)
    diffs = []
    for rtype in sorted(set(original.records) | set(back.records)):
        mine, theirs = original.records.get(rtype, {}), back.records.get(rtype, {})
        for rid in sorted(set(mine) | set(theirs)):
            if rid not in theirs or rid not in mine:
                diffs.append(f"{rtype}.{rid}: {'lost' if rid not in theirs else 'gained'} in the round trip")
                continue
            want = dict(mine[rid])
            if rtype == "item" and want.get("abstract") is False:
                want.pop("abstract")  # `abstract = false` is dropped by design
            d = first_difference(want, theirs[rid])
            if d:
                diffs.append(f"{rtype}.{rid}{d}")
    for rtype in sorted(set(original.merged) | set(back.merged)):
        d = first_difference(original.merged.get(rtype), back.merged.get(rtype))
        if d:
            diffs.append(f"[{rtype}]{d}")
    for iid in sorted(original["item"]):
        if iid in back["item"]:
            d = first_difference(derive.resolve_item(original, iid), derive.resolve_item(back, iid))
            if d:
                diffs.append(f"resolved item.{iid}{d}")
    return diffs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--content", type=Path, default=common.CONTENT, help="content directory")
    parser.add_argument("--engine", type=Path, help="peo_content_dump to diff against")
    parser.add_argument("--unchanged-from", type=Path, metavar="FILE",
                        help="an earlier print of these lines; fail if any item in it changed")
    parser.add_argument("--peo", action="store_true",
                        help="convert to .peo, read it back and compare every record (PEO-136)")
    args = parser.parse_args()

    if args.peo:
        diffs = peo_round_trip(args.content)
        if diffs:
            print(f"golden --peo: {len(diffs)} differences; first {SHOWN_DIFFERENCES}:")
            for d in diffs[:SHOWN_DIFFERENCES]:
                print(f"  {d}")
            return 1
        print(f"golden --peo: every record and resolved item of {args.content} survives the .peo round trip")
        return 0

    expected = golden_lines(args.content)
    if args.unchanged_from is not None:
        before = args.unchanged_from.read_text(encoding="utf-8").splitlines()
        differences, added = unchanged_differences(before, expected)
        if added:
            print(f"golden: {len(added)} new item(s): {', '.join(added)}")
        if differences:
            print(f"golden: {len(differences)} difference(s) in existing items; first {SHOWN_DIFFERENCES}:")
            for line in differences[:SHOWN_DIFFERENCES]:
                print(line)
            return 1
        print(f"golden: {len(by_item(before))} existing items unchanged (parent lines and tag order aside)")
        return 0
    if args.engine is None:
        sys.stdout.write("".join(line + "\n" for line in expected))
        return 0

    run = subprocess.run([str(args.engine), str(args.content)], capture_output=True, text=True)
    if run.returncode != 0:
        print(f"golden: {args.engine.name} failed ({run.returncode}):\n{run.stderr}", file=sys.stderr)
        return 1
    actual = run.stdout.splitlines()
    differences = [line for line in difflib.unified_diff(expected, actual, "derive.py", "engine", n=0, lineterm="")
                   if line[:1] in "+-" and not line.startswith(("+++", "---"))]
    if differences:
        print(f"golden: {len(differences)} differing lines (- derive.py, + engine); first {SHOWN_DIFFERENCES}:")
        for line in differences[:SHOWN_DIFFERENCES]:
            print(line)
        return 1
    print(f"golden: {len(expected)} lines over {len(db_items(expected))} items match derive.py")
    return 0


def db_items(lines: list[str]) -> set[str]:
    return {line.split("\t", 1)[0] for line in lines}


if __name__ == "__main__":
    sys.exit(main())
