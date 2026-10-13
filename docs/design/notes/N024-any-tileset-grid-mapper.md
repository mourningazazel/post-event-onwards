# N024 — Any tileset, mapped with a grid tool

- Date: 2026-10-13
- Source: owner, in the project chat ("a note for later")

> Here is a note for later. I did say we could use dwarf fortress tileset, but I don't want it
> being restricted to that, or the only way to play. When we implement more gui configuration
> options, I want a small tool that can overlay a grid onto a png, and then assign IDs to the
> squares, with common ones, but also letting you add more. A 16x16, or a 54x52, it shouldn't
> matter. The configuration should be saved to an ini file that can be shared for the tileset.
> IDs should be agnostic to the system, so a tileset that references an ID not represented, does
> not break game function, but also if a tileset does not contain something, it uses system
> defaults

## How it was taken

- The Dwarf Fortress grid stays a supported layout, and ASCII mode stays; neither is the only
  way to play. ADR-0015 point 3 is amended to say so (decisions.md, 2026-10-13).
- Not now: it waits for the wider GUI configuration work. Deferred item PEO-131 holds the
  requirements and what ties the game to the DF grid today.
- "A 16x16, or a 54x52" is read as any grid: any number of columns and rows, and any cell size
  in pixels, square or not. The tool asks for both; neither is assumed.
- IDs are names the game defines (`wall`, `floor`, `player`, ...), with a common set built in
  and room for more. An ini entry for an ID the game does not know is ignored with a warning;
  an ID the ini leaves out draws from the game's default.
</content>
</invoke>
<invoke name="Bash">
<parameter name="command">cd /home/claude/post-event-onwards && python3 tools/work_queue.py add --type feature --title "Tileset mapper: grid any PNG, name its cells with game IDs, share as an ini (N024)" --severity S4 --effort L --owner architect --ref docs/design/notes/N024-any-tileset-grid-mapper.md --ref src/app/glyph_atlas.hpp 2>&1 | tail -3