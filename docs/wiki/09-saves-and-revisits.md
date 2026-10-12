# 9 · Saves and revisits

## The save container (`save.hpp`, `save.cpp`, ADR-0002, ADR-0017, PEO-091)

A save is a versioned list of **sections**, one per system. The container knows nothing about
what a payload means; each system writes and reads its own.

```
"PEOSAVE\0"  u32 format (1)  u16-length build string  u32 section count
per section: u32 id ("WRLD" as 4 ASCII bytes)  u16 version  u8 scope  u8 codec (0 = raw)
             u64 length  u32 crc32 of the stored payload  payload
all little-endian
```

- `SyncScope`: **World** (the host owns it once games are networked), **Character** (the
  player's machine owns it), **Local** (never synced). ADR-0017 decision 3.
- `write_save(image)` and `read_save(bytes)` work on bytes in memory. `read_save` never
  throws and never trusts a length. Results: `Ok`, `BadHeader`, `Truncated`, `NewerFormat`
  (never guessed at), `Corrupt` (a World or Character section fails its CRC or validation). A
  bad Local section is dropped with an issue and the image stays Ok.
- `byte_io.hpp`: `ByteWriter` / `ByteReader`, little-endian fields.

### The World's sections (`world_save.cpp`)

| Id | Scope | Holds |
|---|---|---|
| `WRLD` | World | seed, every `WorldParams` field, stage index, the clocks, the spawn `Rng` state |
| `STAG` | World | a hand-built stage only; a generated one is regenerated from the seed |
| `SCNT` | World | the scent values, update count, and which tiles change next round |
| `HORD` | World | the Dead, their pending moves, and the desire field's log-odds |
| `OCCL` | World | the occupancy log since the last update |
| `CHAR` | Character | the player (written as a count, so a second character is a second record) |
| `SEEN` | Character | the player's map memory, one bit per cell (optional: older saves lack it) |

`World::save(build)` is deterministic (same world, same bytes). `World::restore(image,
issues)` checks **every value** against the stage it builds before touching anything: sizes
are bounded (`kMaxRestoreSide = 4096`, periods, rounds), counts are checked against the bytes
left before allocating, and values must be ones the code could have written. On anything but
`Ok` the World is untouched and `issues` says what and where. A restored world then continues
bit-identical to one never saved (`test_save.cpp:507`). The executor and field backend are not
saved; they are the app's.

Every section is version 1 today. A change to a section's layout bumps its version, and a
reader keeps reading the old ones (ADR-0017's save evolution: fallbacks, content migration on
load, PEO-092).

Not built: files on disk, the two-good-copies fallback and crash flag (PEO-107), zstd
compression (PEO-108). Nothing in the game calls save yet.

## The clock since the Event

`World::since_event()` = `start_day x kSubstepsPerDay + earlier stages' substeps + this
stage's`. 64 bits, never decreases. `WorldParams::start_day` defaults to 7: play starts one
week after the Event (the "one-week preset", ADR-0009).

## Aftermath curves (`aftermath.hpp`, ADR-0009, ADR-0011)

A `Curve` is up to 8 knots `(day, affected)`, piecewise linear between them and flat before the
first and after the last (it saturates). `affected` is in parts of 65,536. `affected_at(c, t)`
is integer only and exact at every knot. `valid()` requires days strictly rising and values
never falling.

The one curve in use is **the Dead's attrition** (`kDefaultAttrition`, `aftermath.hpp:93`):
the share of the Dead gone by each day since the Event.

| Day | 0 | 7 | 30 | 365 | 1825 and after |
|---|---|---|---|---|---|
| Gone | 0% | 5% | 25% | 70% | 85% |

These are proposals the owner tunes (ADR-0009); content data takes the curve over once content
loads.

## Fates and thinning (`revisit.hpp`, ADR-0019, PEO-094)

Each of the Dead carries a `fate`, drawn when it appears at clock `at` from a hash of the stage
seed and its index:

```
fate = affected_at(attrition, at) + hash mod (65535 - affected_at(at) + 1)
```

uniform in `[affected_at(at), 65535]`, so a unit that exists has not met the curve yet. It is
gone once the curve passes it:

```
thin_horde(horde, attrition, now):  drop every unit with fate < affected_at(attrition, now)
```

One comparison a unit, reading only `now`, never when it was stored. So several short
absences leave exactly what one long one does (`test_revisit.cpp:269`), and there is no
simulation to catch up.

## Leaving and coming back

- `World::store_area()` copies the stage index, the event clock, the horde and the map memory
  into an `AreaRecord` (none for a hand-built stage, which cannot be regenerated). Scent is
  not carried.
- `World::resume_area(record)` rebuilds the stage as `load_stage` would (same walls, wind, a
  cold scent field, the player at the entry), then moves any unit standing on the entry or a
  wall to the nearest free open cell (a ring scan, before thinning, so where it lands never
  depends on when), then thins the horde to the curve at `since_event()`. The map memory
  comes back if its size matches.

Nothing in play calls these yet. Designed next: player-made things age on revisit (PEO-109),
the catch-up as a streaming job when an area reloads (PEO-110), and D-040 says revisits have
no gate.

## Levers

`WorldParams::start_day` and `WorldParams::attrition` (any valid curve; an invalid one becomes
the default). The knot constants `kAttrition*` in `aftermath.hpp:85` to `:92`.
