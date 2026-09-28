# Decisions

Rolling log of choices that shape the code. Newest first. When this passes
its cap, move the oldest entries to `decisions-archive/`.

Format: date · title · decision · why · consequences.

## 2026-09-28 · N014 · The Dead's AI, content depth, skills 1–100, Mood and Sanity, drugs

User direction, recorded verbatim in `docs/design/notes/N014-*.md` and worked into
`scent-mobs.md` round 3, `survival-actions.md` §2.15–2.16, `content-model.md` §10–11,
`world-catalog.md` §10, G04, ADR-0012 and `purposes.md`. Headlines: one weighted-draw movement
rule with aggregate and company pull replaces the recruitment cascade; triggers steer by
general direction and expire displaced; sound events are kind + dB; trips cascade and trample
hurts; alerted ×1.5, following ×2; skills are 1–100 with percentages and failure outcomes;
every item has a quality and a "broken" text; building census and category retail; Mood and
Sanity are main mechanics; drugs and explosives exist; maps are rare and mark general
locations; crows are text. Consequence: the registry skill scale changed to 1–100
(`content/registry/skills.toml`, `actions.toml` use `unlock`/`difficulty`).

## 2026-09-28 · The design corpus from the feature branches is the design of record

The `content/survival-catalog` branch (GAMEPLAN, SYSTEMS, ADR-0001–0011, gameplay model,
notes N001–N013, content model, survival actions, world catalogue, purposes, playtest harness,
403 authored archetypes) is merged. ADRs stay the record of architecture decisions; this file
stays the rolling log and points at them. `SYSTEMS.md` is the target map, reached one system at
a time through the queue, not by a framework-first rewrite (architecture.md).

## 2026-09-28 · D-002 · Turn-based, with the world computed while waiting

User decision. The world advances only when the player acts (a step or a
wait is a turn); it never runs on a clock. Modern twist: the idle time
between turns is used to compute ahead. The player's action changes only a
small neighbourhood, so the next turn is computed speculatively while
waiting and patched on input. Why: readable tactical play with no waiting
on the simulation, and the 5,000-Dead budget spent when the player is not
looking. Consequence: `World::step(action)` is the only way time moves; a
`speculate / commit` pair must produce bit-identical state to `step`
(golden test); rendering reads the last committed turn, never a
speculation buffer.

## 2026-09-28 · D-001 · `Dead`, `horde`, and how the Dead are described

User decision. Code: `struct Dead` is one unit, `horde` is any collection
(`std::vector<Dead>`, `step_horde`). Fiction: generically "the Dead"; a
large group is "a Horde"; a specific one is described as the corpse it was
("dead office worker", "mutilated cop", "armless child"), bearing unhealed
wounds from the Event, discoloured or rotting. Consequence: `Hordeling` is
gone; descriptions of individual Dead come from a corpse descriptor, not a
type name.

## 2026-09-28 · The enemies are "the Dead", never "Dead"

User decision. They behave like Dead; the word is banned everywhere for
aesthetic reasons. Consequence: docs, UI strings, identifiers and comments
say Dead/dead/horde. Code identifier choice is open as D-001.

## 2026-09-28 · The user owns gameplay and direction decisions

Agents never pick between gameplay, abstraction or direction options, nor
start a large rewrite, without an answered entry in `DECISIONS_NEEDED.md`.
Technical method is the agents' call. Why: the user wants to steer what the
game is, not how each function is written. Consequence: an item waiting on a
decision is `AwaitingUser` with the D-id in its note.

## 2026-09-28 · Two-role agent pipeline

Cloud Claude (Architect) plans, briefs, tests headlessly and reviews; local
Claude (Builder) implements and runs the game. State is exchanged only via
`WORK_QUEUE.json`. Why: the cloud has no display and no user hardware, the
laptop has no time for planning and review; both need to survive context
resets. Consequence: no work starts without a brief; nothing completes
without a report.

## 2026-09-28 · Core/frontend split enforced by the build

`peo::core` cannot link SDL; the `headless` preset builds without it. Why:
tests and cloud sessions must run with no display and no network beyond git.
Consequence: any logic in `src/app` is a review finding.

## 2026-09-28 · doctest, vendored

`third_party/doctest/doctest.h` is committed. Why: zero network during
configure, sub-second compile, one binary. Consequence: upgrade by replacing
the header and noting the version here (currently 2.4.12).

## 2026-09-28 · SDL3 from system or FetchContent

`find_package(SDL3)` first, else fetch the pinned tag in
`cmake/FindOrFetchSDL3.cmake`. Why: one command works on a fresh laptop and
in CI. Consequence: bumping SDL is one line and one CI run.

## 2026-09-28 · xoshiro256** for all randomness

No `std::mt19937`, no `rand()`. Why: identical sequences on every platform,
fast, tiny state. Consequence: every random decision takes an `Rng&`.

## 2026-09-28 · Word caps on agent-facing docs

Caps live in `tools/validate_docs.py`; CI enforces them. Why: docs that
grow unbounded stop being read. Consequence: to add, first cut.
