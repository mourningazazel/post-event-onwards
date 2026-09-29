# Testing

## Layers

| Layer | Who runs it | Command | Budget |
|-------|-------------|---------|--------|
| Docs & queue | both, CI | `python3 tools/validate_docs.py` | instant |
| Format | both, CI | `python3 tools/verify.py` (or `--fix`) | instant |
| Core unit tests | both, CI | `ctest --preset headless` | budget in AGENTS.md, enforced by the `suite_budget` test |
| Core perf | Builder (numbers), Architect (thresholds) | `ctest --preset headless-release` | per-brief |
| Frontend build | Builder, CI (Linux) | `cmake --build --preset dev` | n/a |
| Manual in-game | Builder only | brief's `manual` steps | n/a |

`python3 tools/verify.py` runs the first four in order and stops at the first
failure. It is the gate before every commit.

## Writing a test

- One `TEST_SUITE` per core header, in `tests/core/test_<name>.cpp`.
- Name cases as behaviours: `"walls block scent"`, not `"test3"`.
- Prefer exact expectations with `doctest::Approx` over "is not zero".
- A test that needs a display, a clock or the filesystem is not a unit test;
  it becomes a `manual` step in the brief.
- Keep the suite fast. If a case needs more than ~50 ms, tag it
  `* doctest::skip()` by default and add a `perf` queue item.

## Running a subset

```sh
./build/headless/tests/peo_core_tests -ts=scent          # one suite
./build/headless/tests/peo_core_tests -tc="*walls*"      # by case name
./build/headless/tests/peo_core_tests --list-test-cases
```

## Manual test reports

The Builder records results in the queue item's report note, step by step,
including anything unexpected. "Looked fine" is not a result.
