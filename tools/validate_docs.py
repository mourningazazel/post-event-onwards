#!/usr/bin/env python3
"""Keep the agent-facing documents small enough to actually be read.

Checks (all fast, no network):
  * word caps on rolling docs and the queue files
  * queue schema (delegates to work_queue.py check)
  * every canonical skill in .agents/skills has a Claude wrapper in .claude/skills
  * every Blocked queue item has a handoff doc
  * docs/README.md links resolve

Run:  python3 tools/validate_docs.py
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import work_queue  # noqa: E402

# Word caps. Raise one only with a decision in docs/production/decisions.md.
WORD_CAPS = {
    "AGENTS.md": 450,
    "CLAUDE.md": 300,
    "docs/README.md": 500,
    "docs/roles.md": 1200,
    "docs/bottlenecks.md": 900,
    "docs/production/vision.md": 900,
    "docs/production/decisions.md": 1500,
    "docs/production/DECISIONS_NEEDED.md": 800,
    "WORK_QUEUE.json": 2500,  # metadata only since D-009; briefs are uncapped files
    "DEFERRED_WORK.json": 1500,
}
SKILL_WORD_CAP = 600


def words_in_markdown(text: str) -> int:
    text = re.sub(r"```.*?```", " ", text, flags=re.S)
    return len(re.findall(r"\S+", text))


def words_in_json(path: Path) -> int:
    """Count only human-written content words, not keys or structure."""
    def walk(node) -> int:
        if isinstance(node, dict):
            return sum(walk(v) for v in node.values())
        if isinstance(node, list):
            return sum(walk(v) for v in node)
        if isinstance(node, str):
            return len(node.split())
        return 0
    return walk(json.loads(path.read_text(encoding="utf-8")))


def main() -> int:
    errors: list[str] = []

    for rel, cap in WORD_CAPS.items():
        path = ROOT / rel
        if not path.exists():
            errors.append(f"{rel}: missing")
            continue
        n = words_in_json(path) if path.suffix == ".json" else words_in_markdown(path.read_text(encoding="utf-8"))
        if n > cap:
            errors.append(f"{rel}: {n} words, cap is {cap}")

    canonical = {p.parent.name for p in (ROOT / ".agents" / "skills").glob("*/SKILL.md")}
    wrappers = {p.parent.name for p in (ROOT / ".claude" / "skills").glob("*/SKILL.md")}
    for name in sorted(canonical - wrappers):
        errors.append(f"skill {name}: canonical .agents/skills/{name}/SKILL.md has no .claude/skills wrapper")
    for name in sorted(wrappers - canonical):
        errors.append(f"skill {name}: .claude/skills wrapper has no canonical .agents/skills source")
    for p in (ROOT / ".agents" / "skills").glob("*/SKILL.md"):
        n = words_in_markdown(p.read_text(encoding="utf-8"))
        if n > SKILL_WORD_CAP:
            errors.append(f"{p.relative_to(ROOT)}: {n} words, cap is {SKILL_WORD_CAP}")
        if not p.read_text(encoding="utf-8").startswith("---"):
            errors.append(f"{p.relative_to(ROOT)}: needs YAML front matter with name and description")

    index = ROOT / "docs" / "README.md"
    if index.exists():
        for link in re.findall(r"\]\(([^)#]+)(?:#[^)]*)?\)", index.read_text(encoding="utf-8")):
            if link.startswith(("http://", "https://")):
                continue
            if not (index.parent / link).exists() and not (ROOT / link).exists():
                errors.append(f"docs/README.md: broken link {link}")

    if work_queue.cmd_check(None) != 0:
        errors.append("queue check failed (see above)")

    for e in errors:
        print("DOC ERROR:", e)
    print("docs ok" if not errors else f"{len(errors)} doc problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
