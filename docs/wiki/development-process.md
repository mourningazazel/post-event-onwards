# Development process

The code is written by two Claude Code sessions with different jobs, coordinated only through
files in the repo, so either can lose its memory and carry on. The owner ("the user" in every
rule) sets intent, answers decisions, promotes deferred work, and is the only one who may relax
a rule. The contract is `docs/roles.md`; the shared policy
is `AGENTS.md`; Claude-specific deltas are `CLAUDE.md`. All three are short and worth reading
whole.

## The two roles

| | **Architect** | **Builder** |
|---|---|---|
| Runs | Claude Code on the web (`CLAUDE_CODE_REMOTE` set), on a `claude/*` branch | Claude Code on the owner's machine (an M1 under Asahi Linux), on `main` |
| Owns | Direction (`docs/production/`), public headers, briefs, headless tests, tools, CI, review | Implementation, running the game, manual steps, real-hardware perf numbers |
| Never | Claims to have run the game; tunes feel without Builder numbers; pushes code to `main` without a PR | Skips manual steps; reinterprets a brief; takes an item not `Pending` with `owner: builder` |
| Lands work by | Docs, queue, brief and decision commits fast-forward to `main` (D-044); code, tests, tools, CI and skills through a PR it drives green and merges | Fast-forward commits straight to `main` |

`python3 tools/bootstrap.py` prints which role a session is.

## The queue

`WORK_QUEUE.json` is the single list of work, edited only through `tools/work_queue.py`
(`--help` lists every command). An item has an id (`PEO-123`), type (feature, bug, design,
research, perf, chore), severity S1 to S4, effort S to XL, owner, status, dependencies and
references. Its **brief** is `docs/production/briefs/<id>.md` and its **notes** are
`docs/production/notes/<id>.json`; `show <id>` prints everything.

```
Pending (architect) --/plan--> Pending (builder) --/start-work--> InProgress
   --> verify, commit, push each unit --> report note --> Validation (architect)
   --> /review --> complete (logged in docs/COMPLETED_WORK/YYYY-MM-DD.md)
                   or back to Pending with a numbered list of changes
Blocked     --> docs/production/handoffs/<id>.md (/handoff)
AwaitingUser --> waiting on the owner's answer to a D-xxx entry
```

`DEFERRED_WORK.json` parks items that only the owner may promote.

### A brief

Written by the Architect with `work_queue.py brief <id> --file brief.json`, never by hand:

| Field | Content |
|---|---|
| `goal` | One sentence: what is true when this is done |
| `context` | Files and doc anchors to read first, starting with the item's domain page |
| `units` | Ordered, commit-sized steps, each naming the layer it touches |
| `acceptance` | Observable criteria ("test X passes", "in game, Y happens") |
| `tests` | Headless tests that must pass, written by the Architect, may start red |
| `manual` | Exact in-game steps for the Builder and what to look for |
| `out_of_scope` | What not to touch, and where it went instead |

### A report

The Builder's note when moving an item to Validation:

```
commits: <sha> <sha>
tests: verify.py --frontend passed on <os>/<compiler>
manual: <step> -> <what happened>; ...
perf: <numbers, if the brief asked>
open: <questions or deviations, or "none">
```

## Decisions: what stops for the owner

Both agents stop and write an entry in `docs/production/DECISIONS_NEEDED.md` when a choice is
about **gameplay**, **abstraction** (a new core interface, a public header the frontend uses,
where state lives), **direction** (anything outside `vision.md`), or a **big rewrite** (three
or more core headers, or a signature used outside its file). They set the item
`AwaitingUser` and move on, and implement no option on speculation. Technical method (data
structures, algorithms, file layout, test style) they decide themselves.

An entry has a D-id, why now, what it affects, and 2 to 4 lettered options with one
recommended. The owner answers "D-052: B" in chat; the `/decisions` skill moves it to
`docs/production/decisions.md` with the reason and returns the item to `Pending`. Precedence
when sources disagree: the owner, then `decisions.md` and `docs/adr/`, then the owner's verbatim notes in
`docs/design/notes/`, then topic docs, then `AGENTS.md`.

## Skills and subagents

Canonical text in `.agents/skills/<name>/SKILL.md` and `.agents/agents/<name>.md`; the files in
`.claude/` only point there. Each is capped at 600 words.

| Skill | Who | Does |
|---|---|---|
| `/work` | either | Record, update, prioritise, defer or promote an item. Never implements |
| `/bug` | either | Record a bug with reproduction steps |
| `/plan` | Architect | Turn a Pending item into a full brief |
| `/start-work` | Builder | Take the next item, build it unit by unit, verify, push, report |
| `/burndown` | Builder | Work the queue S to XL, one fresh subagent per item, skipping decision items |
| `/review` | Architect | Review a Validation item against brief, vision, architecture and tests; complete or bounce |
| `/handoff` | either | Write a handoff for a Blocked item |
| `/status` | either | Queue summary, recent completed work, what each role should do next |
| `/decisions` | either | Show open decisions answer-ready, or record the owner's answers |
| `/cpp-core` | either | The C++ checklist: determinism, integer maths, allocation, ownership, API shape |
| `/perf` | either | Measure and report against the D-021 budgets |

| Subagent | Does |
|---|---|
| `core-checker` | Read-only: runs `/cpp-core` over a diff and returns findings |
| `perf-runner` | Before-and-after numbers against D-021 and `docs/bottlenecks.md` |
| `sim-debugger` | Reproduces a simulation bug from a seed, narrows it to the tick and commit, proposes a fix with a failing doctest |
| `docs-drift` | Updates docs a change made stale, using only what the diff proves |

## Guard rails

- **`verify.py` before every commit**; `--full` before pushing to `main`.
- **Git**: commit explicit paths, never `git add -A`, stash or `--no-verify`; messages
  `[PEO-123] imperative summary`, one queue unit per commit; no force-push, no squash; a
  merge commit for PRs.
- **Hooks** block secrets, big blobs, build output, and doc changes over their caps.
- **Word caps** (`tools/validate_docs.py`): `AGENTS.md` 450, `CLAUDE.md` 300,
  `docs/README.md` 500, `roles.md` 1200, `bottlenecks.md` 900, `vision.md` 900,
  `decisions.md` 1500 (archive older entries), `DECISIONS_NEEDED.md` 800, the queue JSON 2500
  content words, skills 600. They exist so every session can afford to read the rules.
- **Never**: skip, disable or weaken a test to get green; promote deferred items (only the
  owner does); widen scope beyond a brief (add a queue item instead).

## The cloud side

Architect sessions run as threads in the Claude project. The project's coordinator
starts a thread per ask; a review or brief batch is one thread with one subagent per item
(D-044). Reviews start when the owner says "review" in the project chat, with a 12 h safety poll
behind it. The project's shared folder holds review reports and probes (`reviews/`); the repo
is the record.

## The loop end to end

1. The owner asks for something in the project (a feature, a bug, "brief the next items").
2. The Architect records or briefs it, and raises anything that is a decision.
3. On the owner's machine, Claude Code in the checkout runs `/start-work` (one item) or
   `/burndown` (until stopped).
4. The Builder builds, runs the game, reports, and sets the item to Validation.
5. The owner says "review"; the Architect passes or bounces it.

Any step can be done by a person instead: the queue does not care who typed the code, as long
as the item moves through the same states with the same report.
