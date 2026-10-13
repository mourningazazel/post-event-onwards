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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--content", type=Path, default=common.CONTENT, help="content directory")
    parser.add_argument("--engine", type=Path, help="peo_content_dump to diff against")
    parser.add_argument("--unchanged-from", type=Path, metavar="FILE",
                        help="an earlier print of these lines; fail if any item in it changed")
    args = parser.parse_args()

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
