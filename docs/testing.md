# Testing

## Layers

| Layer | Who runs it | Command | Budget |
|-------|-------------|---------|--------|
| Docs & queue | both, CI | `python3 tools/validate_docs.py` | instant |
| Format | both, CI | `python3 tools/verify.py` (or `--fix`) | instant |
| Core unit tests | both, CI | `ctest --preset headless` | unit tier budget in AGENTS.md, enforced by `suite_budget` |
| Core scenario tests | both, CI | same run (suites `scenario*`) | scenario tier budget in AGENTS.md, enforced by `scenario_budget` |
| Core perf | Builder (numbers), Architect (thresholds) | `ctest --preset headless-release` | per-brief |
| Frontend build | Builder, CI (Linux) | `cmake --build --preset dev` | n/a |
| Manual in-game | Builder only | brief's `manual` steps | n/a |
| Siege suite (PEO-088) | on demand only: Builder, or the manual `siege` CI workflow | `peo_siege --check` (headless-release) | minutes; never in verify, ctest or regular CI |

**When to run the siege suite.** After a change to the Dead's movement, draw or knobs, the
scent field or wind, the desire field, or a new horde mechanic (PEO-010, 022, 038, 039, 051,
084): build `peo_siege` in `headless-release` and run `./build/headless-release/tests/peo_siege
--check`. If it differs, read the report, then `--write` the new baseline in the same commit
and say in the message why the numbers moved. Its fixture test (`siege layouts`) is the only
part in the regular suite. Nothing else needs it (Ryan, 2026-10-02).

`python3 tools/verify.py` runs the first four in order and stops at the first
failure. It is the gate before every commit. `--full` adds the two CI jobs a
Debug build cannot stand in for (`headless-release`, and clang with ASan/UBSan)
and runs before a push to main; a stage that cannot run locally is reported as
skipped, never as passed.

## Writing a test

- One `TEST_SUITE` per core header, in `tests/core/test_<name>.cpp`.
- Name cases as behaviours: `"walls block scent"`, not `"test3"`.
- Prefer exact expectations with `doctest::Approx` over "is not zero".
- A test that needs a display, a clock or the filesystem is not a unit test;
  it becomes a `manual` step in the brief.
- Keep the unit tier fast (D-033). A case that needs more than ~50 ms under
  the sanitizers (a town fixture, a scenario, a golden run over hundreds of
  updates) goes in the scenario tier: put it in a `TEST_SUITE("scenario: <name>")`,
  or tag the case `* doctest::test_suite("scenario: <file>")`. Never skip it.

## Running a subset

```sh
./build/headless/tests/peo_core_tests -tse='scenario*'   # the fast unit tier
./build/headless/tests/peo_core_tests -ts='scenario*'    # the scenario tier
./build/headless/tests/peo_core_tests -ts=scent          # one suite
./build/headless/tests/peo_core_tests -tc="*walls*"      # by case name
./build/headless/tests/peo_core_tests --list-test-cases
```

## Manual test reports

The Builder records results in the queue item's report note, step by step,
including anything unexpected. "Looked fine" is not a result.
