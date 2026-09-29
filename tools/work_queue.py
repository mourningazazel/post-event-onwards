#!/usr/bin/env python3
"""Work queue contract for Post-Event Onwards.

This file OWNS the queue schema. Everything else (validate_docs.py, skills,
CI) imports or shells out to it; nobody edits WORK_QUEUE.json by hand.

Usage:
  python3 tools/work_queue.py summary
  python3 tools/work_queue.py list [--status S] [--owner O]
  python3 tools/work_queue.py next [--owner builder|architect]
  python3 tools/work_queue.py show PEO-001
  python3 tools/work_queue.py check
  python3 tools/work_queue.py add --type feature --title "..." [--severity S3] [--effort M] [--owner builder]
  python3 tools/work_queue.py set PEO-001 --status InProgress [--owner builder] [--note "..."] [--by builder]
  python3 tools/work_queue.py note PEO-001 --by builder --text "..."
  python3 tools/work_queue.py brief PEO-001 --file brief.json      (architect fills the brief)

Briefs are not in the queue JSON: each lives in docs/production/briefs/<id>.md,
written and read only through `brief` / `show` here. A list entry that spans
lines indents its continuations two spaces; an unknown `## ` heading is an
error, not a silently dropped section; `check` round-trips every brief.
  python3 tools/work_queue.py complete PEO-001 [--by architect] [--commit abc123]
  python3 tools/work_queue.py defer PEO-001 / promote PEO-001
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QUEUE = ROOT / "WORK_QUEUE.json"
DEFERRED = ROOT / "DEFERRED_WORK.json"
COMPLETED_DIR = ROOT / "docs" / "COMPLETED_WORK"
HANDOFFS_DIR = ROOT / "docs" / "production" / "handoffs"
BRIEFS_DIR = ROOT / "docs/production/briefs"

SCHEMA_VERSION = 1
ID_PREFIX = "PEO"
TYPES = ("feature", "bug", "chore", "research", "design", "perf")
STATUSES = ("Pending", "InProgress", "Validation", "AwaitingUser", "Blocked")
SEVERITIES = ("S1", "S2", "S3", "S4")  # S1 = blocks play, S4 = polish
EFFORTS = ("S", "M", "L", "XL")
OWNERS = ("architect", "builder", "user")
ACTORS = ("architect", "builder", "user")
BRIEF_FIELDS = ("goal", "context", "units", "acceptance", "tests", "manual", "out_of_scope")
# Section heading in the brief file <-> field name. Order is the file order.
BRIEF_SECTIONS = (
    ("Goal", "goal"),
    ("Context", "context"),
    ("Units", "units"),
    ("Acceptance", "acceptance"),
    ("Tests", "tests"),
    ("Manual", "manual"),
    ("Out of scope", "out_of_scope"),
)
# Headings are matched case-insensitively: "Out of Scope" must not silently
# drop a section. An unknown heading is an error, never a skip.
HEADING_TO_FIELD = {h.lower(): f for h, f in BRIEF_SECTIONS}
# A list entry spans several lines by indenting its continuations two spaces.
CONTINUATION = "  "


def today() -> str:
    return dt.date.today().isoformat()


def load(path: Path) -> dict:
    if not path.exists():
        return {"schema": SCHEMA_VERSION, "next_id": 1, "items": []}
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def save(path: Path, data: dict) -> None:
    data["items"].sort(key=lambda i: (i["order"], i["id"]))
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")


def find(data: dict, item_id: str) -> dict:
    for item in data["items"]:
        if item["id"] == item_id:
            return item
    sys.exit(f"error: {item_id} not found in {QUEUE.name}")


def empty_brief() -> dict:
    return {k: ([] if k != "goal" else "") for k in BRIEF_FIELDS}


def brief_path(item_id: str) -> Path:
    return BRIEFS_DIR / f"{item_id}.md"


def _escape_goal_line(line: str) -> str:
    """A goal line that looks like a heading would be parsed as one."""
    stripped = line.lstrip("\\")
    return "\\" + line if stripped.startswith("## ") else line


def _unescape_goal_line(line: str) -> str:
    stripped = line.lstrip("\\")
    return line[1:] if line.startswith("\\") and stripped.startswith("## ") else line


def parse_brief(text: str, source: str) -> dict:
    """Parse brief markdown into the 7-key brief dict.

    `source` names the origin (a path, usually) in error messages. Text that
    cannot be placed is an error: losing it quietly is the bug this guards.
    """
    brief = empty_brief()
    field: str | None = None
    section: str = ""
    goal: list[str] = []
    for line in text.split("\n"):
        if line.startswith("## "):
            heading = line[3:].strip()
            field = HEADING_TO_FIELD.get(heading.lower())
            if field is None:
                known = ", ".join(h for h, _ in BRIEF_SECTIONS)
                sys.exit(f"error: {source}: unknown brief section '## {heading}'; expected one of: {known}")
            section = heading
            continue
        if field is None:
            continue
        if field == "goal":
            # The goal is prose: every line belongs to it, verbatim.
            goal.append(_unescape_goal_line(line))
        elif line.startswith("- "):
            brief[field].append(line[2:])
        elif line.startswith(CONTINUATION) and brief[field]:
            # Indented line: the rest of the entry above it, newline restored.
            brief[field][-1] += "\n" + line[len(CONTINUATION):]
        elif line.strip():
            # Not an entry, not a continuation, not blank: it would be dropped.
            sys.exit(
                f"error: {source}: unparseable line in section '## {section}': {line!r}; "
                f"a list entry starts with '- ' and its continuations indent {len(CONTINUATION)} spaces"
            )
    while goal and not goal[0].strip():
        goal.pop(0)
    while goal and not goal[-1].strip():
        goal.pop()
    brief["goal"] = "\n".join(goal)
    return brief


def read_brief(item_id: str) -> dict:
    """Load docs/production/briefs/<id>.md into the 7-key brief dict.

    Missing file or missing section means empty, so callers never special-case
    a brief that has not been written yet.
    """
    path = brief_path(item_id)
    if not path.exists():
        return empty_brief()
    name = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
    return parse_brief(path.read_text(encoding="utf-8"), str(name))


def render_brief(item_id: str, title: str, brief: dict) -> str:
    """Render a brief dict to markdown. One list entry per line, never wrapped;
    an entry or goal that spans lines keeps them, continuations indented."""
    out = [f"# {item_id} · {title}", ""]
    for heading, field in BRIEF_SECTIONS:
        value = brief.get(field) or ("" if field == "goal" else [])
        if not value:
            continue
        out.append(f"## {heading}")
        out.append("")
        if field == "goal":
            out.extend(_escape_goal_line(line) for line in value.split("\n"))
        else:
            for entry in value:
                first, *rest = entry.split("\n")
                out.append(f"- {first}")
                out.extend(CONTINUATION + line for line in rest)
        out.append("")
    return "\n".join(out).rstrip("\n") + "\n"


def write_brief(item_id: str, title: str, brief: dict) -> None:
    BRIEFS_DIR.mkdir(parents=True, exist_ok=True)
    brief_path(item_id).write_text(render_brief(item_id, title, brief), encoding="utf-8")


def validate_item(item: dict, errors: list[str], seen: set[str]) -> None:
    iid = item.get("id", "<no id>")
    if iid in seen:
        errors.append(f"{iid}: duplicate id")
    seen.add(iid)
    required = ("id", "order", "type", "title", "status", "severity", "effort", "owner", "notes", "references", "depends_on", "created")
    for key in required:
        if key not in item:
            errors.append(f"{iid}: missing field '{key}'")
    if not str(iid).startswith(ID_PREFIX + "-"):
        errors.append(f"{iid}: id must look like {ID_PREFIX}-001")
    if item.get("type") not in TYPES:
        errors.append(f"{iid}: type must be one of {TYPES}")
    if item.get("status") not in STATUSES:
        errors.append(f"{iid}: status must be one of {STATUSES}")
    if item.get("severity") not in SEVERITIES:
        errors.append(f"{iid}: severity must be one of {SEVERITIES}")
    if item.get("effort") not in EFFORTS:
        errors.append(f"{iid}: effort must be one of {EFFORTS}")
    if item.get("owner") not in OWNERS:
        errors.append(f"{iid}: owner must be one of {OWNERS}")
    brief = read_brief(iid)
    if item.get("status") == "InProgress" and not brief.get("acceptance"):
        errors.append(f"{iid}: InProgress items need acceptance criteria in the brief")
    if item.get("status") == "Blocked":
        handoff = HANDOFFS_DIR / f"{iid}.md"
        if not handoff.exists():
            errors.append(f"{iid}: Blocked items need a handoff doc at {handoff.relative_to(ROOT)}")
    for dep in item.get("depends_on", []):
        if not str(dep).startswith(ID_PREFIX + "-"):
            errors.append(f"{iid}: bad dependency id {dep}")


def clip(value, limit: int = 160) -> str:
    """repr() short enough to read in an error line."""
    text = repr(value)
    return text if len(text) <= limit else text[:limit] + "..."


def cmd_check(_: argparse.Namespace) -> int:
    errors: list[str] = []
    seen: set[str] = set()
    for path in (QUEUE, DEFERRED):
        data = load(path)
        if data.get("schema") != SCHEMA_VERSION:
            errors.append(f"{path.name}: schema must be {SCHEMA_VERSION}")
        for item in data["items"]:
            validate_item(item, errors, seen)
    q = load(QUEUE)
    ids = {i["id"] for i in q["items"]}
    for item in q["items"]:
        for dep in item.get("depends_on", []):
            if dep not in ids:
                errors.append(f"{item['id']}: depends on {dep} which is not in the active queue")
    # Every brief must survive a write/read cycle unchanged, in memory, so a
    # brief that would lose text on the next edit fails the commit instead.
    for item in q["items"]:
        brief = read_brief(item["id"])
        again = parse_brief(render_brief(item["id"], item["title"], brief), f"{item['id']} (round-trip)")
        for field in BRIEF_FIELDS:
            if again[field] != brief[field]:
                errors.append(f"{item['id']}: brief field '{field}' does not survive a write/read cycle: {clip(brief[field])} -> {clip(again[field])}")
    # Orphan brief files are noise, not breakage: report, never fail.
    for path in sorted(BRIEFS_DIR.glob(f"{ID_PREFIX}-*.md")):
        if path.stem not in ids:
            print(f"queue note: orphan brief {path.relative_to(ROOT)} (no live item {path.stem})")
    for e in errors:
        print("QUEUE ERROR:", e)
    if not errors:
        print("queue ok")
    return 1 if errors else 0


def status_line(item: dict) -> str:
    deps = f" deps={','.join(item['depends_on'])}" if item["depends_on"] else ""
    return f"{item['id']:<8} {item['status']:<12} {item['owner']:<9} {item['severity']} {item['effort']:<2} [{item['type']}] {item['title']}{deps}"


def cmd_summary(_: argparse.Namespace) -> int:
    q = load(QUEUE)
    counts = {s: 0 for s in STATUSES}
    for item in q["items"]:
        counts[item["status"]] += 1
    print(f"active items: {len(q['items'])}  deferred: {len(load(DEFERRED)['items'])}")
    print("  " + "  ".join(f"{s}={n}" for s, n in counts.items()))
    for item in q["items"]:
        if item["status"] in ("InProgress", "Validation", "AwaitingUser", "Blocked"):
            print("  " + status_line(item))
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    q = load(DEFERRED if args.deferred else QUEUE)
    for item in q["items"]:
        if args.status and item["status"] != args.status:
            continue
        if args.owner and item["owner"] != args.owner:
            continue
        print(status_line(item))
    return 0


def actionable(item: dict, done_ids: set[str]) -> bool:
    if item["status"] != "Pending":
        return False
    return all(dep in done_ids for dep in item["depends_on"])


def cmd_next(args: argparse.Namespace) -> int:
    q = load(QUEUE)
    active_ids = {i["id"] for i in q["items"]}
    # A dependency that is no longer in the queue has been completed.
    for item in q["items"]:
        if args.owner and item["owner"] != args.owner:
            continue
        unmet = [d for d in item["depends_on"] if d in active_ids]
        if item["status"] == "Pending" and not unmet:
            print(status_line(item))
            return 0
    print("nothing actionable")
    return 2


def cmd_show(args: argparse.Namespace) -> int:
    item = find(load(QUEUE), args.id)
    print(json.dumps({**item, "brief": read_brief(args.id)}, indent=2, ensure_ascii=False))
    return 0


def cmd_add(args: argparse.Namespace) -> int:
    q = load(QUEUE)
    iid = f"{ID_PREFIX}-{q['next_id']:03d}"
    q["next_id"] += 1
    order = args.order if args.order is not None else (max((i["order"] for i in q["items"]), default=0) + 10)
    item = {
        "id": iid,
        "order": order,
        "type": args.type,
        "title": args.title,
        "status": "Pending",
        "severity": args.severity,
        "effort": args.effort,
        "owner": args.owner,
        "created": today(),
        "notes": [],
        "references": args.ref or [],
        "depends_on": args.depends_on or [],
    }
    if args.goal:
        write_brief(iid, args.title, {**empty_brief(), "goal": args.goal})
    q["items"].append(item)
    save(QUEUE, q)
    print(iid)
    return 0


def add_note(item: dict, by: str, text: str) -> None:
    item["notes"].append({"at": today(), "by": by, "text": text})


def cmd_set(args: argparse.Namespace) -> int:
    q = load(QUEUE)
    item = find(q, args.id)
    if args.status:
        item["status"] = args.status
    if args.owner:
        item["owner"] = args.owner
    if args.order is not None:
        item["order"] = args.order
    if args.note:
        add_note(item, args.by, args.note)
    save(QUEUE, q)
    print(status_line(item))
    return 0


def cmd_note(args: argparse.Namespace) -> int:
    q = load(QUEUE)
    item = find(q, args.id)
    add_note(item, args.by, args.text)
    save(QUEUE, q)
    return 0


def cmd_brief(args: argparse.Namespace) -> int:
    q = load(QUEUE)
    item = find(q, args.id)
    with open(args.file, encoding="utf-8") as f:
        brief = json.load(f)
    unknown = set(brief) - set(BRIEF_FIELDS)
    if unknown:
        sys.exit(f"error: unknown brief fields {sorted(unknown)}; allowed {BRIEF_FIELDS}")
    merged = read_brief(args.id)
    merged.update(brief)
    write_brief(args.id, item["title"], merged)
    print(f"{args.id}: brief updated ({brief_path(args.id).relative_to(ROOT)})")
    return 0


def cmd_complete(args: argparse.Namespace) -> int:
    q = load(QUEUE)
    item = find(q, args.id)
    COMPLETED_DIR.mkdir(parents=True, exist_ok=True)
    log = COMPLETED_DIR / f"{today()}.md"
    new = not log.exists()
    with log.open("a", encoding="utf-8") as f:
        if new:
            f.write(f"# Completed work · {today()}\n\n")
        commit = f" · commit `{args.commit}`" if args.commit else ""
        f.write(f"## {item['id']} · {item['title']}\n\n")
        f.write(f"- type {item['type']} · severity {item['severity']} · effort {item['effort']} · closed by {args.by}{commit}\n")
        goal = read_brief(args.id).get("goal")
        if goal:
            f.write(f"- goal: {goal}\n")
        for n in item["notes"][-3:]:
            f.write(f"- {n['at']} {n['by']}: {n['text']}\n")
        f.write("\n")
    q["items"] = [i for i in q["items"] if i["id"] != args.id]
    # A finished item satisfies every dependency on it.
    for other in q["items"]:
        other["depends_on"] = [d for d in other["depends_on"] if d != args.id]
    save(QUEUE, q)
    print(f"{args.id} logged to {log.relative_to(ROOT)} and removed from queue")
    return 0


def move(src: Path, dst: Path, item_id: str, status: str) -> int:
    s = load(src)
    d = load(dst)
    item = find(s, item_id) if src == QUEUE else next((i for i in s["items"] if i["id"] == item_id), None)
    if item is None:
        sys.exit(f"error: {item_id} not found in {src.name}")
    s["items"] = [i for i in s["items"] if i["id"] != item_id]
    item["status"] = status
    d["items"].append(item)
    d["next_id"] = max(d.get("next_id", 1), load(QUEUE)["next_id"])
    save(src, s)
    save(dst, d)
    print(f"{item_id} -> {dst.name}")
    return 0


def cmd_defer(args: argparse.Namespace) -> int:
    return move(QUEUE, DEFERRED, args.id, "Pending")


def cmd_promote(args: argparse.Namespace) -> int:
    return move(DEFERRED, QUEUE, args.id, "Pending")


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("summary").set_defaults(fn=cmd_summary)
    sub.add_parser("check").set_defaults(fn=cmd_check)

    sp = sub.add_parser("list")
    sp.add_argument("--status", choices=STATUSES)
    sp.add_argument("--owner", choices=OWNERS)
    sp.add_argument("--deferred", action="store_true")
    sp.set_defaults(fn=cmd_list)

    sp = sub.add_parser("next")
    sp.add_argument("--owner", choices=OWNERS)
    sp.set_defaults(fn=cmd_next)

    sp = sub.add_parser("show")
    sp.add_argument("id")
    sp.set_defaults(fn=cmd_show)

    sp = sub.add_parser("add")
    sp.add_argument("--type", required=True, choices=TYPES)
    sp.add_argument("--title", required=True)
    sp.add_argument("--severity", default="S3", choices=SEVERITIES)
    sp.add_argument("--effort", default="M", choices=EFFORTS)
    sp.add_argument("--owner", default="architect", choices=OWNERS, help="who acts next (default: architect writes the brief)")
    sp.add_argument("--order", type=int)
    sp.add_argument("--goal")
    sp.add_argument("--ref", action="append")
    sp.add_argument("--depends-on", action="append")
    sp.set_defaults(fn=cmd_add)

    sp = sub.add_parser("set")
    sp.add_argument("id")
    sp.add_argument("--status", choices=STATUSES)
    sp.add_argument("--owner", choices=OWNERS)
    sp.add_argument("--order", type=int)
    sp.add_argument("--note")
    sp.add_argument("--by", default="builder", choices=ACTORS)
    sp.set_defaults(fn=cmd_set)

    sp = sub.add_parser("note")
    sp.add_argument("id")
    sp.add_argument("--by", required=True, choices=ACTORS)
    sp.add_argument("--text", required=True)
    sp.set_defaults(fn=cmd_note)

    sp = sub.add_parser("brief")
    sp.add_argument("id")
    sp.add_argument("--file", required=True, help="JSON object with any of: " + ", ".join(BRIEF_FIELDS))
    sp.set_defaults(fn=cmd_brief)

    sp = sub.add_parser("complete")
    sp.add_argument("id")
    sp.add_argument("--by", default="architect", choices=ACTORS)
    sp.add_argument("--commit")
    sp.set_defaults(fn=cmd_complete)

    for name, fn in (("defer", cmd_defer), ("promote", cmd_promote)):
        sp = sub.add_parser(name)
        sp.add_argument("id")
        sp.set_defaults(fn=fn)

    args = p.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
