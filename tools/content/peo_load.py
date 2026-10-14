"""Read a .peo content tree into the tools' Db (PEO-136), as common.load reads TOML.

    db = peo_load.load(content_dir)

Record type and flat ID come from the path table in to_peo.py (longest prefix wins, so
item.material.* is a material); an item's flat ID is its last segment and its parent is its
header path minus that segment when that names an item. Values become plain Python: Integer and
Quantity int (base unit), Range [low, high], Boolean bool, String and Expr str, Path a Ref (a
str holding the full path), List list, Block dict. Field names are the .peo ones (no unit
suffixes): golden.py --peo maps them back to compare with the TOML.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402
import peo  # noqa: E402
from to_peo import ITEM_ROOT, MERGED_PATH, PATH_PREFIX  # noqa: E402


class Ref(str):
    """A reference by full path (a .peo Path value)."""


# Longest prefix first, so item.material.x is a material, not an item.
_PREFIXES = sorted(((prefix + ".", rtype) for rtype, prefix in PATH_PREFIX.items()), key=lambda p: -len(p[0]))
_MERGED = {path: rtype for rtype, path in MERGED_PATH.items()}


def record_type(path: str) -> tuple[str, str] | None:
    """(record type, flat ID) for a header path, or None for one in no table."""
    for prefix, rtype in _PREFIXES:
        if path.startswith(prefix):
            return rtype, path[len(prefix):]
    if path.startswith(ITEM_ROOT + "."):
        return "item", path.rsplit(".", 1)[1]
    return None


def plain(v):
    if isinstance(v, peo.Block):
        return {k: plain(x) for k, x in v.fields}
    if isinstance(v, peo.List):
        return [plain(x) for x in v.items]
    if isinstance(v, (peo.Integer, peo.Quantity)):
        return v.value
    if isinstance(v, peo.Range):
        return [v.low.value, v.high.value]
    if isinstance(v, peo.Boolean):
        return v.value
    if isinstance(v, peo.String):
        return v.text
    if isinstance(v, peo.Expr):
        return v.text
    if isinstance(v, peo.Path):
        return Ref(v.text)
    raise TypeError(v)


def load(content_dir: Path) -> common.Db:
    db = common.Db()
    parsed = []
    for f in sorted(content_dir.rglob("*.peo")):
        rel = f.relative_to(content_dir)
        result = peo.parse_peo(str(rel), f.read_text(encoding="utf-8"))
        db.errors += result.errors
        parsed.append((f, result))
    item_paths = {}
    for f, result in parsed:
        for rec in result.records:
            kind = record_type(rec.path)
            if kind and kind[0] == "item":
                item_paths[rec.path] = kind[1]
    first_file: dict[tuple[str, str], Path] = {}
    for f, result in parsed:
        for rec in result.records:
            if rec.path in _MERGED:
                rtype = _MERGED[rec.path]
                common._merge(db.merged[rtype], plain(rec.value))
                continue
            kind = record_type(rec.path)
            if kind is None:
                db.errors.append(f"{f}: record '{rec.path}' is under no root in the path table")
                continue
            rtype, rid = kind
            if (rtype, rid) in first_file:
                db.errors.append(f"{f}: duplicate {rtype}.{rid} (first in {first_file[(rtype, rid)]})")
                continue
            value = plain(rec.value)
            if not isinstance(value, dict):
                db.errors.append(f"{f}: {rec.path} is not a block")
                continue
            if rtype == "item":
                up = rec.path.rsplit(".", 1)[0]
                if up != ITEM_ROOT:
                    if up not in item_paths:
                        db.errors.append(f"{f}: {rec.path}: its parent path '{up}' names no item")
                        continue
                    value["parent"] = item_paths[up]
                if rec.abstract:
                    value["abstract"] = True
            first_file[(rtype, rid)] = f
            db.records[rtype][rid] = value
            db.origin[(rtype, rid)] = f
    return db
