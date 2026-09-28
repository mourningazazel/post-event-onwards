"""Resolver and derivations: the executable spec for how item data turns into behaviour.

The C++ engine will port these formulas; tests in tests/content/ pin their results so the
port can be checked against the same expectations (N002 purpose tests, N011).
"""

from __future__ import annotations

import copy
import math

from common import Db

FLAMMABILITY_RANK = {"none": 0, "low": 1, "moderate": 2, "high": 3, "extreme": 4}
TOUGHNESS = ["brittle", "fragile", "moderate", "tough", "very_tough"]
AERO_BASE = {"poor": 6, "fair": 9, "good": 12}
IGNITE_BY_METHOD = {"flame": 6, "strike": 6, "flint": 4, "spark": 4, "electric": 2}
BIND_BY_TOUGHNESS = {"brittle": 0, "fragile": 1, "moderate": 3, "tough": 5, "very_tough": 7}
SLICE_CAP = 5          # max derived cut from an edge alone (see capabilities())
WIELD_MAX_G = 12000    # heaviest thing that can be swung/levered as a tool or weapon
PART_ROLES_EDGE = ("blade", "edge", "head", "bit")
PART_ROLES_POINT = ("point", "tip", "blade", "head")
PART_ROLES_STRIKE = ("head", "striking_face", "face", "body")


class ResolveError(Exception):
    pass


def rnd(x: float) -> int:
    """Round half away from zero — the spec's rounding rule (engine ports must match;
    Python's round() is banker's rounding and would diverge on .5 values)."""
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))


def deep_merge(base: dict, over: dict) -> dict:
    out = copy.deepcopy(base)
    for k, v in over.items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = deep_merge(out[k], v)
        else:
            out[k] = copy.deepcopy(v)
    return out


def resolve_item(db: Db, iid: str, _seen: tuple = ()) -> dict:
    rec = db.get("item", iid)
    if rec is None:
        raise ResolveError(f"unknown item '{iid}'")
    if iid in _seen:
        raise ResolveError(f"inheritance cycle: {' -> '.join(_seen + (iid,))}")
    parent = rec.get("parent")
    if parent:
        base = resolve_item(db, parent, _seen + (iid,))
        base.pop("abstract", None)
        base.pop("why", None) if "why" in rec else None
        item = deep_merge(base, rec)
        # tags accumulate through inheritance (parent tags + own tags), unlike other arrays
        item["tags"] = list(dict.fromkeys(base.get("tags", []) + rec.get("tags", [])))
    else:
        item = copy.deepcopy(rec)
    item["id"] = iid
    return item


# ---------------------------------------------------------------- modifiers
def _walk(obj: dict, path: str, create: bool = True):
    parts = path.split(".")
    cur = obj
    for p in parts[:-1]:
        if p not in cur:
            if not create:
                return None, None
            cur[p] = {}
        cur = cur[p]
    return cur, parts[-1]


def apply_ops(item: dict, ops: list[dict]) -> None:
    for op in ops or []:
        holder, key = _walk(item, op["path"])
        kind, val = op["op"], op.get("value")
        cur = holder.get(key)
        if kind == "set":
            holder[key] = copy.deepcopy(val)
        elif kind == "add":
            holder[key] = (cur or 0) + val
        elif kind == "mul":
            if isinstance(cur, list):
                holder[key] = [rnd(x * val) for x in cur]
            else:
                holder[key] = rnd((cur or 0) * val)
        elif kind == "append":
            holder[key] = list(cur or []) + (val if isinstance(val, list) else [val])
        elif kind == "remove":
            if isinstance(cur, list):
                holder[key] = [x for x in cur if x not in (val if isinstance(val, list) else [val])]
            else:
                holder.pop(key, None)
        else:
            raise ResolveError(f"unknown op '{kind}'")


def applies(db: Db, item: dict, applies_to: dict) -> bool:
    if not applies_to:
        return True
    tags = set(item.get("tags", [])) | set(derived_tags(db, item))
    if "items" in applies_to and item["id"] not in applies_to["items"]:
        return False
    if "tags_any" in applies_to and not any(
            t in tags or any(x.startswith(t + ".") for x in tags) for t in applies_to["tags_any"]):
        return False
    if "tags_all" in applies_to and not all(t in tags for t in applies_to["tags_all"]):
        return False
    if "shapes" in applies_to and item.get("shape") not in applies_to["shapes"]:
        return False
    return True


def apply_modifier(db: Db, item: dict, mid: str) -> dict:
    mod = db.get("modifier", mid)
    if mod is None:
        raise ResolveError(f"unknown modifier '{mid}'")
    out = copy.deepcopy(item)
    apply_ops(out, mod.get("ops", []))
    out["tags"] = [t for t in out.get("tags", []) if t not in mod.get("remove_tags", [])]
    out["tags"] = list(dict.fromkeys(out["tags"] + mod.get("add_tags", [])))
    out.setdefault("applied", []).append(mid)
    return out


def display_name(db: Db, item: dict) -> str:
    parts = {"prefix": [], "brand": [], "variant": [], "suffix": []}
    for t in item.get("tags", []):
        tag = db.get("tag", t)
        if tag and "name_part" in tag:
            np = tag["name_part"]
            parts[np["slot"]].append((np.get("order", 50), np["text"]))
        if t.startswith("brand."):
            b = db.get("brand", t[6:])
            if b:
                parts["brand"].append((40, b.get("name", t[6:])))
    for mid in item.get("applied", []):
        mod = db.get("modifier", mid) or {}
        if "name_part" in mod:
            np = mod["name_part"]
            parts[np["slot"]].append((np.get("order", 50), np["text"]))
    words = []
    for slot in ("prefix", "brand", "variant"):
        words += [t for _, t in sorted(parts[slot])]
    words.append(item.get("name", item["id"]))
    words += [t for _, t in sorted(parts["suffix"])]
    return " ".join(words)


# ---------------------------------------------------------------- materials
def parts(item: dict) -> dict:
    return item.get("composition", {}) or {}


def main_part(item: dict) -> tuple[str, dict] | tuple[None, None]:
    comp = parts(item)
    if not comp:
        return None, None
    return max(comp.items(), key=lambda kv: kv[1].get("share", 0))


def material(db: Db, mid: str | None) -> dict:
    return (db.get("material", mid) if mid else None) or {}


def main_material(db: Db, item: dict) -> dict:
    _, p = main_part(item)
    return material(db, p.get("material")) if p else {}


def role_material(db: Db, item: dict, roles: tuple[str, ...]) -> dict:
    comp = parts(item)
    for r in roles:
        if r in comp:
            return material(db, comp[r].get("material"))
    return main_material(db, item)


def derived_tags(db: Db, item: dict) -> list[str]:
    fams = {material(db, p.get("material")).get("family") for p in parts(item).values()}
    return [f"mat.{f}" for f in sorted(x for x in fams if x)]


# ---------------------------------------------------------------- derivations
def clamp(v: float, lo: int = 0, hi: int = 10) -> int:
    return int(max(lo, min(hi, rnd(v))))


def total_mass(db: Db, item: dict, contents_ml: int = 0, substance: str | None = None) -> int:
    m = item.get("mass_g", 0)
    if contents_ml and substance:
        s = db.get("substance", substance) or {}
        m += contents_ml * s.get("density_mg_ml", 1000) // 1000
    return m


def capabilities(db: Db, item: dict) -> dict[str, int]:
    c: dict[str, int] = {}
    f = item.get("features", {}) or {}
    mm = main_material(db, item)
    dims = item.get("dims_mm", [0, 0, 0])
    length = dims[0] if dims else 0
    mass = item.get("mass_g", 0)
    shape = item.get("shape", "")
    rigidity = mm.get("rigidity", 0)

    if "edge" in f:
        em = role_material(db, item, PART_ROLES_EDGE)
        sharp = f["edge"].get("sharpness", 0)
        # A slicing edge alone tops out at 5: it cannot part hardness >= 6 materials (metals,
        # glass, stone). Those need saw/grinder/bolt-cutter capabilities (authored).
        c["cut"] = min(SLICE_CAP, clamp(sharp * em.get("edge_holding", 0) / 10))
        chop = c["cut"] * min(1.0, mass / 1200) * (1.3 if item.get("balance") == "head" else 1.0)
        c["chop"] = clamp(chop)
        c["scrape"] = clamp(3 + sharp // 3)
    if "saw_edge" in f:
        c["saw"] = 5 if f["saw_edge"].get("coarse", True) else 7
    if "point" in f:
        pm = role_material(db, item, PART_ROLES_POINT)
        c["pierce"] = clamp(f["point"].get("sharpness", 0) * pm.get("rigidity", 0) / 10)

    # Wieldability gate: things too heavy to swing (or fixed in place) get no derived hammer/pry;
    # blocking/barricade value comes from mass, not these capabilities.
    wieldable = mass <= WIELD_MAX_G and not item.get("fixed")
    if wieldable and mass > 0 and shape not in ("fabric", "cord", "bag", "granular"):
        sm = role_material(db, item, PART_ROLES_STRIKE)
        h = math.log2(max(mass, 50) / 50) + sm.get("hardness", 0) / 3
        if "striking_face" in f:
            h *= 1.2
        if sm.get("rigidity", 0) < 5 or rigidity < 5:
            h *= 0.5
        c["hammer"] = clamp(h)

    if wieldable and shape in ("rod", "blade") and length > 0:
        c["pry"] = clamp(rigidity * min(1.0, length / 600))
    if length:
        c["reach"] = length

    if shape == "cord":
        if mm.get("family") == "metal":
            c["bind"] = 6
        else:
            c["bind"] = BIND_BY_TOUGHNESS.get(mm.get("toughness", ""), 0)
    div = item.get("divisible") or {}
    if div.get("mode") == "sheet" and mm.get("family") == "textile":
        c["bind"] = max(c.get("bind", 0), 3)

    if "ignition" in f:
        c["ignite"] = IGNITE_BY_METHOD.get(f["ignition"].get("method", ""), 0)
    if "heat_source" in f:
        c["heat"] = f["heat_source"].get("output", 5)
    if "light" in f:
        c["light"] = f["light"].get("radius_m", 0)
    if "container" in f:
        compat = f["container"].get("compat", [])
        if "liquid" in compat:
            c["contain_liquid"] = 1
        if "granular" in compat:
            c["contain_granular"] = 1
        if f["container"].get("sealable"):
            c["seal"] = 1
    if "tube" in f:
        t = f["tube"]
        if t.get("inner_diameter_mm", 0) >= 4 and t.get("length_mm", length) >= 500:
            c["siphon"] = 1
    if "wearable" in f:
        w = f["wearable"]
        if w.get("insulation_clo10"):
            c["insulate"] = clamp(w["insulation_clo10"] / 3)
        if w.get("waterproof"):
            c["waterproof"] = w["waterproof"]

    for k, v in (item.get("capabilities") or {}).items():   # authored values win
        c[k] = v
    return c


def energy_class(mass_g: int, thrown: bool = True) -> str:
    if mass_g < 300:
        return "low"
    if mass_g < 3000:
        return "medium"
    return "high"


def impact_outcome(db: Db, item: dict, energy: str) -> str:
    override = (item.get("reactions") or {}).get("impact") or {}
    if energy in override:
        return override[energy]
    tough = main_material(db, item).get("toughness", "moderate")
    return db.merged["default"].get("impact", {}).get(tough, {}).get(energy, "none")


def throw_range_tiles(db: Db, item: dict, mass_g: int | None = None) -> int:
    m = item.get("mass_g", 0) if mass_g is None else mass_g
    if m > 8000 or item.get("fixed"):
        return 0
    shape = db.get("shape", item.get("shape", "")) or {}
    aero = item.get("aerodynamics") or shape.get("aero", "poor")
    factor = 1.0 if m <= 800 else math.sqrt(800 / m)
    if m < 100:
        factor *= 0.6
    return max(1, rnd(AERO_BASE[aero] * factor))


def mass_estimate(db: Db, item: dict) -> int | None:
    dims = item.get("dims_mm")
    comp = parts(item)
    shape = db.get("shape", item.get("shape", "")) or {}
    if not dims or not comp or not shape:
        return None
    fill = item.get("fill_pct", shape.get("fill", 50))
    vol_cm3 = dims[0] * dims[1] * dims[2] / 1000 * fill / 100
    if item.get("hollow"):
        vol_cm3 *= 0.35
    # Shares are MASS fractions, so the mixture density is the harmonic mean:
    # rho = 1 / sum(share_i / rho_i)
    shares = [(p.get("share", 0) or 1, material(db, p.get("material")).get("density_mg_cm3", 1000))
              for p in comp.values()]
    total = sum(s for s, _ in shares)
    inv = sum((s / total) / max(rho, 1) for s, rho in shares)
    dens = 1 / inv if inv else 1000
    return int(vol_cm3 * dens / 1000)


def rigidity(db: Db, item: dict) -> int:
    return main_material(db, item).get("rigidity", 0)


def flammability_rank(db: Db, item_or_sub: dict, is_substance: bool = False) -> int:
    if is_substance:
        return FLAMMABILITY_RANK.get(item_or_sub.get("flammability", "none"), 0)
    return FLAMMABILITY_RANK.get(main_material(db, item_or_sub).get("flammability", "none"), 0)
