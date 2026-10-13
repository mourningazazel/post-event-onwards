# Build, run and test

## Toolchain

- CMake 3.x with presets, Ninja, a C++20 compiler. CI uses g++-13 and clang-18; the Builder
  runs on an M1 under Asahi Linux. Warnings are errors (`PEO_WARNINGS_AS_ERRORS`).
- SDL3 for the frontend: the system package if found, else fetched and built from source
  (`PEO_FETCH_SDL`, `cmake/FindOrFetchSDL3.cmake`). SDL 3.4+ is needed for `--tileset`.
- Optional: `glslc` or `glslangValidator` to build `peo_gpu` (`PEO_BUILD_GPU`); without one
  the game simply runs scent on the CPU.
- Python 3 for every tool under `tools/`, and `clang-format` 18 for the format check.

First thing in a fresh checkout: `python3 tools/bootstrap.py`. It installs the git hooks
(`core.hooksPath=.githooks`), prints which role this session is, and the suggested preset.

## Presets (`CMakePresets.json`)

| Preset | Type | Frontend | Notes |
|---|---|---|---|
| `dev` | Debug | yes | ASan and UBSan on. The everyday build for playing and coding |
| `release` | Release | yes | Playing for feel and frame time |
| `headless` | Debug | no | ASan/UBSan. What CI's main test job and `verify.py` build |
| `headless-release` | Release | no | Perf numbers, the siege suite, release-only tests |
| `headless-tsan` | Debug | no | ThreadSanitizer, for the threaded tests |
| `win-dev` | | | Windows developer build |

```sh
cmake --preset dev && cmake --build --preset dev && ./build/dev/bin/peo
cmake --preset headless && cmake --build --preset headless && ctest --preset headless
```

## Running the game

`./build/dev/bin/peo [SEED] [flags]` (or `build/release/bin/peo`). The flags and keys are
listed on [Frontend](frontend.md#command-line).

## Tests

All tests are headless doctest suites linked into one binary,
`build/<preset>/tests/peo_core_tests`, plus CTest checks beside it (`tests/CMakeLists.txt`): two budgets and the content golden.

- **Unit tier**: everything not in a `scenario*` suite. Must sum under 1 s (D-033).
- **Scenario tier**: suites named `scenario: ...` (golden runs, town fixtures). Under 5 s.
- `suite_budget` and `scenario_budget` (`tools/suite_budget.py`) fail the run when a tier's
  summed case times exceed the numbers written in `AGENTS.md`, naming the slowest cases.
- `content_golden` (`tools/content/golden.py --engine peo_content_dump`) fails when any item in
  `content/` resolves differently in the engine than in `derive.py`, printing the first 20
  differing lines (item, dotted path, value).
- Cases that build a whole 100 km geography square run only in release builds
  (`#ifdef NDEBUG`).

```sh
./build/headless/tests/peo_core_tests -tse='scenario*'   # unit tier only
./build/headless/tests/peo_core_tests -ts='scenario*'    # scenario tier only
./build/headless/tests/peo_core_tests -ts=scent_wave     # one suite
./build/headless/tests/peo_core_tests -tc="*walls*"      # by case name
./build/headless/tests/peo_core_tests --list-test-cases
```

The tests that guard the big promises (`docs/architecture.md`, "Boundaries that tests
protect"): `world: commit(speculate()) is bit-identical to step()`, `scent_wave:
patch_deposit after update equals deposit then update`, `save: a restored world continues
bit-identical to the one never saved`, `revisit: one long absence equals several short
ones`, the serial-versus-threaded golden runs in `test_executor.cpp`, and the swarm
scenarios in `test_swarm.cpp`. If one of those goes red, something fundamental broke.

### On-demand suites

- **Siege suite** (`tests/siege/siege.cpp`, PEO-088). The horde round each of three
  buildings, three opening setups, two densities, 48 game hours while the player waits;
  writes a report and maps. Run it after any change to the Dead, scent, wind or the desire
  field: `./build/headless-release/tests/peo_siege --check`. If the numbers move on purpose,
  `--write` the new baseline (`tests/siege/baseline/<B>/`) in the same commit and say why.
  Never part of `verify.py` or regular CI; the `siege` workflow runs it by hand.
- **Geography images** (`tests/geo/geo_dump.cpp`): `peo_geo --seed N --square X,Y
  [--span K] [--scale S] [--grid] --out f.ppm`. The headless way to look at the map.
- **Resolved content** (`tests/content_dump/content_dump.cpp`): `peo_content_dump <content_dir>`
  prints every resolved item as `<id>\t<dotted path>\t<value>` lines; `golden.py` prints the
  same from Python.

## `verify.py`: the one gate

`python3 tools/verify.py` runs, stopping at the first failure: docs and queue validation,
content lint and tests, clang-format, the headless build, the tests. Flags: `--frontend`
also builds the SDL app; `--full` adds `headless-release` and a clang ASan/UBSan build (run
before pushing to `main`); `--fix` applies clang-format; `--perf` prints the perf lines;
`--skip-build` for docs-only changes. Output tails only; the whole log is `build/verify.log`.

## CI (`.github/workflows/`)

| Workflow | Job | Runs |
|---|---|---|
| `ci.yml` | clang-format | the format check |
| | core tests (g++-13, clang-18) | `headless` build and `ctest` |
| | core threads (tsan) | the executor suites under ThreadSanitizer |
| | gpu field (lavapipe) | `peo_gpu_tests` on a software Vulkan device |
| | core tests (release) | `headless-release` and `ctest` (release-only cases) |
| | frontend build | the SDL3 app on Linux, SDL from source |
| `docs.yml` | docs, queue & content | `validate_docs.py`, queue check, content lint and tests |
| `siege.yml` | siege suite | manual trigger only, one job per building |

## Git hooks

`tools/git_safety.py`, installed by bootstrap. **pre-commit** blocks secrets, files over
5 MB, build output, and any staged doc or queue change that fails `validate_docs.py` (word
caps, queue schema). **pre-push** blocks force-pushes and deleting `main`. The repo's rule is
never to bypass them with `--no-verify`.
