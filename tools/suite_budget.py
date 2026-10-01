#!/usr/bin/env python3
"""Fail when a tier of the headless test suite exceeds its time budget (PEO-033, D-033).

Runs the doctest binary with per-case durations for one tier, sums them, and compares the
total with that tier's budget stated in AGENTS.md, the single source of both numbers:
"the unit suite runs in under 1 s" and "scenario tests run in under 5 s" (or "N ms").
The scenario tier is every doctest suite named scenario*; the unit tier is everything else.
Over budget, it names the slowest cases so the next person knows where the time went.

usage: suite_budget.py <peo_core_tests> [--tier unit|scenario] [--agents AGENTS.md]
Registered with CTest as `suite_budget` (unit) and `scenario_budget` (tests/CMakeLists.txt).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCENARIO_SUITES = "scenario*"
BUDGET_PATTERNS = {
    "unit": re.compile(r"unit\s+suite\s+runs\s+in\s+under\s+(\d+(?:\.\d+)?) ?(ms|s)\b"),
    "scenario": re.compile(r"scenario\s+tests\s+run\s+in\s+under\s+(\d+(?:\.\d+)?) ?(ms|s)\b"),
}
TIER_FILTER = {"unit": f"--test-suite-exclude={SCENARIO_SUITES}", "scenario": f"--test-suite={SCENARIO_SUITES}"}
MS_PER = {"ms": 1.0, "s": 1000.0}
DURATION_LINE = re.compile(r"^(\d+(?:\.\d+)?) s: (.+)$")
SLOWEST_SHOWN = 5


def budget_ms(agents: Path, tier: str) -> float:
    pattern = BUDGET_PATTERNS[tier]
    match = pattern.search(agents.read_text(encoding="utf-8"))
    if not match:
        sys.exit(f"suite_budget: no '{pattern.pattern}' in {agents}; the budget must be stated there")
    return float(match.group(1)) * MS_PER[match.group(2)]


def case_durations(binary: str, tier: str) -> tuple[list[tuple[float, str]], int]:
    proc = subprocess.run([binary, "--no-intro", "--duration=true", TIER_FILTER[tier]], capture_output=True, text=True)
    cases = []
    for line in proc.stdout.splitlines():
        m = DURATION_LINE.match(line.strip())
        if m:
            cases.append((float(m.group(1)) * 1000.0, m.group(2)))
    return cases, proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("binary")
    parser.add_argument("--tier", choices=sorted(BUDGET_PATTERNS), default="unit")
    parser.add_argument("--agents", type=Path, default=ROOT / "AGENTS.md")
    args = parser.parse_args()

    limit = budget_ms(args.agents, args.tier)
    cases, code = case_durations(args.binary, args.tier)
    if not cases:
        print(f"suite_budget: no per-case durations from {args.binary} (exit {code})")
        return 1
    total = sum(ms for ms, _ in cases)
    slowest = sorted(cases, reverse=True)[:SLOWEST_SHOWN]
    over = total > limit
    verdict = "OVER BUDGET" if over else "within budget"
    print(f"suite_budget ({args.tier}): {len(cases)} cases, {total:.1f} ms of {limit:.0f} ms ({verdict}; budget from AGENTS.md)")
    print("slowest:")
    for ms, name in slowest:
        print(f"  {ms:8.1f} ms  {name}")
    if code != 0:
        print(f"note: the suite itself failed (exit {code}); see the 'core' test")
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main())
