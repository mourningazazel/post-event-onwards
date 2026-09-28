#!/usr/bin/env python3
"""One-time (idempotent) setup for a fresh checkout. Safe to re-run.

  * installs the git hooks (core.hooksPath = .githooks)
  * reports which toolchain pieces are present
  * says which CMake preset fits this machine

Run:  python3 tools/bootstrap.py
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    subprocess.run(["git", "config", "core.hooksPath", ".githooks"], cwd=ROOT, check=True)
    print("git hooks: installed (.githooks)")

    tools = ["cmake", "ninja", "clang-format", "clang-tidy", "g++", "clang++", "cl"]
    for t in tools:
        print(f"{t:<13} {'ok' if shutil.which(t) else 'missing'}")

    cloud = bool(os.environ.get("CLAUDE_CODE_REMOTE"))
    role = "ARCHITECT (cloud)" if cloud else "BUILDER (local)"
    preset = "headless" if cloud else ("win-dev" if sys.platform == "win32" else "dev")
    print(f"\nrole: {role}")
    print(f"suggested preset: cmake --preset {preset} && cmake --build --preset {preset} && ctest --preset {preset}")
    print("verify everything: python3 tools/verify.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
