---
name: plan
description: Architect: turn a Pending item into a full brief with units, acceptance criteria, tests and manual steps.
---
# /plan (Architect only)

Turn an item into something the Builder can finish without asking.

A batch is one thread, one item per subagent (D-044): the thread checks out
`origin/main` on its branch and starts one general-purpose subagent per
item, in order, with: "Architect. Read `.agents/skills/plan/SKILL.md` and
follow it for `<id>` only; commit; finish with one line: id, briefed or the
D-xxx it raised, commit." It keeps those lines, pushes once (step 7) and
replies once. When handed one item, do that item only.

1. `python3 tools/work_queue.py show <id>`; read `docs/production/vision.md`,
   the item's domain page (`docs/domains/`: in its references, or the page
   that lists its id) and the
   `docs/architecture.md` section for the system involved. Load only what the
   domain page lists for this item; other domains only where the work crosses.
2. Decide the seam. If the item needs a new public interface, write the
   header in `src/core/include/peo/core/` with doc comments and a doctest in
   `tests/core/` that pins the behaviour. Leave the implementation as a
   stub only if it keeps the build green; otherwise leave the test red and
   say so in the brief.
   A brief that changes horde or scent mechanics (the Dead's movement, draw or
   knobs, the scent field or wind, the desire field, a new horde mechanic) lists
   `peo_siege --check` under Tests (docs/testing.md, PEO-088).
3. Write the brief JSON (fields: goal, context, units, acceptance, tests,
   manual, out_of_scope; see `docs/roles.md#handoff-formats`) and apply it:
   `python3 tools/work_queue.py brief <id> --file <path>`.
   - Context: the domain page first, then only the docs this item needs.
     Code by symbol, section or line range (`world.cpp: advance,
     dead_second`), never a whole file over about 300 lines; the Builder
     reads only what is named.
   - Units: ordered, commit-sized, each named with its layer: `core:`,
     `test:`, `app:`, `docs:`.
   - Acceptance: observable. "`ctest` passes", "in game, the Dead stop at
     the wall".
   - Manual: what the Builder should do and what to look for, including
     one perf observation when the change touches a per-tick loop.
   - Per-update, per-second or per-Dead work: name the
     `docs/bottlenecks.md` rows it touches and its expected cost, and make
     before-and-after `/perf` numbers for them an acceptance line.
4. Split anything over effort L into several items with `depends_on`.
   If briefing needs a gameplay, abstraction or direction choice, or the
   right design is a big rewrite: add a `D-xxx` entry to
   `docs/production/DECISIONS_NEEDED.md` (fixed A/B/C format, recommend
   one), set the item `AwaitingUser --note "D-xxx"`, and brief a different
   item. Never brief the recommended option on speculation.
5. `set <id> --owner builder --note "briefed" --by architect`.
6. If it needs a decision, add it to `docs/production/decisions.md` in the
   same commit.
7. `python3 tools/verify.py` (`--skip-build` when no code changed), then
   commit the queue file, any headers, tests and docs: `[PEO-xxx] brief:
   <title>`. The batch thread pushes fast-forward to `main` (`git push
   origin HEAD:main`; on rejection fetch, rebase its own commits, re-run the
   tool on a queue conflict, push again). A batch that wrote headers or
   tests goes through a pull request instead.
