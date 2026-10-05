#!/usr/bin/env python3
"""The single pre-commit / pre-review gate. Run before every commit and before
marking a queue item Validation or complete; add --full before a push to main.

  python3 tools/verify.py            # docs + content lint/tests + format + headless build + tests
  python3 tools/verify.py --frontend # also build the SDL frontend (local machines)
  python3 tools/verify.py --fix      # apply clang-format instead of checking
  python3 tools/verify.py --release  # use the optimised headless preset (perf work)
  python3 tools/verify.py --full     # also the CI jobs a Debug run cannot represent:
                                     # headless-release and a clang (ASan/UBSan) build
  python3 tools/verify.py --perf     # also print the perf lines from headless-release

Exit code is non-zero on the first failing stage so agents can act on it. A
stage that cannot run here (no clang-format, no clang sanitizer runtime) is
reported as skipped, and the summary then says so instead of "all stages passed".
Each stage prints only its last lines (3 on success, 60 on failure); the whole
output is in build/verify.log for the rare case the tail is not enough.
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_GLOBS = ("src/**/*.cpp", "src/**/*.hpp", "tests/**/*.cpp", "tests/**/*.hpp")

# The clang job in .github/workflows/ci.yml. Preferred when installed; another
# version still runs, with a warning, since it catches most of the same things.
CI_CLANG_MAJOR = 18
CLANG_CANDIDATES = (f"clang++-{CI_CLANG_MAJOR}", "clang++")
CLANG_BUILD_DIR = "build/headless-clang"
SANITIZE = "-fsanitize=address,undefined"
# Diagnostics newer clang raises inside doctest's macros and libstdc++'s
# <ciso646> that CI's clang 18 does not know. Demoted to plain warnings (still
# printed) only in this local mirror and only when the compiler accepts the flag;
# CMake and CI keep every warning an error. -Wno-error= because the target's
# later -Wpedantic re-enables a plain -Wno-.
NEWER_CLANG_ONLY = ("-Wno-error=c2y-extensions", "-Wno-error=#warnings")

# Every stage's full output; the terminal only gets a tail (see run()).
LOG = ROOT / "build" / "verify.log"
TAIL_OK = 3
TAIL_FAIL = 60

PERF_TESTS = "build/headless-release/tests"
PERF_FILTER = "-tc=perf*"
PERF_LINE = re.compile(r"MESSAGE: (perf .*)$")

skipped: list[str] = []


def run(label: str, cmd: list[str], **kw) -> None:
    """Run one stage. Its full output goes to LOG; the terminal gets the last
    TAIL_OK lines on success and the last TAIL_FAIL on failure, so an agent's
    session is not filled with a compiler's or a test's whole output."""
    shown = " ".join(cmd)
    if len(shown) > 160:  # clang-format lists every source; the log keeps the whole line
        shown = f"{shown[:160]}... ({len(cmd)} args)"
    print(f"\n=== {label}: {shown}")
    t = time.time()
    r = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with LOG.open("a", encoding="utf-8") as f:
        f.write(f"=== {label}: {' '.join(cmd)}\n{r.stdout}")
    lines = r.stdout.splitlines()
    ok = r.returncode == 0
    tail = lines[-(TAIL_OK if ok else TAIL_FAIL):]
    if len(lines) > len(tail):
        print(f"... ({len(lines) - len(tail)} earlier lines in {LOG.relative_to(ROOT)})")
    print("\n".join(tail))
    print(f"=== {label}: {'ok' if ok else 'FAILED'} ({time.time() - t:.1f}s)")
    if not ok:
        sys.exit(r.returncode)


def skip(label: str, why: str) -> None:
    print(f"\n=== {label}: SKIPPED, {why}")
    skipped.append(f"{label}: {why}")


def sources() -> list[str]:
    files: list[Path] = []
    for g in SOURCE_GLOBS:
        files.extend(ROOT.glob(g))
    return sorted(str(f.relative_to(ROOT)) for f in files)


def compiles(cxx: str, flags: list[str]) -> bool:
    """True when cxx compiles and links an empty program with flags, warnings as errors."""
    with tempfile.TemporaryDirectory() as d:
        src = Path(d) / "probe.cpp"
        src.write_text("int main() { return 0; }\n")
        r = subprocess.run([cxx, "-Werror", *flags, str(src), "-o", str(Path(d) / "probe")], capture_output=True)
        return r.returncode == 0


def clang_major(cxx: str) -> int | None:
    out = subprocess.run([cxx, "--version"], capture_output=True, text=True).stdout
    m = re.search(r"clang version (\d+)", out)
    return int(m.group(1)) if m else None


def clang_stage() -> None:
    cxx = next((c for c in CLANG_CANDIDATES if shutil.which(c)), None)
    if cxx is None:
        skip("clang", f"no {' or '.join(CLANG_CANDIDATES)} on PATH")
        return
    if not compiles(cxx, [SANITIZE]):
        skip("clang", f"{cxx} cannot link {SANITIZE} (install the clang runtime, e.g. libclang-rt-{CI_CLANG_MAJOR}-dev)")
        return
    major = clang_major(cxx)
    if major != CI_CLANG_MAJOR:
        print(f"\n=== clang: note: {cxx} is clang {major}, CI runs clang {CI_CLANG_MAJOR}; results can differ")
    extra = [f for f in NEWER_CLANG_ONLY if compiles(cxx, [f])]
    run(
        "clang configure",
        ["cmake", "--preset", "headless", "-B", CLANG_BUILD_DIR, f"-DCMAKE_CXX_COMPILER={cxx}",
         f"-DCMAKE_CXX_FLAGS={' '.join(extra)}"],
    )
    run("clang build", ["cmake", "--build", CLANG_BUILD_DIR])
    run("clang test", ["ctest", "--test-dir", CLANG_BUILD_DIR, "--output-on-failure"])


def preset_stage(label: str, preset: str) -> None:
    run(f"{label} configure", ["cmake", "--preset", preset])
    run(f"{label} build", ["cmake", "--build", "--preset", preset])
    run(f"{label} test", ["ctest", "--preset", preset])


def perf_stage() -> None:
    """Run only the perf cases from the release build and print their lines to paste."""
    binary = next(iter(sorted((ROOT / PERF_TESTS).glob("peo_core_tests*"))), None)
    if binary is None:
        skip("perf", f"no test binary in {PERF_TESTS}")
        return
    print(f"\n=== perf: {binary.relative_to(ROOT)} {PERF_FILTER}")
    r = subprocess.run([str(binary), PERF_FILTER], cwd=ROOT, capture_output=True, text=True)
    lines = [m.group(1) for m in map(PERF_LINE.search, r.stdout.splitlines()) if m]
    print("\n".join(lines))
    if r.returncode != 0 or not lines:
        print(r.stdout[-2000:])
        print(f"=== perf: FAILED")
        sys.exit(r.returncode or 1)
    print(f"=== perf: ok ({len(lines)} variants)")


def summary() -> int:
    if skipped:
        print("\npassed, but these stages did NOT run:\n  " + "\n  ".join(skipped))
    else:
        print("\nall stages passed")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--frontend", action="store_true")
    p.add_argument("--fix", action="store_true")
    p.add_argument("--release", action="store_true")
    p.add_argument("--full", action="store_true")
    p.add_argument("--perf", action="store_true")
    p.add_argument("--skip-build", action="store_true")
    args = p.parse_args()

    LOG.parent.mkdir(parents=True, exist_ok=True)
    LOG.write_text("", encoding="utf-8")
    run("docs", [sys.executable, "tools/validate_docs.py"])
    run("content-lint", [sys.executable, "tools/content/lint.py"])
    run("content-tests", [sys.executable, "tools/content/test.py"])

    if shutil.which("clang-format"):
        mode = ["-i"] if args.fix else ["--dry-run", "-Werror"]
        run("format", ["clang-format", *mode, *sources()])
    else:
        skip("format", "clang-format not installed")

    if args.skip_build:
        return summary()

    preset = "headless-release" if args.release else "headless"
    if args.frontend:
        preset = "release" if args.release else ("win-dev" if sys.platform == "win32" else "dev")
    run("configure", ["cmake", "--preset", preset])
    run("build", ["cmake", "--build", "--preset", preset])
    run("test", ["ctest", "--preset", preset])

    release_built = preset == "headless-release"
    if (args.full or args.perf) and not release_built:
        preset_stage("release", "headless-release")
    if args.full:
        if os.name == "nt":
            skip("clang", "the clang job is Linux-only in CI")
        else:
            clang_stage()
    if args.perf:
        perf_stage()
    return summary()


if __name__ == "__main__":
    sys.exit(main())
