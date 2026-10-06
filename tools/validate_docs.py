#!/usr/bin/env python3
"""Keep the agent-facing documents small enough to actually be read.

Checks (all fast, no network):
  * word caps on rolling docs and the queue files
  * queue schema (delegates to work_queue.py check)
  * every canonical skill in .agents/skills has a Claude wrapper in .claude/skills
  * every canonical subagent in .agents/agents has a Claude wrapper in .claude/agents
  * every Blocked queue item has a handoff doc
  * docs/README.md links resolve
  * licenses (ADR-0015): every dependency row in docs/LICENSING.md has an allowed SPDX id,
    and every third-party library the build uses has a row

Run:  python3 tools/validate_docs.py [--self-test]
"""
from __future__ import annotations

import json
import re
import subprocess
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

LICENSING = "docs/LICENSING.md"
# Found by CMake but never shipped with the game, so they need no LICENSING.md row.
NOT_REDISTRIBUTED = {"threads", "python3", "vulkan"}
CMAKE_DEPENDENCY = re.compile(
    r"\b(?:find_package|FetchContent_Declare)\(\s*([A-Za-z0-9_.+-]+)"
    r"|\bCPMAddPackage\(\s*(?:NAME\s+([A-Za-z0-9_.+-]+)|\"gh:[^/\"]+/([A-Za-z0-9_.+-]+))"
)


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


def allowed_licenses(licensing: str) -> set[str]:
    """The SPDX ids in LICENSING.md's allowed list, lowercased; 'public domain' if named."""
    section = licensing.split("## Allowed dependency licenses", 1)[-1].split("\n## ", 1)[0]
    listed = section.split("Anything else", 1)[0]
    allowed = {m.lower() for m in re.findall(r"`([^`]+)`", listed)}
    if "public domain" in listed.lower():
        allowed.add("public domain")
    return allowed


def dependency_rows(licensing: str) -> list[tuple[str, str]]:
    """(library, license cell) for each row of the table headed 'License (SPDX)'."""
    rows, in_table = [], False
    for line in licensing.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")] if line.startswith("|") else []
        if not cells:
            in_table = False
        elif any("License (SPDX)" in c for c in cells):
            in_table = True
        elif in_table and not set(cells[0]) <= set("-: "):
            rows.append((cells[0], cells[-1]))
    return rows


def license_ok(cell: str, allowed: set[str]) -> bool:
    """A cell passes when each '+' part has at least one allowed 'or' alternative: dual
    licenses are used under one, as zstd and PCG are. Parenthesised remarks are ignored."""
    cell = re.sub(r"\([^)]*\)", " ", cell)
    for part in re.split(r"\s\+\s|\band\b", cell):
        options = [o.strip().lower() for o in re.split(r"\bor\b|/", part) if o.strip()]
        if not any(o in allowed for o in options):
            return False
    return True


def cmake_files(root: Path) -> list[Path]:
    """CMake files of this checkout only (git knows them; nested worktrees are skipped)."""
    try:
        out = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", "*CMakeLists.txt", "*.cmake"],
            cwd=root, capture_output=True, text=True, check=True,
        ).stdout
        return [root / p for p in out.splitlines() if (root / p).is_file()]
    except (OSError, subprocess.CalledProcessError):
        return [p for p in root.rglob("*") if p.name == "CMakeLists.txt" or p.suffix == ".cmake"]


def used_dependencies(cmake_texts: dict[str, str], vendored: list[str]) -> dict[str, str]:
    """Third-party library name (lowercased) -> where the build pulls it in."""
    used: dict[str, str] = {}
    for where, text in cmake_texts.items():
        for m in CMAKE_DEPENDENCY.finditer(text):
            name = next(g for g in m.groups() if g)
            if name.lower() not in NOT_REDISTRIBUTED:
                used.setdefault(name.lower(), where)
    for name in vendored:
        used.setdefault(name.lower(), f"third_party/{name}")
    return used


def license_errors(licensing: str, cmake_texts: dict[str, str], vendored: list[str]) -> list[str]:
    allowed = allowed_licenses(licensing)
    rows = dependency_rows(licensing)
    errors = [
        f"{LICENSING}: {lib} is {cell}, not an allowed license (ADR-0015)"
        for lib, cell in rows
        if not license_ok(cell, allowed)
    ]
    recorded = {lib.lower() for lib, _ in rows}
    for name, where in sorted(used_dependencies(cmake_texts, vendored).items()):
        if name not in recorded:
            errors.append(f"{where}: the build uses {name}, which has no row in {LICENSING} (ADR-0015)")
    return errors


def repo_license_inputs(root: Path) -> tuple[str, dict[str, str], list[str]]:
    licensing = (root / LICENSING).read_text(encoding="utf-8")
    texts = {str(p.relative_to(root)): p.read_text(encoding="utf-8") for p in cmake_files(root)}
    third_party = root / "third_party"
    vendored = sorted(p.name for p in third_party.iterdir() if p.is_dir()) if third_party.is_dir() else []
    return licensing, texts, vendored


def license_self_test() -> list[str]:
    """The check must refuse a GPL row and an unlisted FetchContent name, and pass the repo."""
    licensing, texts, vendored = repo_license_inputs(ROOT)
    problems = []
    gpl = licensing.replace("| doctest | Tests | MIT |", "| doctest | Tests | MIT |\n| foo | test | GPL-3.0-only |")
    if not any("foo is GPL-3.0-only" in e for e in license_errors(gpl, texts, vendored)):
        problems.append("license self-test: a GPL-3.0-only row was not refused")
    fetched = dict(texts, **{"self-test.cmake": "FetchContent_Declare(bar GIT_REPOSITORY x)"})
    if not any("uses bar" in e for e in license_errors(licensing, fetched, vendored)):
        problems.append("license self-test: an unlisted FetchContent_Declare(bar) was not refused")
    return problems


def main() -> int:
    if "--self-test" in sys.argv[1:]:
        problems = license_self_test()
        for p in problems:
            print("DOC ERROR:", p)
        print("license self-test ok" if not problems else "license self-test failed")
        return 1 if problems else 0

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

    canonical = {p.stem for p in (ROOT / ".agents" / "agents").glob("*.md")}
    wrappers = {p.stem for p in (ROOT / ".claude" / "agents").glob("*.md")}
    for name in sorted(canonical - wrappers):
        errors.append(f"agent {name}: canonical .agents/agents/{name}.md has no .claude/agents wrapper")
    for name in sorted(wrappers - canonical):
        errors.append(f"agent {name}: .claude/agents wrapper has no canonical .agents/agents source")
    for p in (ROOT / ".agents" / "agents").glob("*.md"):
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

    errors += license_errors(*repo_license_inputs(ROOT))
    errors += license_self_test()

    if work_queue.cmd_check(None) != 0:
        errors.append("queue check failed (see above)")

    for e in errors:
        print("DOC ERROR:", e)
    print("docs ok" if not errors else f"{len(errors)} doc problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
