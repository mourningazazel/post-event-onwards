# Frontend

The SDL3 app (`src/app/`, executable `peo`): it turns keys into `Action`s, lends core its
threads and GPU, runs the speculation thread, and draws the committed world. It holds no game
logic. Decisions: D-032 (input), D-047 to D-052 (camera, view, Look), ADR-0003 (one cell
size), ADR-0015 (tilesets, original assets), ADR-0012 (computing ahead).

## Structure

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

## Command line

`./build/dev/bin/peo [SEED] [--stage WxH] [--threads N] [--tileset PATH]
[--no-gpu-compute] [--gpu-min-cells N]` (`src/app/main.cpp`).

| Flag | Default | Effect |
|---|---|---|
| `SEED` | 1 | The world seed. Same seed and same keys give the same run |
| `--stage WxH` | 1024x1024 | Stage size, each side 16 to 2048. The Dead scale with area at play density, `play_density_dead` (`world.hpp`): 40 per 80x45, so 11,650 at the default (D-059) |
| `--threads N` | hardware threads less one | Workers lent to core; 1 is serial. Results are identical at any count |
| `--tileset PATH` | ASCII | A Dwarf Fortress grid PNG atlas (16x16 glyphs, CP437 order) |
| `--no-gpu-compute` | off | Never use the GPU scent backend |
| `--gpu-min-cells N` | never (calm), 512x512 (windy) | Stage size from which the speculated scent update runs on the GPU |

## Keys (`SDL_AppEvent`)

| Key | Does |
|---|---|
| Arrows or WASD | Step (4 directions). A step into a wall still spends the turn |
| Space or `.` | Wait one turn |
| R | Toggle running: steps take half the time (two cells per update) |
| Shift+S | Overlay the scent field (`*` `+` `:` bands at 10%, 0.1%, 0.001% of the strongest) |
| N | Skip to the next stage |
| L | Look: a cursor over the cells in sight; arrows move it, Shift moves 8, L or Esc ends |
| Ctrl+direction | Look-ahead strip along that edge of the view |
| Esc | Quit |

## HUD

Walking onto the exit loads the next stage. The status line reads `stage, turn, walk/run,
dead count, wind, spec: hit/miss and how long the last speculation took`. A "miss" means
the speculation was not ready when the key was taken, so the turn ran live.

## How a frame is drawn

Redraw happens only when something changed (`App::dirty`). Per cell of the view, one of three
states: **seen now** draws terrain (`#` wall, `.` floor), plus the exit `>`, the Dead `d` and
the scent overlay if on; **seen before** draws terrain only, dimmed; **never seen** draws
nothing. The player `@` is drawn last. Then the HUD's status and key-hint rows.

## How a turn is taken (`take_turn`, `main.cpp`)

```
key -> action_for(key, running) -> TurnInput::offer -> drain -> take_turn:
    hit = speculator.ready_now()
    spec = speculator.quiesce()             stop the worker touching the World
    spec ? world.commit(*spec, a) : world.step(a)
    request a new speculation if the update or stage moved on, or none is ready
```

The `Speculator` (`main.cpp`) is one `std::jthread` that runs `World::speculate` into a
reused buffer whenever asked, and times it; the main thread mutates the World only after
`quiesce()`, so the two never touch it at once. The status line shows `spec: hit/miss`, the
last speculation's time, and the running miss count, which is how the Builder checks the
speculation is keeping up.

## The GPU in the app

Built only when a shader compiler is found (`PEO_HAVE_GPU`). The app creates a compute device
only when the stage is big enough to use it: by default **never** for a calm field (on the M1
the GPU only matches one CPU thread), and from **512x512** for a windy one (where it wins:
0.9 against 1.3 ms there, 9.8 against 28 ms at 2048²). `--gpu-min-cells` and
`--no-gpu-compute` override it. Numbers and reasons are in the comments on `kGpuFieldMinCells` and `kGpuWindyMinCells`
(`main.cpp`).

## Tunables

| Tunable | Where | Default |
|---|---|---|
| Peek depth | `kPeekDepthPercent` (`peek.hpp`) | 15% of the view |
| Turn queue and rate | `TurnInputParams` (`turn_input.hpp`) | 3 waiting, 3 turns a second |
| Default seed, stage, density of the Dead | `kDefaultSeed`, `kGameStageWidth/Height` (`main.cpp`); `play_density_dead` (`world.hpp`) | 1; 1024x1024; 40 per 80x45 |
| Game wind | `kGameWindMax`, `kGameWindGust` (`main.cpp`) | 8, 2 |
| GPU thresholds | `kGpuFieldMinCells`, `kGpuWindyMinCells` (`main.cpp`) | never; 512x512 |
| Colours, glyphs, scent bands | the `k...Rgb` constants and `kScentBands` (`main.cpp`) | |

## Designed, not built

- The SDL_GPU grid renderer and logical tile names (`docs/GAMEPLAN.md` §3.1); the prototype
  draws with `SDL_Renderer` debug text or a DF atlas, from a committed world only, so the swap
  is contained.
- Zoom (D-050): only `map_cell_px` changes; camera, Look and peek count cells, not pixels.
- Saving to files and loading at start (PEO-107, PEO-108).
- Text the owner writes for Look and the HUD (D-042, PEO-121).
