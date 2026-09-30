"""Reference roller for a compartment's contents (D-030, PEO-057): the executable spec
PEO-053 ports to C++. Everything comes from hash_u64(seed, salt, index), the same splitmix64
fold as src/core/include/peo/core/rng.hpp; no shared random stream anywhere, so a pick's
result never depends on how many other picks were made.

Order and salts (the spec):
  0  purpose: a weighted pick over the compartment's purposes, index 0.
  1  main fill: index 0 is the empty chance; index i >= 1 picks main entry i.
  2  main fill: index 0 is the roll count n; index i >= 1 picks main entry i's count.
  3  detail pick j's entry (j >= 1).         4  detail pick j's count.
  5  kind extras: table t fires when hash % 100 < chance_pct.
  6  ctx extras: the same, for tables that also match a context.
  7  kind extras picks: index t * 256 + 0 is the table's roll count, t * 256 + j pick j.
  8  ctx extras picks: the same.
A pick's entry is the weighted entry at hash % total weight; its count is lo + (hash >> 32)
% span. Roll counts (the main n, an extras table's rolls) and the empty chance use
lo + hash % span and hash % 100. A nested loot entry re-picks inside its table with
hash(parent hash, 9, depth). The main fill makes n turns. Without a detail every turn is a main pick. With a
detail, turns alternate main 1, detail 1, main 2, detail 2, ..., each side keeping its own
index, and the fill is never empty and at least two turns (a detail is authored evidence).
A pick that does not fit what is left (packed volume, mass, longest side) is skipped; its
turn is spent. Extras run after the main fill, tables in id order, and never push it out.
"""

from __future__ import annotations

import sys

from common import Db, load
from derive import ResolveError, item_expected, packed_dims, resolve_item, apply_modifier

MASK64 = (1 << 64) - 1
GOLDEN_GAMMA = 0x9E3779B97F4A7C15
SALT_PURPOSE, SALT_MAIN_ENTRY, SALT_MAIN_COUNT, SALT_DETAIL_ENTRY, SALT_DETAIL_COUNT = 0, 1, 2, 3, 4
SALT_EXTRAS_FIRE = 5   # + tag index (0 kind, 1 ctx)
SALT_EXTRAS_PICK = 7   # + tag index
EXTRAS_PICK_STRIDE = 256  # pick index space per extras table
PERCENT = 100
MIN_DETAIL_TURNS = 2
NESTED_DEPTH_SALT = 9  # a nested loot entry re-picks with hash(parent hash, 9, depth)


def splitmix64(z: int) -> int:
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
    return z ^ (z >> 31)


def hash_u64(a: int, b: int, c: int) -> int:
    """rng.hpp's hash_u64: three words folded with the golden-ratio gamma."""
    h = splitmix64((a + GOLDEN_GAMMA) & MASK64)
    h = splitmix64(h ^ ((b + GOLDEN_GAMMA) & MASK64))
    return splitmix64(h ^ ((c + GOLDEN_GAMMA) & MASK64))


def in_range(h: int, rng) -> int:
    lo, hi = (rng, rng) if isinstance(rng, int) else (rng[0], rng[-1])
    return lo + h % (hi - lo + 1)


def weighted(entries: list[dict], h: int, key: str = "w") -> dict:
    total = sum(e.get(key, 1) for e in entries)
    r = h % total
    for e in entries:
        r -= e.get(key, 1)
        if r < 0:
            return e
    return entries[-1]


def pick_line(db: Db, table: dict, h_entry: int, h_count: int, source: str, depth: int = 0) -> dict:
    """One pick from a loot-shaped table; nested loot entries re-pick inside their table."""
    e = weighted(table["entries"], h_entry)
    if "loot" in e:
        nested = db.get("loot", e["loot"])
        return pick_line(db, nested, hash_u64(h_entry, NESTED_DEPTH_SALT, depth + 1),
                         hash_u64(h_count, NESTED_DEPTH_SALT, depth + 1), source, depth + 1)
    return {"item": e["item"], "count": in_range(h_count >> 32, e.get("count", 1)), "mods": list(e.get("mods", [])),
            "substance": e.get("substance"), "fill_pct": e.get("fill_pct"), "source": source}


class Space:
    """What is left in a compartment: volume, mass, longest side."""

    def __init__(self, db: Db, comp: dict) -> None:
        self.db = db
        self.ml = comp.get("capacity_ml", 0)
        self.g = comp.get("max_mass_g")
        self.dim = comp.get("max_dim_mm")

    def take(self, line: dict) -> bool:
        try:
            it = resolve_item(self.db, line["item"])
            for m in line["mods"]:
                it = apply_modifier(self.db, it, m)
        except ResolveError:
            return False
        if self.dim is not None and packed_dims(self.db, it)[0] > self.dim:
            return False
        vol, mass = item_expected(self.db, it, line if line.get("substance") else None)
        vol, mass = vol * line["count"], mass * line["count"]
        if vol > self.ml or (self.g is not None and mass > self.g):
            return False
        self.ml -= vol
        if self.g is not None:
            self.g -= mass
        return True


def blend_table(db: Db, detail: str, kind: str) -> dict:
    for rec in db["detail_loot"].values():
        if rec["match"] == {"detail": detail, "kind": kind}:
            return db.get("loot", rec["loot"])
    return db.get("loot", db.get("detail", detail.split(".", 1)[1])["loot"])


def roll_container(db: Db, seed: int, comp: dict, ctx: str, detail: str | None = None,
                   turns: int | None = None) -> dict:
    """Roll one compartment: its purpose, name and lines. `ctx` is its context tag; `detail`
    a detail tag or None; `turns` overrides the rolled turn count (for prefix tests)."""
    purpose_id = weighted(comp["purposes"], hash_u64(seed, SALT_PURPOSE, 0))["purpose"]
    purpose = db.get("purpose", purpose_id)
    kind = f"kind.{purpose_id}"
    main = db.get("loot", purpose["loot"])
    space = Space(db, comp)
    lines: list[dict] = []

    n = turns
    if n is None:
        empty = hash_u64(seed, SALT_MAIN_ENTRY, 0) % PERCENT < main.get("empty_chance_pct", 0)
        n = 0 if empty and detail is None else in_range(hash_u64(seed, SALT_MAIN_COUNT, 0), main.get("rolls", 1))
        if detail is not None:
            n = max(n, MIN_DETAIL_TURNS)
    blend = blend_table(db, detail, kind) if detail else None
    i = j = 0
    for turn in range(n):
        if blend is not None and turn % 2 == 1:
            j += 1
            line = pick_line(db, blend, hash_u64(seed, SALT_DETAIL_ENTRY, j),
                             hash_u64(seed, SALT_DETAIL_COUNT, j), "detail")
        else:
            i += 1
            line = pick_line(db, main, hash_u64(seed, SALT_MAIN_ENTRY, i), hash_u64(seed, SALT_MAIN_COUNT, i), "main")
        if space.take(line):
            lines.append(line)

    tags = {"kind": kind, "ctx": ctx}
    passes: list[list[str]] = [[], []]
    for xid in sorted(db["extras"]):
        match = db["extras"][xid].get("match") or {}
        if all(tags.get(k) == v for k, v in match.items()):
            passes[1 if "ctx" in match else 0].append(xid)
    for ti, ids in enumerate(passes):
        for t, xid in enumerate(ids):
            rec = db["extras"][xid]
            if hash_u64(seed, SALT_EXTRAS_FIRE + ti, t) % PERCENT >= rec["chance_pct"]:
                continue
            base = t * EXTRAS_PICK_STRIDE
            rolls = in_range(hash_u64(seed, SALT_EXTRAS_PICK + ti, base), rec.get("rolls", 1))
            for k in range(1, rolls + 1):
                h = hash_u64(seed, SALT_EXTRAS_PICK + ti, base + k)
                line = pick_line(db, rec, h, h, "extras")
                if space.take(line):
                    lines.append(line)
    return {"purpose": purpose_id, "name": purpose["name"], "lines": lines}


def main() -> int:
    """python3 tools/content/generate.py <item> <compartment> <seed> [ctx] [detail]"""
    if len(sys.argv) < 4:
        print(main.__doc__)
        return 2
    db = load()
    item, cname, seed = sys.argv[1], sys.argv[2], int(sys.argv[3])
    ctx = sys.argv[4] if len(sys.argv) > 4 else "ctx.home"
    detail = sys.argv[5] if len(sys.argv) > 5 else None
    comp = next(c for c in resolve_item(db, item)["compartments"] if c["name"] == cname)
    r = roll_container(db, seed, comp, ctx, detail)
    print(f"{r['name']} ({r['purpose']})")
    for line in r["lines"]:
        print(f"  {line['count']:3d} x {line['item']:24s} {line['source']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
