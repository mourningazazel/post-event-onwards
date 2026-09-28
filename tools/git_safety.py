#!/usr/bin/env python3
"""Git hook body. Installed by tools/bootstrap.py (core.hooksPath=.githooks).

pre-commit: block secrets, huge blobs, build output, and any staged doc or
            queue change that fails tools/validate_docs.py (word caps, schema).
pre-push:   block force-pushes and deletions of main.

Never bypass with --no-verify; fix the cause or raise it with the user.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAX_BLOB_BYTES = 5 * 1024 * 1024
PROTECTED = ("refs/heads/main",)
ZERO = "0" * 40

SECRET_PATTERNS = [
    (re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----"), "private key"),
    (re.compile(r"\bAKIA[0-9A-Z]{16}\b"), "AWS access key"),
    (re.compile(r"\bghp_[A-Za-z0-9]{36}\b"), "GitHub token"),
    (re.compile(r"\bsk-ant-[A-Za-z0-9_-]{20,}"), "Anthropic API key"),
    (re.compile(r"(?i)\b(?:api[_-]?key|secret|password|token)\s*[:=]\s*['\"][^'\"\s]{12,}['\"]"), "hard-coded credential"),
]
FORBIDDEN_PATHS = ("build/", "out/", "cmake-build-", "_deps/", ".claude/settings.local.json", "CMakeUserPresets.json")


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout


def pre_commit() -> int:
    problems: list[str] = []
    staged = [l for l in git("diff", "--cached", "--name-only", "--diff-filter=ACMR").splitlines() if l]
    for rel in staged:
        if any(rel.startswith(f) or f"/{f}" in rel for f in FORBIDDEN_PATHS):
            problems.append(f"{rel}: build output / local config must not be committed")
            continue
        blob = subprocess.run(["git", "show", f":{rel}"], cwd=ROOT, capture_output=True).stdout
        if len(blob) > MAX_BLOB_BYTES:
            problems.append(f"{rel}: {len(blob) // 1024} KiB exceeds the {MAX_BLOB_BYTES // 1024 // 1024} MiB limit")
            continue
        try:
            text = blob.decode("utf-8")
        except UnicodeDecodeError:
            continue
        for pattern, label in SECRET_PATTERNS:
            if pattern.search(text):
                problems.append(f"{rel}: looks like it contains a {label}")
    doc_like = (".md", ".json")
    if any(rel.endswith(doc_like) for rel in staged):
        r = subprocess.run([sys.executable, "tools/validate_docs.py"], cwd=ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            problems.append("docs/queue validation failed:\n" + r.stdout)
    for p in problems:
        print("pre-commit:", p, file=sys.stderr)
    return 1 if problems else 0


def pre_push() -> int:
    problems: list[str] = []
    for line in sys.stdin.read().splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        local_ref, local_sha, remote_ref, remote_sha = parts
        if remote_ref not in PROTECTED:
            continue
        if local_sha == ZERO:
            problems.append(f"refusing to delete {remote_ref}")
            continue
        if remote_sha == ZERO:
            continue
        r = subprocess.run(["git", "merge-base", "--is-ancestor", remote_sha, local_sha], cwd=ROOT)
        if r.returncode != 0:
            problems.append(f"refusing non-fast-forward push to {remote_ref}; fetch, merge and push again")
    for p in problems:
        print("pre-push:", p, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    hook = sys.argv[1] if len(sys.argv) > 1 else ""
    sys.exit({"pre-commit": pre_commit, "pre-push": pre_push}.get(hook, lambda: 0)())
