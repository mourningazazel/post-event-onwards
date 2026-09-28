#!/usr/bin/env python3
"""The single pre-commit / pre-review gate. Run before every commit and before
marking a queue item Validation or complete.

  python3 tools/verify.py            # docs + content lint/tests + format + headless build + tests
  python3 tools/verify.py --frontend # also build the SDL frontend (local machines)
  python3 tools/verify.py --fix      # apply clang-format instead of checking
  python3 tools/verify.py --release  # use the optimised headless preset (perf work)

Exit code is non-zero on the first failing stage so agents can act on it.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_GLOBS = ("src/**/*.cpp", "src/**/*.hpp", "tests/**/*.cpp", "tests/**/*.hpp")


def run(label: str, cmd: list[str], **kw) -> None:
    print(f"\n=== {label}: {' '.join(cmd)}")
    t = time.time()
    r = subprocess.run(cmd, cwd=ROOT, **kw)
    print(f"=== {label}: {'ok' if r.returncode == 0 else 'FAILED'} ({time.time() - t:.1f}s)")
    if r.returncode != 0:
        sys.exit(r.returncode)


def sources() -> list[str]:
    files: list[Path] = []
    for g in SOURCE_GLOBS:
        files.extend(ROOT.glob(g))
    return sorted(str(f.relative_to(ROOT)) for f in files)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--frontend", action="store_true")
    p.add_argument("--fix", action="store_true")
    p.add_argument("--release", action="store_true")
    p.add_argument("--skip-build", action="store_true")
    args = p.parse_args()

    run("docs", [sys.executable, "tools/validate_docs.py"])
    run("content-lint", [sys.executable, "tools/content/lint.py"])
    run("content-tests", [sys.executable, "tools/content/test.py"])

    if shutil.which("clang-format"):
        mode = ["-i"] if args.fix else ["--dry-run", "-Werror"]
        run("format", ["clang-format", *mode, *sources()])
    else:
        print("\n=== format: clang-format not installed, skipped")

    if args.skip_build:
        return 0

    preset = "headless-release" if args.release else "headless"
    if args.frontend:
        preset = "release" if args.release else ("win-dev" if sys.platform == "win32" else "dev")
    run("configure", ["cmake", "--preset", preset])
    run("build", ["cmake", "--build", "--preset", preset])
    run("test", ["ctest", "--preset", preset])
    print("\nall stages passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
