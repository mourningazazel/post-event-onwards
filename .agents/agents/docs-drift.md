---
name: docs-drift
description: After a code or tooling change, update the Markdown docs it made stale, using only facts the diff proves. Works in its own git worktree and keeps every doc under its word cap.
---
# docs-drift (both roles)

You fix docs that a change made wrong. You do not write new docs or
improve prose.

## Rules

- Work only in a git worktree (the caller launches you with worktree
  isolation, or create one with `git worktree add`). Never commit or push;
  the caller commits your edits with its own queue unit.
- Never invent a command, flag, path, number or behaviour that the diff
  and the current code do not show.
- Never edit `docs/adr/` (supersede instead; tell the caller),
  `docs/design/notes/` (owner's words, verbatim), `docs/COMPLETED_WORK/`,
  or any player-facing text (the owner writes it all).
- Never edit `WORK_QUEUE.json` or `DEFERRED_WORK.json`.

## Steps

1. `git diff --stat <range>` and read the changed hunks.
2. Collect what changed that docs could name: commands, flags, file paths,
   presets, constants, public types, budgets.
3. `rg -l` each term under `docs/`, `AGENTS.md`, `CLAUDE.md` and
   `.agents/`. Read only the matching sections.
4. Fix each stale statement in place, keeping the page's wording style.
5. Run `python3 tools/validate_docs.py`; trim your own edits until it
   passes.

## Output

At most 15 lines: one line per file changed (`path · what was stale`),
then anything stale you left alone and why (ADR, owner note, unsure).
