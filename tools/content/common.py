"""Shared loading for content tools. Loads every TOML file under content/ into a database
keyed by record type -> id -> record, remembering which file each record came from."""

from __future__ import annotations

import sys
import tomllib
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / "content"
TESTS = ROOT / "tests" / "content"

# Record types that hold id -> record maps.
RECORD_TYPES = {
    "property", "feature", "capability", "stimulus", "namespace", "tag", "fastener",
    "scent", "action", "shape", "skill", "schema", "material", "substance", "item",
    "modifier", "detail_table", "company", "brand", "store_chain", "setting",
    "building", "room", "loot", "outdoor_set", "profile",
}
# Record types that are single merged tables rather than id -> record maps.
MERGED_TYPES = {"dominance", "meta", "status", "default"}


class Db:
    def __init__(self) -> None:
        self.records: dict[str, dict[str, dict]] = defaultdict(dict)
        self.merged: dict[str, dict] = defaultdict(dict)
        self.origin: dict[tuple[str, str], Path] = {}
        self.errors: list[str] = []

    def __getitem__(self, rtype: str) -> dict[str, dict]:
        return self.records[rtype]

    def get(self, rtype: str, rid: str) -> dict | None:
        return self.records[rtype].get(rid)

    def where(self, rtype: str, rid: str) -> str:
        p = self.origin.get((rtype, rid))
        return str(p.relative_to(ROOT)) if p else "?"


def _merge(dst: dict, src: dict) -> None:
    for k, v in src.items():
        if isinstance(v, dict) and isinstance(dst.get(k), dict):
            _merge(dst[k], v)
        else:
            dst[k] = v


def load(content_dir: Path = CONTENT) -> Db:
    db = Db()
    for f in sorted(content_dir.rglob("*.toml")):
        try:
            with open(f, "rb") as fh:
                data = tomllib.load(fh)
        except tomllib.TOMLDecodeError as e:
            db.errors.append(f"{f.relative_to(ROOT)}: TOML parse error: {e}")
            continue
        for rtype, recs in data.items():
            if rtype in MERGED_TYPES:
                if isinstance(recs, dict):
                    _merge(db.merged[rtype], recs)
                continue
            if rtype not in RECORD_TYPES:
                db.errors.append(f"{f.relative_to(ROOT)}: unknown record type [{rtype}.*]")
                continue
            if not isinstance(recs, dict):
                db.errors.append(f"{f.relative_to(ROOT)}: [{rtype}] must be a table of records")
                continue
            for rid, rec in recs.items():
                if not isinstance(rec, dict):
                    db.errors.append(f"{f.relative_to(ROOT)}: {rtype}.{rid} is not a table")
                    continue
                if rid in db.records[rtype]:
                    db.errors.append(
                        f"{f.relative_to(ROOT)}: duplicate {rtype}.{rid} (first in {db.where(rtype, rid)})")
                    continue
                db.records[rtype][rid] = rec
                db.origin[(rtype, rid)] = f
    return db


def load_tests(kind: str) -> list[tuple[Path, dict]]:
    out = []
    d = TESTS / kind
    if d.exists():
        for f in sorted(d.rglob("*.toml")):
            with open(f, "rb") as fh:
                out.append((f, tomllib.load(fh)))
    return out


def fail_if(errors: list[str], label: str) -> int:
    for e in errors:
        print(f"ERROR {e}")
    if errors:
        print(f"{label}: {len(errors)} error(s)")
        return 1
    return 0


if __name__ == "__main__":
    db = load()
    for rtype in sorted(db.records):
        print(f"{rtype:14} {len(db.records[rtype])}")
    sys.exit(fail_if(db.errors, "load"))
