# 14 · Taking over

You already hold the owner's role: every rule in the repo defers to you, and you are the only
one who may relax one. "Taking over" therefore means choosing how much of the building you do
yourself. Three ways, from least to most change.

## Option A: keep both agents, steer harder

Nothing changes in the repo. You keep asking the Architect (cloud threads) for briefs and
reviews, and run the Builder locally with `/start-work` or `/burndown`. What reading this wiki
buys you is better decisions: you can read a brief and see whether it is right, read a diff and
see whether it fits, and answer D-entries knowing what each option costs in code.

Best if your time is short and you mostly want to direct.

## Option B: you become the Builder

You write the code; the Architect still briefs and reviews. Follow the Builder's loop by hand
(`.agents/skills/start-work/SKILL.md`):

1. `git fetch origin main && git merge --ff-only origin/main`
2. `python3 tools/work_queue.py next --owner builder`, then `show <id>`
3. `set <id> --status InProgress --by builder`, commit `[PEO-xxx] start`
4. Per unit: code in `src/core` first, tests in `tests/core`, `python3 tools/verify.py
   --frontend --full`, commit explicit paths `[PEO-xxx] <summary>`, push to `main`
5. Run the brief's `manual` steps in the game
6. `note <id> --by builder --text "commits: ... tests: ... manual: ... perf: ... open: ..."`,
   then `set <id> --status Validation --owner architect`
7. Ask for a review

You can still hand a unit to the local Builder whenever you like; the queue does not care who
typed the code.

## Option C: you replace the Architect too

You plan, build and review. The ceremony (briefs as files, word caps, review notes) exists so
that sessions without memory can hand work to each other; a human with a memory can drop much
of it. What to **keep**, because the code depends on it, not the process:

- `python3 tools/verify.py` as the gate, and CI on every push.
- The determinism tests (`commit(speculate()) is bit-identical to step()`, the save round
  trip, the threaded golden runs). They are what lets speculation, saves, threads and the GPU
  stay correct.
- The siege baseline and swarm scenarios for anything touching the Dead or scent.
- The core rules on [Architecture](03-architecture.md): no SDL in core, integers, seeded
  hashes, no per-tick allocation.

What you **can relax** (say so in `docs/production/decisions.md`, so any agent you bring back
later knows): writing briefs for your own items, the PR-for-code rule, the word caps (raise
them in `tools/validate_docs.py`), the queue (use GitHub issues if you prefer, but then
retire `WORK_QUEUE.json` and its skills together, or agents will keep reading a stale list).

If you later bring the agents back, `AGENTS.md`, `CLAUDE.md` and `docs/roles.md` are what they
obey: update those three first.

## A first week

1. Build and play: `cmake --preset dev && cmake --build --preset dev && ./build/dev/bin/peo`.
   Toggle the scent overlay (Shift+S), walk a trail, wait, watch the Dead climb it. Try
   `--stage 512x512` and look at `spec:` in the HUD.
2. Run the tests: `cmake --preset headless && cmake --build --preset headless && ctest
   --preset headless`. Then run one suite with `-ts=scent_wave` and read its cases: the test
   names are a readable spec.
3. Read `world.hpp`, then `world.cpp` from `advance` (`:498`) outward. Everything in pages 4
   to 6 is in roughly 400 lines there plus `desire.cpp:106`.
4. Make a lever change: set `lean_full` to 32 in `desire.hpp`, rebuild, run
   `-ts="scenario: swarm"` and watch the printed shares move; build `headless-release` and
   run `peo_siege --check` to see how the siege report reacts. Revert.
5. Make a small feature end to end: for example a new `ActionKind` (a "listen" wait that
   costs 24 substeps) in core with a doctest, then a key in `src/app/main.cpp`.
6. Pick a real queue item: PEO-121 (what can be made out at range) and PEO-092 (content
   migration on load) are the two briefed for the Builder today.

## The state you inherit (2026-10-12)

- `main` at `fddde4c`; the last item done was PEO-127 (the look-ahead strip).
- Queue: 56 items, all `Pending`. Two are briefed for the Builder (PEO-092, PEO-121), one waits
  on you (PEO-072: run `peo_bench` on a real PC per tier, after PEO-070), the other 53 need an
  Architect brief first. No open decisions; one deferred item (PEO-098).
- The highest-severity item is PEO-122 (S1): the g++-13 unit test tier sat at 1,024 to
  1,063 ms against its 1 s budget, making CI flaky. PEO-126 moved the slow cases and CI went
  green on PR #61; the item is open to check whether anything is left.
- Next big design steps by severity S2: the content loader (PEO-008), skills (PEO-012),
  quality (PEO-013), triggers and sound (PEO-010), the contents generator (PEO-053), the
  settlement layout generator (PEO-114), one continuous map (PEO-118). See
  [Roadmap](15-roadmap.md).

## Known drift in the existing docs

Found while writing this wiki; the code is right in each case.

- `docs/architecture.md`, "The Dead": says a unit `decide_move`s to "its strongest
  neighbour". Since PEO-009 (D-038 B) it is the weighted draw over the desire field
  ([The Dead](06-the-dead.md)); the strongest-neighbour bytes now have no reader (PEO-100).
  The code tree at the top of that page also omits `desire.hpp` and several later headers.
- `docs/architecture.md`, "Turn model": lists contests, trample and sound events in the turn
  order. None is built.
- `docs/architecture.md`, "Boundaries that tests protect": names `scent: mass is conserved`
  and `walls absorb scent`, which belonged to the diffusion field removed in PEO-076; no test
  by those names exists now.
- `docs/bottlenecks.md`: its `world.cpp` line numbers are stale (`dead_slot` is now
  `world.cpp:433`, `poll` `:62`, `speculate_dead` `:575`, `commit` `:608`).

The `docs-drift` subagent exists for exactly this; worth one pass.
