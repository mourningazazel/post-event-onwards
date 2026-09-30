#!/usr/bin/env python3
"""Content tests: property expectations and action-chain feasibility (N002, N011).

tests/content/expectations/*.toml:
    [[expect]]  item, path, op (== != >= <= > < in has), value, why
      path forms: cap.<capability> | impact.<low|medium|high> | throw_range | name
                  | mass_est | packed_dims | packed_long | <dotted path into the resolved item>
      optional: mods = [modifier ids applied before checking]
    [[expect]]  loot (instead of item), path = loot_volume_ml | loot_mass_g, op, value, why
    [[expect]]  capacity_ml (instead of item), path = search_seconds, op, value, why
      item path search_seconds uses the item's container capacity
      item paths soak_rate (int, x the floor) and soak_portable (bool): D-013, D-028
      item paths compartment_count, and compartment.<name>.<key> (PEO-054)

tests/content/rolls/*.toml:
    [[roll]] item, compartment, seed, ctx, detail?, purpose, lines = [[item, count, source], ...]
      pinned rolls of generate.roll_container (D-030, PEO-057); run_rolls adds determinism,
      prefix stability, context-only extras and the detail blend over 200 seeds.

Inline: the container fit rule of lint.py fails a too-small kitchen base run (PEO-052), and
the room fit rule fails a too-small kitchen (PEO-054).

tests/content/chains/*.toml:
    [chain.<id>] title, source
    [[chain.<id>.steps]] action, bind = { role = id }, and optional checks:
        yields = "<item>"             target's salvage includes this item, tool meets its requirement
        fuel = "<substance>"          bound 'source' tank/container can hold this substance
        contents = { substance, ml }  used for thrown-mass checks
        expect_impact = "<outcome>"   outcome of the thrown/bound item at its throw energy
        expect_range_min = n          throw range in tiles with contents
        min_skill = n                 skill level (1..100) the chain assumes for this step; must reach the action's unlock
"""

from __future__ import annotations

import sys

from common import Db, fail_if, load, load_tests
from derive import (FLAMMABILITY_RANK, ResolveError, apply_modifier, capabilities, display_name,
                    energy_class, impact_outcome, lock_resistance, loot_expected, main_material,
                    mass_estimate, packed_dims, resolve_item, rigidity, search_seconds, soak_portable,
                    soak_rate, throw_range_tiles, total_mass)
from generate import roll_container
from lint import Lint

REQ_OPS = {
    "==": lambda a, b: a == b, "!=": lambda a, b: a != b, ">=": lambda a, b: a >= b,
    "<=": lambda a, b: a <= b, ">": lambda a, b: a > b, "<": lambda a, b: a < b,
    "in": lambda a, b: a in b, "has": lambda a, b: b in a,
}


def get_path(obj: dict, path: str):
    cur = obj
    for p in path.split("."):
        if isinstance(cur, dict) and p in cur:
            cur = cur[p]
        else:
            return None
    return cur


class Ctx:
    def __init__(self, db: Db) -> None:
        self.db = db
        self.cache: dict[str, dict] = {}

    def item(self, iid: str) -> dict:
        if iid not in self.cache:
            self.cache[iid] = resolve_item(self.db, iid)
        return self.cache[iid]

    def caps(self, iid: str) -> dict:
        return capabilities(self.db, self.item(iid))


# ---------------------------------------------------------------- predicates
def param_value(item: dict, path: str):
    v = get_path(item.get("features", {}) or {}, path)
    return v if v is not None else get_path(item, path)


def num(v) -> int:
    if isinstance(v, bool):
        return int(v)
    return v if isinstance(v, (int, float)) else 0


def bounds_ok(v, pred: dict) -> bool:
    if "min" in pred and num(v) < pred["min"]:
        return False
    if "max" in pred and num(v) > pred["max"]:
        return False
    if "equals" in pred and v != pred["equals"]:
        return False
    if "has" in pred and (not isinstance(v, list) or pred["has"] not in v):
        return False
    return True


def eval_pred(ctx: Ctx, pred: dict, rid: str, bind: dict) -> tuple[bool, str]:
    db = ctx.db
    is_sub = db.get("substance", rid) is not None
    if "any" in pred:
        reasons = []
        for group in pred["any"]:
            res = [eval_pred(ctx, p, rid, bind) for p in group]
            if all(ok for ok, _ in res):
                return True, ""
            reasons.append(" & ".join(r for ok, r in res if not ok))
        return False, "none of: " + " | ".join(reasons)
    if "substance" in pred:
        return (is_sub == pred["substance"]), f"'{rid}' substance={is_sub}"
    if "substance_param" in pred:
        s = db.get("substance", rid) or {}
        v = s.get(pred["substance_param"])
        if v is None and "max" in pred:
            return False, f"{rid}.{pred['substance_param']} missing"
        return bounds_ok(v, pred), f"{rid}.{pred['substance_param']}={v}"
    if is_sub:
        return False, f"'{rid}' is a substance, predicate {pred} needs an item"
    it = ctx.item(rid)
    feats = it.get("features", {}) or {}
    if "feature" in pred:
        return pred["feature"] in feats, f"{rid} lacks feature {pred['feature']}"
    if "param" in pred:
        v = param_value(it, pred["param"])
        return v is not None and bounds_ok(v, pred), f"{rid}.{pred['param']}={v}"
    if "capability" in pred:
        v = ctx.caps(rid).get(pred["capability"], 0)
        if "vs" in pred:
            target = bind.get(pred["vs"])
            if target is None:
                return False, f"vs role '{pred['vs']}' unbound"
            need = main_material(db, ctx.item(target)).get("hardness", 0)
            return v >= need, f"{rid} {pred['capability']}={v} < {target} hardness {need}"
        return bounds_ok(v, pred), f"{rid} {pred['capability']}={v}"
    if "prop" in pred:
        v = it.get(pred["prop"], 0)
        return bounds_ok(v, pred), f"{rid}.{pred['prop']}={v}"
    if "shape_in" in pred:
        return it.get("shape") in pred["shape_in"], f"{rid} shape {it.get('shape')}"
    if "tag" in pred:
        return pred["tag"] in it.get("tags", []), f"{rid} lacks tag {pred['tag']}"
    if "tag_prefix" in pred:
        return any(t.startswith(pred["tag_prefix"]) for t in it.get("tags", [])), \
            f"{rid} lacks tag prefix {pred['tag_prefix']}"
    if "has_salvage" in pred:
        has = any("salvage" in p for p in (it.get("composition") or {}).values())
        return has == pred["has_salvage"], f"{rid} salvage={has}"
    if "salvage_yields" in pred:
        ys = [p["salvage"]["item"] for p in (it.get("composition") or {}).values() if "salvage" in p]
        return pred["salvage_yields"] in ys, f"{rid} yields {ys}"
    if "compatible_with" in pred:
        sub_id = bind.get(pred["compatible_with"])
        s = db.get("substance", sub_id or "") or {}
        if not s:
            return False, f"compatible_with: role '{pred['compatible_with']}' not a bound substance"
        cont = feats.get("container") or feats.get("fuel_tank") or {}
        phase = s.get("phase")
        if "compat" in cont and phase not in cont["compat"]:
            return False, f"{rid} container can't hold {phase}"
        mats = [p.get("material") for p in (it.get("composition") or {}).values()]
        bad = [m for m in mats if m in s.get("attacks", [])]
        return not bad, f"{sub_id} attacks {bad} in {rid}"
    if "divisible_mode" in pred:
        mode = (it.get("divisible") or {}).get("mode")
        return mode == pred["divisible_mode"], f"{rid} divisible {mode}"
    if "lock_bashable_by" in pred:
        lock = feats.get("lock") or {}
        tool = bind.get(pred["lock_bashable_by"])
        if tool is None:
            return False, f"lock_bashable_by: role '{pred['lock_bashable_by']}' unbound"
        resist = lock_resistance(lock)
        h = ctx.caps(tool).get("hammer", 0)
        return h >= resist, f"{tool} hammer {h} < {rid} lock resistance {resist} ({lock.get('extra', [])})"
    if "divisible_by_hand" in pred:
        needs = (it.get("divisible") or {}).get("needs", "none")
        return (needs == "none") == pred["divisible_by_hand"], f"{rid} divisible needs '{needs}'"
    if "flammable_min" in pred:
        v = FLAMMABILITY_RANK.get(main_material(db, it).get("flammability", "none"), 0)
        return v >= FLAMMABILITY_RANK[pred["flammable_min"]], f"{rid} flammability rank {v}"
    if "fits_socket_of" in pred:
        base = ctx.item(bind[pred["fits_socket_of"]])
        sock = (base.get("features") or {}).get("socket", {})
        dims = it.get("dims_mm", [0, 0, 0])
        ok = it.get("mass_g", 0) <= sock.get("max_attach_mass_g", 0) and \
            dims[1] <= sock.get("max_attach_size_mm", 0)
        acc = sock.get("accepts")
        if acc and it.get("shape") not in acc:
            ok = False
        return ok, f"{rid} (mass {it.get('mass_g')}, width {dims[1]}, shape {it.get('shape')}) vs socket {sock}"
    if "fits_slot_of" in pred:
        dev = ctx.item(bind[pred["fits_slot_of"]])
        acc = ((dev.get("features") or {}).get("slot") or {}).get("accepts")
        return acc in it.get("tags", []), f"{rid} lacks slot tag {acc}"
    if "param_or_dim_min" in pred:
        v = param_value(it, "tube.length_mm") or it.get("dims_mm", [0])[0]
        return v >= pred["param_or_dim_min"], f"{rid} length {v}"
    if "rigidity_min" in pred:
        v = rigidity(db, it)
        return v >= pred["rigidity_min"], f"{rid} rigidity {v}"
    if "medical" in pred:
        v = (it.get("medical") or {}).get(pred["medical"], 0)
        return bounds_ok(v, pred), f"{rid} medical.{pred['medical']}={v}"
    return False, f"unknown predicate {pred}"


def check_requirement_string(ctx: Ctx, req: str, tool: str | None) -> tuple[bool, str]:
    if req == "none":
        return True, ""
    if tool is None:
        return False, f"requires {req} but no tool bound"
    caps = ctx.caps(tool)
    for part in req.split("&"):
        cap, n = part.strip().split(">=")
        if caps.get(cap, 0) < int(n):
            return False, f"{tool} {cap}={caps.get(cap, 0)} < {n}"
    return True, ""


# ---------------------------------------------------------------- runners
def run_expectations(ctx: Ctx) -> list[str]:
    errors = []
    n = 0
    for f, data in load_tests("expectations"):
        for e in data.get("expect", []):
            n += 1
            if "loot" in e:
                errors += check_loot_expectation(ctx, f.name, e)
                continue
            if "capacity_ml" in e:
                v = search_seconds(e["capacity_ml"]) if e["path"] == "search_seconds" else None
                if v is None or not REQ_OPS[e["op"]](v, e["value"]):
                    errors.append(f"{f.name}: capacity_ml {e['capacity_ml']} {e['path']} {e['op']} "
                                  f"{e['value']!r}: got {v!r}  ({e.get('why', '')})")
                continue
            where = f"{f.name}: {e['item']} {e['path']} {e['op']} {e['value']!r}"
            try:
                it = ctx.item(e["item"])
                for m in e.get("mods", []):
                    it = apply_modifier(ctx.db, it, m)
            except ResolveError as ex:
                errors.append(f"{where}: {ex}")
                continue
            p = e["path"]
            if p.startswith("cap."):
                v = capabilities(ctx.db, it).get(p[4:], 0)
            elif p.startswith("impact."):
                v = impact_outcome(ctx.db, it, p[7:])
            elif p == "throw_range":
                v = throw_range_tiles(ctx.db, it)
            elif p == "name":
                v = display_name(ctx.db, it)
            elif p == "mass_est":
                v = mass_estimate(ctx.db, it)
            elif p == "packed_dims":
                v = packed_dims(ctx.db, it)
            elif p == "packed_long":
                v = packed_dims(ctx.db, it)[0]
            elif p == "compartment_count":
                v = len(it.get("compartments") or [])
            elif p.startswith("compartment."):
                name, _, key = p[len("compartment."):].rpartition(".")
                comp = next((c for c in it.get("compartments") or [] if c.get("name") == name), {})
                v = comp.get(key)
            elif p == "soak_rate":
                v = soak_rate(ctx.db, it)
            elif p == "soak_portable":
                v = soak_portable(ctx.db, it)
            elif p == "search_seconds":
                box = (it.get("features") or {}).get("container")
                v = search_seconds(box["capacity_ml"]) if box else None
            else:
                v = get_path(it, p)
            if v is None or not REQ_OPS[e["op"]](v, e["value"]):
                errors.append(f"{where}: got {v!r}  ({e.get('why', '')})")
    print(f"expectations: {n} checked, {len(errors)} failed")
    return errors


LOOT_PATHS = {"loot_volume_ml": 0, "loot_mass_g": 1}


def check_loot_expectation(ctx: Ctx, fname: str, e: dict) -> list[str]:
    where = f"{fname}: loot {e['loot']} {e['path']} {e['op']} {e['value']!r}"
    if e["path"] not in LOOT_PATHS:
        return [f"{where}: unknown loot path"]
    try:
        v = loot_expected(ctx.db, e["loot"])[LOOT_PATHS[e["path"]]]
    except ResolveError as ex:
        return [f"{where}: {ex}"]
    return [] if REQ_OPS[e["op"]](v, e["value"]) else [f"{where}: got {v!r}  ({e.get('why', '')})"]


# A kitchen base run shrunk to 1 L must fail the fit rule, naming its cupboard's dish
# table (PEO-052; runs since PEO-054; purposes since PEO-057).
FIT_PROBE_ITEM = "kitchen_base_run"
FIT_PROBE_CAPACITY_ML = 1000
FIT_PROBE_EXPECT = "dish_cupboard_items: expected"


def run_fit_probe(db: Db) -> list[str]:
    rec = db.get("item", FIT_PROBE_ITEM)
    # A single container, or compartments (PEO-054): shrink whichever it has.
    boxes = rec["compartments"] if "compartments" in rec else [rec["features"]["container"]]
    saved = [b["capacity_ml"] for b in boxes]
    for b in boxes:
        b["capacity_ml"] = FIT_PROBE_CAPACITY_ML // len(boxes)
    try:
        lint = Lint(db)
        lint.lint_items()  # a compartment's default loot is checked on the item
        lint.lint_world()
    finally:
        for b, s in zip(boxes, saved):
            b["capacity_ml"] = s
    hits = [e for e in lint.errors if FIT_PROBE_EXPECT in e and "exceeds capacity_ml" in e]
    print(f"fit probe: {FIT_PROBE_ITEM} at {FIT_PROBE_CAPACITY_ML} ml -> {len(hits)} fit error(s)")
    return [] if hits else [f"fit probe: no '{FIT_PROBE_EXPECT} ... exceeds capacity_ml' error"]


# A kitchen shrunk to 1 m2 must fail the room fit rule (PEO-054).
ROOM_PROBE = "kitchen"
ROOM_PROBE_SIZE_M2 = [1, 1]


def run_room_probe(db: Db) -> list[str]:
    rec = db.get("room", ROOM_PROBE)
    saved = rec["size_m2"]
    rec["size_m2"] = ROOM_PROBE_SIZE_M2
    try:
        lint = Lint(db)
        lint.lint_world()
    finally:
        rec["size_m2"] = saved
    hits = [e for e in lint.errors if f"room.{ROOM_PROBE}:" in e and "room fit:" in e]
    print(f"room probe: {ROOM_PROBE} at {ROOM_PROBE_SIZE_M2} m2 -> {len(hits)} room fit error(s)")
    return [] if hits else [f"room probe: no 'room fit' error for room.{ROOM_PROBE}"]


# Extras tables lint must refuse (PEO-057): an unregistered context, and a detail key
# (a detail blends into the main fill; it has no may-appear table).
EXTRAS_PROBES = {
    "unregistered ctx": ({"kind": "kind.junk_drawer", "ctx": "ctx.moon_base"}, "ctx.moon_base"),
    "detail key": ({"kind": "kind.junk_drawer", "detail": "detail.smoker"}, "key 'detail'"),
}


def run_extras_probes(db: Db) -> list[str]:
    errors = []
    for label, (match, expect) in EXTRAS_PROBES.items():
        db["extras"]["probe"] = {"match": match, "chance_pct": 10, "rolls": [1, 1],
                                 "entries": [{"item": "cash", "w": 1, "count": [1, 1]}], "why": "probe"}
        try:
            lint = Lint(db)
            lint.lint_d030()
        finally:
            del db["extras"]["probe"]
        hits = [e for e in lint.errors if "extras.probe" in e and expect in e]
        print(f"extras probe: {label} -> {len(hits)} error(s)")
        if not hits:
            errors.append(f"extras probe: {label} was not refused")
    return errors


# ---------------------------------------------------------------- rolls (D-030, PEO-057)
ROLL_SEEDS = range(1, 201)
SMOKER = "detail.smoker"
SMOKER_ITEMS = {"cigarettes", "lighter", "matchbook"}
SMOKER_EVIDENCE_LINES = 4
MIN_BLEND_SHARE = 0.40
PREFIX_TURNS = 6


def compartment(db: Db, item: str, name: str, purposes: list[dict] | None = None) -> dict:
    comp = dict(next(c for c in resolve_item(db, item)["compartments"] if c["name"] == name))
    if purposes is not None:
        comp["purposes"] = purposes
    return comp


def fill(r: dict) -> list[tuple]:
    """Main and detail lines, as comparable tuples (extras left out)."""
    return [(l["item"], l["count"], l["source"]) for l in r["lines"] if l["source"] != "extras"]


def run_rolls(db: Db) -> list[str]:
    errors: list[str] = []
    n = 0

    def check(ok: bool, what: str) -> None:
        nonlocal n
        n += 1
        if not ok:
            errors.append(f"rolls: {what}")

    drawer = compartment(db, "kitchen_base_run", "top drawer")
    junk = compartment(db, "kitchen_base_run", "top drawer", [{"purpose": "junk_drawer", "w": 1}])
    maint = compartment(db, "kitchen_cabinet", "cupboard", [{"purpose": "maintenance_cabinet", "w": 1}])

    for f, data in load_tests("rolls"):
        for case in data.get("roll", []):
            comp = compartment(db, case["item"], case["compartment"])
            r = roll_container(db, case["seed"], comp, case["ctx"], case.get("detail"))
            got = [[l["item"], l["count"], l["source"]] for l in r["lines"]]
            check(r["purpose"] == case["purpose"] and got == case["lines"],
                  f"{f.name}: seed {case['seed']} rolled {r['purpose']} {got}")

    names = {db.get("purpose", e["purpose"])["name"] for e in drawer["purposes"]}
    seen_names = set()
    for s in ROLL_SEEDS:
        a = roll_container(db, s, drawer, "ctx.home")
        check(a == roll_container(db, s, drawer, "ctx.home"), f"seed {s} is not deterministic")
        check(a["name"] in names, f"seed {s} named its drawer {a['name']!r}")
        seen_names.add(a["name"])
        for detail in (None, SMOKER):
            longer = fill(roll_container(db, s, junk, "ctx.home", detail, turns=PREFIX_TURNS))
            for k in range(1, PREFIX_TURNS):
                shorter = fill(roll_container(db, s, junk, "ctx.home", detail, turns=k))
                check(longer[:len(shorter)] == shorter, f"seed {s} detail {detail} turns {k} is not a prefix")
    check(len(seen_names) > 1, "a kitchen drawer always rolled the same purpose")

    extras_differ = 0
    for s in ROLL_SEEDS:
        office = roll_container(db, s, maint, "ctx.office")
        retail = roll_container(db, s, maint, "ctx.retail")
        check(office["purpose"] == retail["purpose"] and fill(office) == fill(retail),
              f"seed {s}: context changed the main fill")
        extras_differ += office["lines"] != retail["lines"]
    check(extras_differ > 0, "context never changed a maintenance cabinet's extras")

    blend = total = 0
    for s in ROLL_SEEDS:
        plain = fill(roll_container(db, s, junk, "ctx.home", turns=PREFIX_TURNS))
        smoked = roll_container(db, s, junk, "ctx.home", SMOKER)
        lines = fill(smoked)
        mains = [l for l in lines if l[2] == "main"]
        check(mains == plain[:len(mains)], f"seed {s}: the smoker's main lines are not the plain roll's first picks")
        check(any(l[0] in SMOKER_ITEMS for l in lines[:SMOKER_EVIDENCE_LINES]),
              f"seed {s}: no smoker item in the first {SMOKER_EVIDENCE_LINES} lines")
        blend += sum(l[2] == "detail" for l in lines)
        total += len(lines)
    check(total > 0 and blend / total >= MIN_BLEND_SHARE,
          f"the smoker's blend is {blend}/{total} of its fill, under {MIN_BLEND_SHARE:.0%}")
    print(f"rolls: {n} checked, {len(errors)} failed (smoker blend {blend}/{total})")
    return errors


def run_chains(ctx: Ctx) -> list[str]:
    db = ctx.db
    errors = []
    nchains = nsteps = 0
    for f, data in load_tests("chains"):
        for cid, chain in (data.get("chain") or {}).items():
            nchains += 1
            outer, errors = errors, []
            for i, step in enumerate(chain.get("steps", []), 1):
                nsteps += 1
                where = f"{f.name} chain.{cid} step {i} ({step.get('action')})"
                act = db.get("action", step.get("action", ""))
                if act is None:
                    errors.append(f"{where}: unknown action")
                    continue
                bind = step.get("bind", {})
                for rid in bind.values():
                    if db.get("item", rid) is None and db.get("substance", rid) is None:
                        errors.append(f"{where}: bound id '{rid}' is not an item or substance")
                if any(e.startswith(where) for e in errors):
                    continue
                if act.get("unlock", 0) > step.get("min_skill", 0):
                    errors.append(f"{where}: action needs skill {act['skill']} >= {act['unlock']}; "
                                  f"chain step must declare min_skill")
                for role, rdef in (act.get("roles") or {}).items():
                    if role not in bind:
                        if not rdef.get("optional"):
                            errors.append(f"{where}: role '{role}' not bound")
                        continue
                    for pred in rdef.get("need", []):
                        try:
                            ok, why = eval_pred(ctx, pred, bind[role], bind)
                        except (ResolveError, KeyError) as ex:
                            ok, why = False, f"error {ex}"
                        if not ok:
                            errors.append(f"{where}: role {role}={bind[role]} fails {pred}: {why}")
                if "yields" in step:
                    tgt = ctx.item(bind["target"])
                    found = [p["salvage"] for p in (tgt.get("composition") or {}).values()
                             if p.get("salvage", {}).get("item") == step["yields"]]
                    if not found:
                        errors.append(f"{where}: {bind['target']} does not yield {step['yields']}")
                    else:
                        ok, why = check_requirement_string(ctx, found[0].get("tool", "none"), bind.get("tool"))
                        if not ok:
                            errors.append(f"{where}: salvage tool check: {why}")
                if "fuel" in step:
                    src = ctx.item(bind["source"])
                    tank = (src.get("features") or {}).get("fuel_tank") or {}
                    if step["fuel"] not in tank.get("fuel", []):
                        cont = src.get("contents", {}).get("substance")
                        if cont != step["fuel"]:
                            errors.append(f"{where}: {bind['source']} does not hold {step['fuel']}")
                if "expect_impact" in step or "expect_range_min" in step:
                    it = ctx.item(bind["item"])
                    c = step.get("contents") or {}
                    m = total_mass(db, it, c.get("ml", 0), c.get("substance"))
                    if "expect_impact" in step:
                        out = impact_outcome(db, it, energy_class(m))
                        if out != step["expect_impact"]:
                            errors.append(f"{where}: impact at {energy_class(m)} energy "
                                          f"(mass {m} g) = {out}, expected {step['expect_impact']}")
                    if "expect_range_min" in step:
                        r = throw_range_tiles(db, it, m)
                        if r < step["expect_range_min"]:
                            errors.append(f"{where}: throw range {r} < {step['expect_range_min']}")
            if chain.get("expect_fail"):
                if not errors:
                    outer.append(f"{f.name} chain.{cid}: negative control unexpectedly feasible")
                errors = outer
            else:
                errors = outer + errors
    print(f"chains: {nchains} chains / {nsteps} steps checked, {len(errors)} failed")
    return errors


def main() -> int:
    db = load()
    if db.errors:
        return fail_if(db.errors, "load")
    ctx = Ctx(db)
    errors = (run_expectations(ctx) + run_chains(ctx) + run_fit_probe(db) + run_room_probe(db)
              + run_extras_probes(db) + run_rolls(db))
    return fail_if(errors, "content tests")


if __name__ == "__main__":
    sys.exit(main())
