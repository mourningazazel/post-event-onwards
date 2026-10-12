# 8 · Player view and frontend

What the player sees, and how the SDL app wraps the simulation. Decisions: D-042 (the owner
writes every player-facing line), D-047 to D-053 (camera, sight, memory, Look, nothing named
past 20 ft), D-032 (input), ADR-0003 (one cell size), ADR-0015 (tilesets, original assets).

## Sight (`sight.hpp`, `sight.cpp`, PEO-119)

`SightField::compute(opaque, origin, params)` marks every cell within the optical range that
walls do not hide, by **symmetric shadowcasting**: if an open cell sees another, it is seen
back, so the Dead that can see (later) can reuse the same pass. Integer slopes (fractions
compared by cross-multiplying), a disc test on `dx² + dy² ≤ r²`, no floats.

- Range: `kOpticalRangeCells = 152` (500 ft, D-049). `SightParams::range_cells` is the one
  hook binoculars will use. Capped at `kMaxSightRangeCells = 2048`.
- `opaque` is `stage.blocked` today; doors, windows and glass will need a separate sight
  layer.
- Cost grows with the open area in range. It runs once per player move, in the frontend
  (`refresh_sight`), never per update. Buffers are reused.

## Map memory (`memory.hpp`, PEO-124, D-051 A)

`MapMemory` is one byte per stage cell: walls and floor once seen stay known out of sight,
but nothing on them (no Dead, no exit, no scent). `World::remember(sight)` folds a sight field
in, walking only the sight window; a field computed from anywhere but the player's current
cell is ignored. The memory resets on every stage load, travels with `store_area` /
`resume_area`, and is saved (section SEEN). A second character later is simply a second
`MapMemory`.

## Look (`look.hpp`, PEO-120)

`LookCursor` only ever rests on a cell the player sees now, so looking can never tell more
than sight already has, and it never spends a turn. `look_at(sight, stage, horde, player,
cell)` returns a `LookReport` (data only, no words: distance squared, player, opaque, exit,
how many Dead stand there) or nothing for an unseen cell. The HUD line for it is empty on
purpose: the owner writes that text (D-042), and PEO-121 decides what can be made out at each
distance.

## The frontend (`src/app/`)

`main.cpp` uses SDL3's callback entry points (`SDL_AppInit`, `SDL_AppEvent`,
`SDL_AppIterate`, `SDL_AppQuit`). Everything else in `src/app` is a small SDL-free header,
tested headlessly in `tests/core`:

| File | Job |
|---|---|
| `camera.hpp` (PEO-117) | Which stage cells the window shows: centred on the player, never clamped to the stage; cells past the edge draw dark |
| `hud.hpp` (PEO-123) | Wrap HUD lines at spaces when the window is under 80 columns (tiling window managers) |
| `peek.hpp` (PEO-127) | Ctrl+direction look-ahead strip along a view edge, 15% of the view deep per press |
| `glyph_atlas.hpp` (PEO-097) | Dwarf Fortress grid tilesets: 16x16 glyphs in CP437 order in one square PNG |
| `thread_pool.hpp` (PEO-080) | The `Executor` core is lent; a nested or concurrent run goes inline, so no deadlock |

### How a frame is drawn

Redraw happens only when something changed (`App::dirty`). Per cell of the view, one of three
states: **seen now** draws terrain (`#` wall, `.` floor), plus the exit `>`, the Dead `d` and
the scent overlay if on; **seen before** draws terrain only, dimmed; **never seen** draws
nothing. The player `@` is drawn last. Then the HUD's status and key-hint rows.

### How a turn is taken (`take_turn`, `main.cpp:402`)

```
key -> action_for(key, running) -> TurnInput::offer -> drain -> take_turn:
    hit = speculator.ready_now()
    spec = speculator.quiesce()             stop the worker touching the World
    spec ? world.commit(*spec, a) : world.step(a)
    request a new speculation if the update or stage moved on, or none is ready
```

The `Speculator` (`main.cpp:201`) is one `std::jthread` that runs `World::speculate` into a
reused buffer whenever asked, and times it; the main thread mutates the World only after
`quiesce()`, so the two never touch it at once. The status line shows `spec: hit/miss`, the
last speculation's time, and the running miss count, which is how the Builder checks the
speculation is keeping up.

### The GPU in the app

Built only when a shader compiler is found (`PEO_HAVE_GPU`). The app creates a compute device
only when the stage is big enough to use it: by default **never** for a calm field (on the M1
the GPU only matches one CPU thread), and from **512x512** for a windy one (where it wins:
0.9 against 1.3 ms there, 9.8 against 28 ms at 2048²). `--gpu-min-cells` and
`--no-gpu-compute` override it. Numbers and reasons: `main.cpp:85` to `:96`.

## Levers

| Lever | Where | Default |
|---|---|---|
| Sight range | `kOpticalRangeCells` (`sight.hpp`) or `SightParams` | 152 cells |
| Look jump | `kLookJumpCells` (`look.hpp`) | 8 cells |
| Peek depth | `kPeekDepthPercent` (`peek.hpp:21`) | 15% of the view |
| Turn queue and rate | `TurnInputParams` (`turn_input.hpp`) | 3 waiting, 3 turns a second |
| Default seed, stage, density of the Dead | `main.cpp:64`, `:123` to `:131` | 1; 200x120; 40 per 80x45 |
| Game wind | `kGameWindMax`, `kGameWindGust` (`main.cpp:67`) | 8, 2 |
| GPU thresholds | `kGpuFieldMinCells`, `kGpuWindyMinCells` (`main.cpp:91`) | never; 512x512 |
| Colours, glyphs, scent bands | `main.cpp:70` to `:83`, `:186` | |
