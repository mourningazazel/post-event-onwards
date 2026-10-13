#!/usr/bin/env python3
"""Conformance of the two .peo parsers (PEO-135): every tests/content/peo/<case>.peo must give
exactly the canonical lines in <case>.expect, records and errors alike.

    python3 tools/content/peo_conformance.py                    # the Python parser
    python3 tools/content/peo_conformance.py --engine PEO_DUMP  # the C++ one (peo_peo_dump)
    python3 tools/content/peo_conformance.py --write            # rewrite .expect from Python

Each case is parsed under its file name alone, so error paths read the same everywhere.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import peo  # noqa: E402

CASES = Path(__file__).resolve().parents[2] / "tests" / "content" / "peo"


def python_lines(case: Path) -> list[str]:
    return peo.print_lines(peo.parse_peo(case.name, case.read_text(encoding="utf-8")))


def engine_lines(engine: Path, case: Path) -> list[str]:
    run = subprocess.run([str(engine.resolve()), case.name], cwd=case.parent, capture_output=True, text=True)
    return run.stdout.splitlines()


def check(engine: Path | None) -> list[str]:
    """Every case whose lines differ from its .expect, with the first differing line."""
    failures = []
    for case in sorted(CASES.glob("*.peo")):
        expect = case.with_suffix(".expect")
        if not expect.exists():
            failures.append(f"{case.name}: no {expect.name}")
            continue
        want = expect.read_text(encoding="utf-8").splitlines()
        got = engine_lines(engine, case) if engine else python_lines(case)
        if got != want:
            first = next((i for i, (a, b) in enumerate(zip(got, want)) if a != b), min(len(got), len(want)))
            failures.append(f"{case.name}: line {first + 1}: got {got[first] if first < len(got) else '(none)'!r},"
                            f" want {want[first] if first < len(want) else '(none)'!r}")
    return failures


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", type=Path, help="peo_peo_dump, to check the C++ parser")
    ap.add_argument("--write", action="store_true", help="rewrite every .expect from the Python parser")
    args = ap.parse_args()
    cases = sorted(CASES.glob("*.peo"))
    if args.write:
        for case in cases:
            case.with_suffix(".expect").write_text("".join(f"{line}\n" for line in python_lines(case)),
                                                   encoding="utf-8")
        print(f"peo conformance: wrote {len(cases)} .expect files")
        return 0
    failures = check(args.engine)
    who = args.engine.name if args.engine else "peo.py"
    for f in failures:
        print(f"FAIL {f}")
    print(f"peo conformance ({who}): {len(cases)} cases, {len(failures)} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
