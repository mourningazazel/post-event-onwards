#!/usr/bin/env python3
"""Turn docs/wiki into the repository's GitHub wiki.

docs/wiki stays the source: pages change with the code in the same commit and are
reviewed with it. This tool renders that folder in GitHub wiki form, and the `wiki`
workflow pushes the result to the Wiki tab on every push to main.

  * README.md becomes Home; every other page is named after its first `# ` heading
    ("The Dead" -> The-Dead.md), and that heading is dropped since the wiki shows the name
  * links between pages (`scent.md#wind`) become wiki links (`Scent#wind`)
  * _Sidebar.md is built from Home's "## Pages" section; _Footer.md says where to edit

Run:  python3 tools/wiki_publish.py OUT_DIR   # render into OUT_DIR
      python3 tools/wiki_publish.py --check   # render to a temp dir; fail on a broken link
"""
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "wiki"
HOME = "README.md"
SIDEBAR_SECTION = "## Pages"
FOOTER = (
    "Published from [`docs/wiki/`](https://github.com/mourningazazel/post-event-onwards/tree/main/docs/wiki)"
    " on `main`. Edit the page there: changes made on the Wiki tab are overwritten.\n"
)

LINK = re.compile(r"\]\(([^)\s]+)\)")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*$")


def slug(heading: str) -> str:
    """GitHub's heading anchor: lowercase, punctuation dropped, spaces to hyphens."""
    text = re.sub(r"[`*_]", "", heading).strip().lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def page_names(source: Path) -> dict[str, str]:
    names = {}
    for path in sorted(source.glob("*.md")):
        if path.name == HOME:
            names[path.name] = "Home"
            continue
        first = path.read_text(encoding="utf-8").splitlines()[0]
        match = HEADING.match(first)
        if not match or match.group(1) != "#":
            raise SystemExit(f"{path}: the first line must be the page's `# ` title")
        names[path.name] = re.sub(r"[^\w\-]+", "-", match.group(2)).strip("-")
    return names


def convert(text: str, names: dict[str, str], where: str, errors: list[str]) -> str:
    def repl(match: re.Match[str]) -> str:
        target = match.group(1)
        if re.match(r"^[a-z]+:", target) or target.startswith("#"):
            return match.group(0)
        file, _, anchor = target.partition("#")
        if file not in names:
            errors.append(f"{where}: link to {target} is not a wiki page")
            return match.group(0)
        return f"]({names[file]}{'#' + anchor if anchor else ''})"

    return LINK.sub(repl, text)


def render(source: Path, out: Path) -> list[str]:
    names = page_names(source)
    errors: list[str] = []
    anchors: dict[str, set[str]] = {}
    out.mkdir(parents=True, exist_ok=True)
    for file, name in names.items():
        lines = (source / file).read_text(encoding="utf-8").splitlines(keepends=True)
        if name != "Home":
            lines = lines[1:]
            while lines and not lines[0].strip():
                lines = lines[1:]
        body = convert("".join(lines), names, file, errors)
        anchors[name] = {slug(m.group(2)) for m in map(HEADING.match, body.splitlines()) if m}
        (out / f"{name}.md").write_text(body, encoding="utf-8")
        if name == "Home":
            section = body.partition(SIDEBAR_SECTION + "\n")[2].split("\n## ")[0]
            (out / "_Sidebar.md").write_text("**[Home](Home)**\n" + section.rstrip() + "\n")
    (out / "_Footer.md").write_text(FOOTER, encoding="utf-8")
    for page in out.glob("*.md"):
        for target in LINK.findall(page.read_text(encoding="utf-8")):
            name, _, anchor = target.partition("#")
            name = name or page.stem
            if name in anchors and anchor and anchor not in anchors[name]:
                errors.append(f"{page.name}: anchor #{anchor} not found on {name}")
    return errors


def main(argv: list[str]) -> int:
    if argv == ["--check"]:
        with tempfile.TemporaryDirectory() as tmp:
            errors = render(SOURCE, Path(tmp))
    elif len(argv) == 1 and not argv[0].startswith("-"):
        errors = render(SOURCE, Path(argv[0]))
    else:
        print(__doc__)
        return 2
    for error in errors:
        print(f"wiki: {error}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
