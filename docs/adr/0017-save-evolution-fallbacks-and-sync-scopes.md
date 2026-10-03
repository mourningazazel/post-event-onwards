# ADR-0017: Saves survive content changes, keep two fallbacks, and know what syncs

- Status: Accepted (owner note N023). The network layer itself is future work.
- Date: 2026-10-03
- Refines: ADR-0002 (binary, sectioned, versioned, checksummed, stable IDs). All of it stands.

## Context

Content will change under existing saves: items renamed, split, removed. A new build can crash
on, or corrupt, an old save. Later the game becomes networked: one machine (the host) runs the
world and remote clients join it, and each player must be able to carry on alone from their copy
afterwards. Characters are already generic, so player count is a question of active radius and
budget, not of special player code.

## Decision

1. **Two fallback copies.** Saving writes a new file beside the old one and swaps them only
   after the new file reads back with every checksum valid. The previous two good saves are
   kept, each tagged with the build and save-format version that wrote it. Loading tries the
   newest; on a checksum failure or a load crash flag left from the last attempt, the game
   offers the next copy.
2. **Content migration on load, never a refusal.** A content reference resolves through, in
   order: the ID-remap table (renames, splits); a **generic stand-in** of the nearest kind still
   present (a removed "claw hammer" becomes a generic hammer, keeping material, quality and
   contents); failing that, **junk**, an inert item that keeps the old name in its description
   ("a broken, unrecognisable tool") and can be dropped. The player is told once what was
   migrated. Migration is a pure function of (old record, current content), so it is testable.
3. **Every save section declares a sync scope,** chosen now so networking later is a transport,
   not a format change:

   | Scope | Sections | Network role |
   |---|---|---|
   | `world` | seed, event parameters, clock, generator versions, stored areas, horde aggregates, area clocks | the host owns it; clients receive it for the areas they have seen |
   | `character` | one character: body, stats, skills, mood, inventory, knowledge | the player's machine owns it; the host holds the authoritative copy while connected |
   | `local` | settings, key bindings, UI layout, map notes | never synced |

4. **Each copy stays playable alone.** A client keeps a full save of what it received: the world
   sections for areas it saw plus its character. Offline, it resumes as a single-player session
   from that copy; the two worlds then diverge, and no merge back is promised.
5. **Transport (future):** an encrypted channel with SSH as the default, through an
   MIT-compatible library or the system's `ssh` (ADR-0015). Clients send actions; the host sends
   state deltas for its sections. Determinism (ADR-0012, ADR-0014) is what lets a host replay
   a client's action exactly.

## Consequences

- PEO-091 builds the save file with sections, scopes and fallbacks; PEO-092 builds migration.
- Several players means several active radii: the detail rings and the Budget Manager must
  budget the union of players' areas, so horde ceilings divide between them
  (`docs/bottlenecks.md` gains the row when networking is planned).
- Nothing in core may assume one player: "the player" in a system means "each character".
