# Content

## What it is

`content/` holds the game's catalogue as TOML: item archetypes, materials, substances,
modifiers and detail tables, brands, building and room types, loot tables and occupant
profiles. `python3 tools/content/lint.py` prints the record count of every type. It is
authored mostly by Claude (owner notes N008, N011) and checked by Python. The game loads it at
startup (PEO-008): the frontend reads every `*.toml` under `content/` (or `--content DIR`) in
sorted path order, core's `load_content` parses them and resolves every item through its parent
chain, and a broken file stops the game before its window opens, naming the file and line. The
ctest `content_golden` runs `tools/content/golden.py`, which diffs every resolved value from
`peo_content_dump` against `derive.resolve_item`. The Python tools stay the checker and the
**executable spec** the rest of the C++ port must match.

Schema reference: `content/README.md`. Design: `docs/design/content-model.md` (materials,
features, capabilities, reactions, tags; attachment and joints; generated descriptions;
brands; quality), `docs/design/survival-actions.md`, `docs/design/world-catalog.md`,
`docs/design/purposes.md`.

## The model in brief

- **Registry first** (`content/registry/`): every property, feature, capability, tag,
  stimulus, fastener, scent channel, action, shape, skill and unit is registered with a
  description and its readers before anything may use it.
- **Materials** (`materials/`) are the physical truth: density, hardness, flammability and so
  on. Items are compositions of materials (`[item.x.composition.part]`, with mass shares).
- **Items** (`items/<domain>/`) are archetypes: portable things and world objects alike
  (furniture, fixtures, vehicles). Lean: a new item only if it behaves differently. Colour,
  print, small wear and flavour go to **modifiers** and **detail tables** that generation
  attaches. Inheritance by `parent` (tables merge, arrays replace).
- **World** (`world/`): settings, building types, room types, loot tables, outdoor sets, and
  the purposes, details and may-appear extras that decide what a container holds (D-030).
- **Profiles** (`profiles/`): what occupants wore and carried, by occupation and time of the
  Event.
- **Brands** (`brands/`): a fictional brand universe; no real brands (N010).
- Rules: integers only, SI units (`registry/units.toml`), real dimensions and masses (the
  linter checks mass against dimensions x density), qualities 0 to 10, original content only.

## The Python tools (`tools/content/`)

| Tool | Job |
|---|---|
| `common.py` | Loads every TOML file into one database keyed by record type, remembering each record's file |
| `lint.py` | Schema, vocabulary and reference checks; mass plausibility; container and room fit. Errors fail; `--strict` fails on warnings |
| `derive.py` | **The executable spec** for turning item data into behaviour: capabilities from features and materials, impact, throw range, packed size, ignition, binding. The C++ port must reproduce it |
| `generate.py` | The reference roller for a compartment's contents (D-030, PEO-057): every pick from `hash_u64(seed, salt, index)`, the same splitmix64 fold as `rng.hpp`, so the C++ port can be checked roll for roll |
| `test.py` | Runs `tests/content/`: property expectations, action-chain feasibility, pinned rolls, the `peo_convert` fixture and the .peo round trip |
| `to_peo.py` | One-time converter of the TOML tree to .peo (D-057, PEO-136), `--out DIR`; its path, field and value tables are `docs/design/content-format.md` 'Converting from TOML'. Refuses rather than guesses |
| `peo_load.py` | Reads a .peo tree into the same database `common.py` builds (record type and flat ID from the path table, an item's parent from its header path) |
| `golden.py` | Prints every resolved item for the engine diff; `--peo` converts `content/`, reads it back, maps it to TOML names with an independent inverse and fails on any difference |

`verify.py` and the `docs` CI job run `lint.py` and `test.py`. `tests/content/`:

- `expectations/*.toml`: `[[expect]] item, path, op, value, why`, for example a capability's
  magnitude or an item's packed size.
- `chains/*.toml`: survival action chains that must be feasible with the catalogue (N002's
  intended-use tests).
- `rolls/pinned.toml`: container rolls pinned seed by seed, plus determinism, prefix
  stability and the detail blend over 200 seeds.

## Tunables

The content is itself the tunable: numbers in TOML. When the derivations land (PEO-128), item
behaviour comes from materials and features through `derive.py`'s formulas (the named constants at the top of
`derive.py`, such as `SLICE_CAP`, `WIELD_MAX_G`, `IGNITE_BY_METHOD`, are tunables). Container
fill comes from loot tables and purposes through `generate.py`'s order of salts.

## Designed, not built

Derivations in C++ (PEO-128), modifiers and the variant table (PEO-129), quality and
degradation (PEO-013), items as seeds that instantiate on
change (PEO-050), the contents generator in C++ (PEO-053), skills (PEO-012), locks (PEO-019),
books (PEO-020), generic parent archetypes and remaps (PEO-116), content migration on load
(PEO-092), the building census (PEO-014).
