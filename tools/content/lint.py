#!/usr/bin/env python3
"""Content linter: schema, vocabulary and reference checks for content/ (N008, N011).

Errors fail the run; warnings are printed (use --strict to fail on warnings too).
"""

from __future__ import annotations

import argparse
import re
import sys

from common import Db, load
from derive import (ResolveError, entry_item, extras_expected, item_expected, loot_expected, loot_items,
                    mass_estimate, packed_dims, resolve_item, rnd)

REQ_RE = re.compile(r"^([a-z_]+)>=(\d+)$")
# Purposes with no may-appear table of their own, and why (D-030 wants one per kind or a reason).
KINDS_WITHOUT_EXTRAS: dict[str, str] = {}
EXTRAS_MATCH_KEYS = {"kind": "kind", "ctx": "ctx"}
# cat families that label records other than items (buildings, settings): registered, but
# never queried against items (PEO-056).
CAT_LABEL_FAMILIES = ("building", "setting")
# Concrete items no content references because the building generator places them.
REACHABLE_BY_GENERATOR = {
    "door_interior": "the building generator hangs interior doors",
    "door_exterior": "the building generator hangs exterior doors",
    "door_steel": "the building generator hangs steel doors (commercial, institutional)",
    "window_house": "the building generator glazes house walls",
    "window_storefront": "the building generator glazes shop fronts",
}
# Keys a compartment may carry (PEO-054) and their types; lock is checked against feature.lock.
COMPARTMENT_KEYS = {"name": "str", "capacity_ml": "int", "max_mass_g": "int", "max_dim_mm": "int",
                    "closable": "bool", "lock": "table", "purposes": "list<table>"}
COMPARTMENT_REQUIRED = ("name", "capacity_ml")
# Room fit (PEO-054, Q2 C): furniture may cover at most this share of a room's floor, so
# people can walk between it. The generator draws counts in proportion to the rolled
# area, so both ends of the size range must fit.
ROOM_FILL_MAX = 0.6
MM2_PER_M2 = 1_000_000


def combined_box(comps: list[dict]) -> dict:
    """An item's compartments as one container for a table rolled once and spread over
    them by fit (PEO-054, Q1 B): the capacities summed, the mass limits summed when every
    compartment has one, and the largest longest side any compartment takes."""
    box = {"capacity_ml": sum(c.get("capacity_ml", 0) for c in comps)}
    if comps and all("max_mass_g" in c for c in comps):
        box["max_mass_g"] = sum(c["max_mass_g"] for c in comps)
    if comps and all("max_dim_mm" in c for c in comps):
        box["max_dim_mm"] = max(c["max_dim_mm"] for c in comps)
    return box


class Lint:
    def __init__(self, db: Db) -> None:
        self.db = db
        self.errors: list[str] = list(db.errors)
        self.warnings: list[str] = []

    def err(self, where: str, msg: str) -> None:
        self.errors.append(f"{where}: {msg}")

    def warn(self, where: str, msg: str) -> None:
        self.warnings.append(f"{where}: {msg}")

    # ---------------------------------------------------------------- type checks
    def check_type(self, where: str, key: str, value, tstr: str) -> None:
        ok = True
        if tstr == "int":
            ok = isinstance(value, int) and not isinstance(value, bool)
        elif tstr == "bool":
            ok = isinstance(value, bool)
        elif tstr == "str":
            ok = isinstance(value, str)
        elif tstr == "int[3]":
            ok = isinstance(value, list) and len(value) == 3 and all(isinstance(x, int) for x in value)
        elif tstr == "range":
            ok = isinstance(value, list) and len(value) == 2 and all(isinstance(x, int) for x in value)
        elif tstr == "table":
            ok = isinstance(value, dict)
        elif tstr == "any":
            ok = True
        elif tstr.startswith("list<"):
            inner = tstr[5:-1]
            ok = isinstance(value, list)
            if ok and inner.startswith("enum:"):
                allowed = inner[5:].split("|")
                bad = [x for x in value if x not in allowed]
                if bad:
                    self.err(where, f"{key}: values {bad} not in {allowed}")
            elif ok and inner == "table":
                ok = all(isinstance(x, dict) for x in value)
            elif ok and inner == "str":
                ok = all(isinstance(x, str) for x in value)
        elif tstr.startswith("enum:"):
            allowed = tstr[5:].split("|")
            if value not in allowed:
                self.err(where, f"{key}: '{value}' not in {allowed}")
            return
        elif tstr.startswith("ref:"):
            rtype = tstr[4:]
            if self.db.get(rtype, value) is None:
                self.err(where, f"{key}: unknown {rtype} '{value}'")
            return
        if not ok:
            self.err(where, f"{key}: expected {tstr}, got {value!r}")

    def check_schema(self, where: str, rec: dict, schema: dict) -> None:
        fields = schema.get("fields", {})
        for req in schema.get("required", []):
            if req not in rec:
                self.err(where, f"missing required field '{req}'")
        for k, v in rec.items():
            if k not in fields:
                self.err(where, f"unknown field '{k}'")
            else:
                self.check_type(where, k, v, fields[k])

    # ---------------------------------------------------------------- vocabulary
    def check_tag(self, where: str, tag: str) -> None:
        if "." not in tag:
            self.err(where, f"tag '{tag}' has no namespace")
            return
        ns, rest = tag.split(".", 1)
        nsrec = self.db.get("namespace", ns)
        if nsrec is None:
            self.err(where, f"tag '{tag}': unknown namespace '{ns}'")
            return
        if ns == "emits":
            ch = rest.split(".")[0]
            if self.db.get("scent", ch) is None:
                self.err(where, f"tag '{tag}': unknown scent channel '{ch}'")
        elif ns == "brand":
            if self.db["brand"] and self.db.get("brand", rest) is None:
                self.err(where, f"tag '{tag}': unknown brand '{rest}'")
        elif ns == "mat":
            fams = self.db["schema"]["material"]["fields"]["family"][5:].split("|")
            if rest not in fams:
                self.err(where, f"tag '{tag}': unknown material family")
        elif ns == "kind":
            if self.db.get("purpose", rest) is None:
                self.err(where, f"tag '{tag}': no purpose '{rest}'")
        elif ns == "detail":
            if self.db.get("detail", rest) is None:
                self.err(where, f"tag '{tag}': no detail '{rest}'")
        elif ns == "cat":
            if self.db.get("tag", "cat." + rest.split(".")[0]) is None:
                self.err(where, f"tag '{tag}': cat family 'cat.{rest.split('.')[0]}' is not registered")
        elif not nsrec.get("open") and self.db.get("tag", tag) is None:
            self.err(where, f"tag '{tag}' is not registered (registry/tags.toml)")

    def check_req_string(self, where: str, s: str) -> None:
        if s == "none":
            return
        for part in s.split("&"):
            m = REQ_RE.match(part.strip())
            if not m:
                self.err(where, f"bad requirement string '{s}' (want 'cap>=n' joined by &)")
            elif self.db.get("capability", m.group(1)) is None:
                self.err(where, f"requirement '{s}': unknown capability '{m.group(1)}'")

    # ---------------------------------------------------------------- registries
    def lint_registry(self) -> None:
        for rtype in ("property", "feature", "capability"):
            for rid, rec in self.db[rtype].items():
                w = f"{self.db.where(rtype, rid)} {rtype}.{rid}"
                if not rec.get("desc"):
                    self.err(w, "missing desc")
                if not rec.get("readers") and not rec.get("flavor"):
                    self.err(w, "missing readers (detail budget: who consumes this?)")
        for fid, rec in self.db["fastener"].items():
            w = f"{self.db.where('fastener', fid)} fastener.{fid}"
            self.check_req_string(w, rec.get("tools", "none"))
        for aid, rec in self.db["action"].items():
            w = f"{self.db.where('action', aid)} action.{aid}"
            if self.db.get("skill", rec.get("skill", "")) is None:
                self.err(w, f"unknown skill '{rec.get('skill')}'")
            for k in ("desc", "mechanics", "difficulty", "time_s", "noise"):
                if k not in rec:
                    self.err(w, f"missing '{k}'")
            for role, rdef in (rec.get("roles") or {}).items():
                for pred in rdef.get("need", []):
                    self.check_predicate(f"{w} role {role}", pred)

    KNOWN_PREDICATES = {
        "feature", "param", "capability", "prop", "shape_in", "tag", "tag_prefix", "has_salvage",
        "salvage_yields", "substance", "substance_param", "holds_substance_of", "compatible_with",
        "divisible_mode", "divisible_by_hand", "lock_bashable_by", "flammable_min", "fits_socket_of", "fits_slot_of", "param_or_dim_min",
        "rigidity_min", "medical", "any",
    }

    def check_predicate(self, where: str, pred: dict) -> None:
        keys = set(pred) & self.KNOWN_PREDICATES
        if not keys:
            self.err(where, f"unknown predicate {pred}")
            return
        if "any" in pred:
            for group in pred["any"]:
                for p in group:
                    self.check_predicate(where, p)
        if "feature" in pred and self.db.get("feature", pred["feature"]) is None:
            self.err(where, f"unknown feature '{pred['feature']}'")
        if "capability" in pred and self.db.get("capability", pred["capability"]) is None:
            self.err(where, f"unknown capability '{pred['capability']}'")
        for t in ("tag",):
            if t in pred:
                self.check_tag(where, pred[t])

    # ---------------------------------------------------------------- materials etc
    def lint_simple(self, rtype: str) -> None:
        schema = self.db.get("schema", rtype)
        for rid, rec in self.db[rtype].items():
            w = f"{self.db.where(rtype, rid)} {rtype}.{rid}"
            self.check_schema(w, rec, schema)
            if rtype == "substance":
                for m in rec.get("attacks", []):
                    if self.db.get("material", m) is None:
                        self.err(w, f"attacks unknown material '{m}'")
                sc = rec.get("scent")
                if sc and self.db.get("scent", sc.get("channel", "")) is None:
                    self.err(w, f"unknown scent channel {sc}")
            if rtype == "material":
                for q in ("hardness", "rigidity", "edge_holding"):
                    if q in rec and not 0 <= rec[q] <= 10:
                        self.err(w, f"{q} out of 0..10")

    # ---------------------------------------------------------------- items
    def lint_items(self) -> None:
        props = self.db["property"]
        for iid, raw in self.db["item"].items():
            w = f"{self.db.where('item', iid)} item.{iid}"
            for k in raw:
                if k not in props:
                    self.err(w, f"unknown property '{k}' (register it in registry/properties.toml)")
            try:
                item = resolve_item(self.db, iid)
            except ResolveError as e:
                self.err(w, str(e))
                continue
            for k, v in item.items():
                if k in props and props[k]["type"] not in ("table",):
                    self.check_type(w, k, v, props[k]["type"])
            if item.get("abstract"):
                continue
            for req in ("name", "shape", "dims_mm", "mass_g", "why", "composition", "glyph"):
                if req not in item:
                    self.err(w, f"missing '{req}'")
            dims = item.get("dims_mm")
            if isinstance(dims, list) and len(dims) == 3 and dims != sorted(dims, reverse=True):
                self.warn(w, f"dims_mm should be longest-first: {dims}")
            for t in item.get("tags", []):
                self.check_tag(w, t)
            if not any(t.startswith("cat.") for t in item.get("tags", [])):
                self.err(w, "needs a cat.* tag (loot tables, UI)")
            self.lint_composition(w, item)
            self.lint_features(w, item)
            self.lint_compartments(w, item)
            for cap in (item.get("capabilities") or {}):
                if self.db.get("capability", cap) is None:
                    self.err(w, f"unknown capability '{cap}'")
            for stim, outs in (item.get("reactions") or {}).items():
                st = self.db.get("stimulus", stim)
                if st is None:
                    self.err(w, f"unknown stimulus '{stim}'")
                    continue
                for e, o in outs.items():
                    if e not in ("low", "medium", "high") or o not in st["outcomes"]:
                        self.err(w, f"reaction {stim}.{e}='{o}' invalid")
            for m in item.get("modifiers", []):
                if self.db.get("modifier", m) is None:
                    self.err(w, f"unknown modifier '{m}'")
            for d in item.get("details", []):
                if self.db.get("detail_table", d) is None:
                    self.err(w, f"unknown detail table '{d}'")
            cont = item.get("contents") or {}
            if "substance" in cont and self.db.get("substance", cont["substance"]) is None:
                self.err(w, f"contents: unknown substance '{cont['substance']}'")
            for ci in cont.get("items", []):
                if self.db.get("item", ci) is None and self.db.get("loot", ci) is None:
                    self.err(w, f"contents: unknown item/loot '{ci}'")
            teach = item.get("teaches")
            if teach and self.db.get("skill", teach.get("skill", "")) is None:
                self.err(w, f"teaches unknown skill '{teach.get('skill')}'")
            est = mass_estimate(self.db, item)
            m = item.get("mass_g", 0)
            if est and m and not (est / 4 <= m <= est * 4):
                self.warn(w, f"mass_g {m} vs estimate ~{est} from dims x density (check numbers)")

    def lint_composition(self, w: str, item: dict) -> None:
        comp = item.get("composition") or {}
        total = 0
        for pname, p in comp.items():
            if self.db.get("material", p.get("material", "")) is None:
                self.err(w, f"part '{pname}': unknown material '{p.get('material')}'")
            total += p.get("share", 0)
            s = p.get("salvage")
            if s:
                if self.db.get("item", s.get("item", "")) is None:
                    self.err(w, f"part '{pname}': salvage yields unknown item '{s.get('item')}'")
                self.check_req_string(f"{w} part {pname}", s.get("tool", "none"))
        if comp and total and not 90 <= total <= 110:
            self.warn(w, f"composition shares sum to {total}%")

    def lint_features(self, w: str, item: dict) -> None:
        for fid, params in (item.get("features") or {}).items():
            fdef = self.db.get("feature", fid)
            if fdef is None:
                self.err(w, f"unknown feature '{fid}'")
                continue
            for req in fdef.get("required", []):
                if req not in params:
                    self.err(w, f"feature {fid}: missing '{req}'")
            for k, v in params.items():
                if k not in fdef["params"]:
                    self.err(w, f"feature {fid}: unknown param '{k}'")
                else:
                    self.check_type(f"{w} feature {fid}", k, v, fdef["params"][k])
            if fid == "wearable":
                if self.db.get("tag", f"slot.{params.get('slot')}") is None:
                    self.err(w, f"wearable: unknown slot '{params.get('slot')}'")
                for b in params.get("coverage", []):
                    if self.db.get("tag", f"body.{b}") is None:
                        self.err(w, f"wearable: unknown body part '{b}'")
            if fid == "fuel_tank":
                for s in params.get("fuel", []):
                    if self.db.get("substance", s) is None:
                        self.err(w, f"fuel_tank: unknown substance '{s}'")

    def lint_compartments(self, w: str, item: dict) -> None:
        comps = item.get("compartments")
        if comps is None:
            return
        if "container" in (item.get("features") or {}):
            self.err(w, "has both compartments and features.container; use one")
        names = [c.get("name") for c in comps]
        for n in sorted({n for n in names if names.count(n) > 1}):
            self.err(w, f"compartment name '{n}' is not unique")
        lock_def = self.db.get("feature", "lock")
        for i, c in enumerate(comps):
            cw = f"{w} compartment {c.get('name', i)}"
            for req in COMPARTMENT_REQUIRED:
                if req not in c:
                    self.err(cw, f"missing '{req}'")
            for k, v in c.items():
                if k not in COMPARTMENT_KEYS:
                    self.err(cw, f"unknown key '{k}'")
                else:
                    self.check_type(cw, k, v, COMPARTMENT_KEYS[k])
            lock = c.get("lock")
            if isinstance(lock, dict):
                for req in lock_def.get("required", []):
                    if req not in lock:
                        self.err(cw, f"lock: missing '{req}'")
                for k, v in lock.items():
                    if k not in lock_def["params"]:
                        self.err(cw, f"lock: unknown param '{k}'")
                    else:
                        self.check_type(f"{cw} lock", k, v, lock_def["params"][k])
            if "purposes" in c:
                self.lint_purposes(cw, c, c["purposes"])

    def lint_purposes(self, w: str, comp: dict, purposes) -> None:
        """A weighted list of purposes (D-030): each names a purpose, with a positive
        weight, and each purpose's fill fits the compartment on its own."""
        if not isinstance(purposes, list) or not purposes:
            self.err(w, "purposes: expected a non-empty list of { purpose, w }")
            return
        for e in purposes:
            pid = e.get("purpose") if isinstance(e, dict) else None
            if not isinstance(e, dict) or set(e) != {"purpose", "w"} or not isinstance(e.get("w"), int) or e["w"] <= 0:
                self.err(w, f"purposes: entry {e!r} must be {{ purpose, w > 0 }}")
                continue
            rec = self.db.get("purpose", pid)
            if rec is None:
                self.err(w, f"purposes: unknown purpose '{pid}'")
                continue
            self.check_box(f"{w} purpose {pid}", comp, self.purpose_fill(pid), extras=self.kind_extras(pid))

    def purpose_fill(self, pid: str) -> list[str]:
        """What a purpose puts in a compartment on average: its main table (the kind's
        extras are added to the volume by check_box's extras argument)."""
        return [self.db.get("purpose", pid)["loot"]]

    def kind_extras(self, pid: str) -> list[str]:
        """Extras a kind.<pid> container can meet at once: the tables matching its kind in
        any context, plus those of the one context that adds the most (a container has
        exactly one context)."""
        tables = [(xid, rec.get("match") or {}) for xid, rec in self.db["extras"].items()
                  if (rec.get("match") or {}).get("kind") == f"kind.{pid}"]
        any_ctx = [xid for xid, m in tables if "ctx" not in m]
        by_ctx: dict[str, list[str]] = {}
        for xid, m in tables:
            if "ctx" in m:
                by_ctx.setdefault(m["ctx"], []).append(xid)

        def volume(ids: list[str]) -> int:
            return sum(extras_expected(self.db, x)[0] for x in ids)

        worst = max(by_ctx.values(), key=volume, default=[])
        return any_ctx + worst

    def lint_d030(self) -> None:
        """Purposes, details, detail_loot and extras (D-030, PEO-057)."""
        for xid, rec in self.db["extras"].items():
            w = f"{self.db.where('extras', xid)} extras.{xid}"
            match = rec.get("match") or {}
            if not match:
                self.err(w, "match: needs kind or ctx")
            for k, v in match.items():
                if k not in EXTRAS_MATCH_KEYS:
                    self.err(w, f"match: key '{k}' is not kind or ctx (a detail blends, it has no extras)")
                elif not isinstance(v, str) or not v.startswith(EXTRAS_MATCH_KEYS[k] + "."):
                    self.err(w, f"match: {k} must be a {EXTRAS_MATCH_KEYS[k]}.* tag, got {v!r}")
                else:
                    self.check_tag(w, v)
            if not 0 < rec.get("chance_pct", 0) <= 100:
                self.err(w, "chance_pct must be 1..100")
            for i, e in enumerate(rec.get("entries", [])):
                ref = e.get("item") or e.get("loot")
                if not ref:
                    self.err(w, f"entry {i}: needs item or loot")
                else:
                    self.item_or_loot(w, ref)
                for m in e.get("mods", []):
                    if self.db.get("modifier", m) is None:
                        self.err(w, f"entry {i}: unknown modifier '{m}'")
        seen: set[tuple[str, str]] = set()
        for lid, rec in self.db["detail_loot"].items():
            w = f"{self.db.where('detail_loot', lid)} detail_loot.{lid}"
            match = rec.get("match") or {}
            if set(match) != {"detail", "kind"}:
                self.err(w, "match: needs exactly detail and kind")
                continue
            for k in ("detail", "kind"):
                self.check_tag(w, match[k])
            key = (match["detail"], match["kind"])
            if key in seen:
                self.err(w, f"a second detail_loot for {key[0]} on {key[1]}")
            seen.add(key)
            kind = match["kind"].split(".", 1)[1]
            self.check_blend(w, rec.get("loot", ""), self.kind_compartments(kind))
        every = [c for comps in self.all_compartments().values() for c in comps]
        for did, rec in self.db["detail"].items():
            self.check_blend(f"{self.db.where('detail', did)} detail.{did}", rec.get("loot", ""), every)
        for pid in self.db["purpose"]:
            if not self.kind_extras(pid) and pid not in KINDS_WITHOUT_EXTRAS:
                self.err(f"{self.db.where('purpose', pid)} purpose.{pid}",
                         "no extras table matches this kind; add one or list it in KINDS_WITHOUT_EXTRAS")

    def all_compartments(self) -> dict[str, list[dict]]:
        """item id -> compartments, for items that have them."""
        out: dict[str, list[dict]] = {}
        for iid in self.db["item"]:
            try:
                comps = resolve_item(self.db, iid).get("compartments")
            except ResolveError:
                continue
            if comps:
                out[iid] = comps
        return out

    def kind_compartments(self, pid: str) -> list[dict]:
        """Compartments that can roll purpose pid, by default or by a room override."""
        comps = self.all_compartments()
        out = [c for cs in comps.values() for c in cs
               if any(e.get("purpose") == pid for e in c.get("purposes", []))]
        for rec in self.db["room"].values():
            for fx in rec.get("objects", []):
                for name, plist in (fx.get("purposes") or {}).items():
                    if any(e.get("purpose") == pid for e in plist):
                        out += [c for c in comps.get(fx.get("item", ""), []) if c.get("name") == name]
        return out

    def check_blend(self, w: str, loot_id: str, comps: list[dict]) -> None:
        """A blend table exists and at least one of its items fits one of `comps`."""
        if self.db.get("loot", loot_id) is None:
            self.err(w, f"loot: unknown loot '{loot_id}'")
            return
        if not comps:
            self.err(w, "no compartment it could be blended into")
            return
        for _, e in loot_items(self.db, loot_id):
            try:
                longest = packed_dims(self.db, entry_item(self.db, e))[0]
            except ResolveError:
                continue
            if any(longest <= c.get("max_dim_mm", longest) for c in comps):
                return
        self.err(w, f"loot {loot_id}: no item fits any compartment it is authored for")

    # ---------------------------------------------------------------- modifiers
    def lint_modifiers(self) -> None:
        for mid, rec in self.db["modifier"].items():
            w = f"{self.db.where('modifier', mid)} modifier.{mid}"
            self.check_schema(w, rec, self.db.get("schema", "modifier"))
            for t in rec.get("add_tags", []):
                self.check_tag(w, t)
            self.lint_applies(w, rec.get("applies_to", {}))
            self.lint_ops(w, rec.get("ops", []))
        for did, rec in self.db["detail_table"].items():
            w = f"{self.db.where('detail_table', did)} detail_table.{did}"
            self.check_schema(w, rec, self.db.get("schema", "detail_table"))
            self.lint_applies(w, rec.get("applies_to", {}))
            for i, e in enumerate(rec.get("entries", [])):
                if "text" not in e and "modifier" not in e:
                    self.err(w, f"entry {i}: needs text or modifier")
                if "modifier" in e and self.db.get("modifier", e["modifier"]) is None:
                    self.err(w, f"entry {i}: unknown modifier '{e['modifier']}'")
                for t in e.get("add_tags", []):
                    self.check_tag(w, t)
                self.lint_ops(f"{w} entry {i}", e.get("ops", []))

    def lint_applies(self, w: str, a: dict) -> None:
        for k, v in a.items():
            if k == "items":
                for i in v:
                    if self.db.get("item", i) is None:
                        self.err(w, f"applies_to unknown item '{i}'")
            elif k in ("tags_any", "tags_all"):
                for t in v:
                    if not t.startswith(("cat.", "mat.", "brand.")):
                        self.check_tag(w, t)
            elif k == "shapes":
                for s in v:
                    if self.db.get("shape", s) is None:
                        self.err(w, f"applies_to unknown shape '{s}'")
            else:
                self.err(w, f"applies_to: unknown key '{k}'")

    def lint_ops(self, w: str, ops: list) -> None:
        for op in ops:
            if op.get("op") not in ("set", "add", "mul", "append", "remove") or "path" not in op:
                self.err(w, f"bad op {op}")
            else:
                root = op["path"].split(".")[0]
                if root not in self.db["property"]:
                    self.err(w, f"op path root '{root}' is not a registered property")

    # ---------------------------------------------------------------- brands & world
    def lint_refs(self) -> None:
        for rtype in ("company", "brand", "store_chain", "setting", "building", "room", "loot",
                      "outdoor_set", "profile", "purpose", "detail", "detail_loot", "extras"):
            schema = self.db.get("schema", rtype)
            for rid, rec in self.db[rtype].items():
                w = f"{self.db.where(rtype, rid)} {rtype}.{rid}"
                self.check_schema(w, rec, schema)
                if rtype == "store_chain" and self.db["building"] and \
                        self.db.get("building", rec.get("type", "")) is None:
                    self.err(w, f"type: unknown building '{rec.get('type')}'")
        cats = {c for b in self.db["brand"].values() for c in b.get("categories", [])}
        for iid, rec in self.db["item"].items():
            for c in rec.get("brand_cats", []):
                if c not in cats:
                    self.err(f"{self.db.where('item', iid)} item.{iid}",
                             f"brand_cats: no brand covers category '{c}' (content/brands)")
        self.lint_world()

    def item_or_loot(self, w: str, ref: str) -> None:
        if self.db.get("item", ref) is None and self.db.get("loot", ref) is None:
            self.err(w, f"unknown item/loot '{ref}'")

    def lint_world(self) -> None:
        for lid, rec in self.db["loot"].items():
            w = f"{self.db.where('loot', lid)} loot.{lid}"
            for i, e in enumerate(rec.get("entries", [])):
                ref = e.get("item") or e.get("loot")
                if not ref:
                    self.err(w, f"entry {i}: needs item or loot")
                    continue
                self.item_or_loot(w, ref)
                for m in e.get("mods", []):
                    if self.db.get("modifier", m) is None:
                        self.err(w, f"entry {i}: unknown modifier '{m}'")
                if "substance" in e and self.db.get("substance", e["substance"]) is None:
                    self.err(w, f"entry {i}: unknown substance '{e['substance']}'")
        for rid, rec in self.db["room"].items():
            w = f"{self.db.where('room', rid)} room.{rid}"
            for i, fx in enumerate(rec.get("objects", [])):
                self.item_or_loot(w, fx.get("item", ""))
                for c in fx.get("contains", []):
                    self.item_or_loot(w, c)
                self.lint_fit(w, fx)
            for c in rec.get("loose", []):
                self.item_or_loot(w, c)
            self.lint_room_fit(w, rec)
            self.check_context(w, rec)
        for bid, rec in self.db["building"].items():
            w = f"{self.db.where('building', bid)} building.{bid}"
            self.check_context(w, rec)
            for r in rec.get("rooms", []):
                if self.db.get("room", r.get("room", "")) is None:
                    self.err(w, f"unknown room '{r.get('room')}'")
            for o in rec.get("outdoor", []):
                if self.db.get("outdoor_set", o) is None:
                    self.err(w, f"unknown outdoor_set '{o}'")
            for p in rec.get("occupants", []):
                if self.db.get("profile", p.get("profile", "")) is None:
                    self.err(w, f"unknown profile '{p.get('profile')}'")
        for sid, rec in self.db["setting"].items():
            w = f"{self.db.where('setting', sid)} setting.{sid}"
            for b in rec.get("buildings", []):
                if self.db.get("building", b.get("building", "")) is None:
                    self.err(w, f"unknown building '{b.get('building')}'")
            for o in rec.get("outdoor", []):
                if self.db.get("outdoor_set", o.get("set", "")) is None:
                    self.err(w, f"unknown outdoor_set '{o.get('set')}'")
        for oid, rec in self.db["outdoor_set"].items():
            w = f"{self.db.where('outdoor_set', oid)} outdoor_set.{oid}"
            for e in rec.get("objects", []):
                self.item_or_loot(w, e.get("item", ""))
        for pid, rec in self.db["profile"].items():
            w = f"{self.db.where('profile', pid)} profile.{pid}"
            for slot, choices in (rec.get("outfit") or {}).items():
                for c in choices:
                    ref = c[0] if isinstance(c, list) else c
                    if ref != "none":
                        self.item_or_loot(w, ref)
            for ref in rec.get("pockets", []) + [c[0] for c in rec.get("carried", []) if c[0] != "none"]:
                self.item_or_loot(w, ref)

    def check_context(self, w: str, rec: dict) -> None:
        """A building's context, or a room's override, is a registered ctx.* tag (D-030)."""
        ctx = rec.get("context")
        if ctx is None:
            return  # required on buildings by the schema; optional on rooms
        if not isinstance(ctx, str) or not ctx.startswith("ctx."):
            self.err(w, f"context '{ctx}' is not a ctx.* tag")
        else:
            self.check_tag(w, ctx)

    def floor_m2(self, iid: str) -> float:
        """Floor an object covers: the two horizontal dimensions when it blocks or is fixed
        (for an upright item, the two after its height); blocks = "none" covers none."""
        try:
            it = resolve_item(self.db, iid)
        except ResolveError:
            return 0.0
        if it.get("blocks") == "none" or not (it.get("blocks") in ("partial", "full") or it.get("fixed")):
            return 0.0
        d = sorted(it.get("dims_mm") or [0, 0, 0], reverse=True)
        return (d[1] * d[2] if it.get("upright") else d[0] * d[1]) / MM2_PER_M2

    def room_floor(self, rec: dict) -> tuple[float, float]:
        """(m2 at the minimum counts of objects always placed, m2 at every maximum). An
        object resting on another item placed in the same room covers no floor."""
        placed = {fx.get("item") for fx in rec.get("objects", [])}
        lo = hi = 0.0
        for fx in rec.get("objects", []):
            try:
                it = resolve_item(self.db, fx.get("item", ""))
            except ResolveError:
                continue
            if it.get("rests_on") in placed:
                continue
            area = self.floor_m2(fx["item"])
            count = fx.get("count", [1, 1])
            if "chance_pct" not in fx:
                lo += count[0] * area
            hi += count[-1] * area
        return lo, hi

    def lint_room_fit(self, w: str, rec: dict) -> None:
        size = rec.get("size_m2")
        if not size:
            return
        lo, hi = self.room_floor(rec)
        if lo > ROOM_FILL_MAX * size[0]:
            self.err(w, f"room fit: {lo:.1f} m2 of furniture at minimum counts, over {ROOM_FILL_MAX} x {size[0]} m2")
        if hi > ROOM_FILL_MAX * size[-1]:
            self.err(w, f"room fit: {hi:.1f} m2 of furniture at maximum counts, over {ROOM_FILL_MAX} x {size[-1]} m2")

    def lint_fit(self, w: str, fx: dict) -> None:
        """A room object's contents must fit it (PEO-052): the mean fill by volume and mass,
        and every entry's packed longest side. Worst case may overflow: generation fills
        until full (PEO-053), so capacity is the ceiling, not the table. On an item with
        compartments (PEO-054) a room's contains is rolled once for the object and spread
        over them by fit, so it is checked against their sum; without contains, each
        compartment is checked against its own loot, the room's override
        (loot = { name = id }) first."""
        oid = fx.get("item", "")
        try:
            obj = resolve_item(self.db, oid)
        except ResolveError:
            return  # reported by item_or_loot
        where = f"{w} object {oid}"
        comps = obj.get("compartments")
        if "loot" in fx:
            self.err(where, "loot overrides are purposes now: purposes = { <compartment> = [{ purpose, w }] }")
        override = fx.get("purposes")
        if comps is not None:
            if fx.get("contains"):
                if override is not None:
                    self.err(where, "both contains and loot overrides: contains is rolled once and spread over "
                             "the compartments, so use one or the other")
                self.check_box(where, combined_box(comps), fx["contains"])
                return
            by_name = {c.get("name"): c for c in comps}
            for name, plist in (override or {}).items():
                if name not in by_name:
                    self.err(where, f"purposes: {oid} has no compartment '{name}'")
                else:  # defaults are checked on the item
                    self.lint_purposes(f"{where} compartment {name}", by_name[name], plist)
            return
        if override is not None:
            self.err(where, f"purposes override on {oid}, which has no compartments; use contains")
        if not fx.get("contains"):
            return
        box = (obj.get("features") or {}).get("container")
        if box is None:
            self.err(where, f"contains {fx['contains']} but {oid} has no container feature")
            return
        self.check_box(where, box, fx["contains"])

    def check_box(self, where: str, box: dict, contents: list[str], extras: list[str] = ()) -> None:
        """Mean fill of `contents` (loot or item ids), plus the expected volume and mass
        of any `extras` tables, against one container or compartment."""
        if any(self.db.get("loot", c) is None and self.db.get("item", c) is None for c in contents):
            return  # reported by item_or_loot
        vol = mass = 0
        entries: list[tuple[str, dict]] = []
        try:
            for c in contents:
                if self.db.get("loot", c) is not None:
                    v, m = loot_expected(self.db, c)
                    entries += loot_items(self.db, c)
                elif self.db.get("item", c) is not None:
                    v, m = (rnd(x) for x in item_expected(self.db, resolve_item(self.db, c)))
                    entries.append(("", {"item": c}))
                vol, mass = vol + v, mass + m
            for xid in extras:
                v, m = extras_expected(self.db, xid)
                vol, mass = vol + v, mass + m
        except ResolveError as ex:
            self.err(where, f"cannot size contents: {ex}")
            return
        tables = ", ".join(contents)
        if vol > box["capacity_ml"]:
            self.err(where, f"{tables}: expected {vol} ml exceeds capacity_ml {box['capacity_ml']}")
        if "max_mass_g" in box and mass > box["max_mass_g"]:
            self.err(where, f"{tables}: expected {mass} g exceeds max_mass_g {box['max_mass_g']}")
        if "max_dim_mm" in box:
            for lid, e in entries:
                try:
                    longest = packed_dims(self.db, entry_item(self.db, e))[0]
                except ResolveError:
                    continue
                if longest > box["max_dim_mm"]:
                    src = f"loot {lid}" if lid else "contains"
                    self.err(where, f"{src}: {e['item']} packs to {longest} mm, over max_dim_mm {box['max_dim_mm']}")

    def cat_queries(self) -> list[tuple[str, str]]:
        """(where, tag) for every cat tag that selects items: modifier and detail table
        applies_to, fastener items_tag, action role predicates (tag, tag_prefix)."""
        out = []
        for rtype in ("modifier", "detail_table"):
            for rid, rec in self.db[rtype].items():
                a = rec.get("applies_to") or {}
                for k in ("tags_any", "tags_all"):
                    out += [(f"{rtype}.{rid} applies_to.{k}", q) for q in a.get(k, []) if q.startswith("cat.")]
        for rid, rec in self.db["fastener"].items():
            q = rec.get("items_tag", "")
            if q.startswith("cat."):
                out.append((f"fastener.{rid} items_tag", q))

        def walk(where, v):
            if isinstance(v, dict):
                for k, x in v.items():
                    if k in ("tag", "tag_prefix") and isinstance(x, str) and x.startswith("cat."):
                        out.append((where, x))
                    else:
                        walk(where, x)
            elif isinstance(v, list):
                for x in v:
                    walk(where, x)

        for rid, rec in self.db["action"].items():
            walk(f"action.{rid}", rec)
        return out

    def lint_cat_queries(self) -> None:
        """A cat tag that selects items must match at least one item's tag (prefix match), so
        a typo cannot make an empty store or an impossible recipe (PEO-056)."""
        tags: set[str] = set()
        for iid in self.db["item"]:
            try:
                tags.update(t for t in resolve_item(self.db, iid).get("tags", []) if t.startswith("cat."))
            except ResolveError:
                continue
        for where, q in self.cat_queries():
            if q.split(".")[1] in CAT_LABEL_FAMILIES:
                continue
            if not any(t == q or t.startswith(q + ".") for t in tags):
                self.err(where, f"cat query '{q}' matches no item's tags")

    def lint_reachable(self) -> None:
        """Warn for each concrete item nothing places: no room, loot, outdoor set, profile or
        salvage yield names it, and it is not on the generator's list (PEO-056)."""
        refs: set[str] = set()

        def walk(v):
            if isinstance(v, str):
                refs.add(v)
            elif isinstance(v, list):
                for x in v:
                    walk(x)
            elif isinstance(v, dict):
                for x in v.values():
                    walk(x)

        for rtype in ("room", "loot", "outdoor_set", "profile"):
            for rec in self.db[rtype].values():
                walk(rec)
        items = {}
        for iid in self.db["item"]:
            try:
                items[iid] = resolve_item(self.db, iid)
            except ResolveError:
                continue
        for it in items.values():
            for part in (it.get("composition") or {}).values():
                if part.get("salvage"):
                    refs.add(part["salvage"].get("item", ""))
            walk((it.get("contents") or {}).get("items", []))
        for iid, it in items.items():
            if not it.get("abstract") and iid not in refs and iid not in REACHABLE_BY_GENERATOR:
                self.warn(f"{self.db.where('item', iid)} item.{iid}",
                          "unreachable: no room, loot, outdoor set, profile or salvage places it")

    def run(self) -> None:
        self.lint_registry()
        self.lint_simple("material")
        self.lint_simple("substance")
        self.lint_items()
        self.lint_modifiers()
        self.lint_refs()
        self.lint_cat_queries()
        self.lint_d030()
        self.lint_reachable()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true", help="fail on warnings too")
    ap.add_argument("-q", "--quiet", action="store_true", help="hide warnings")
    args = ap.parse_args()
    db = load()
    lint = Lint(db)
    lint.run()
    if not args.quiet:
        for x in lint.warnings:
            print(f"warn  {x}")
    for x in lint.errors:
        print(f"ERROR {x}")
    counts = ", ".join(f"{k}={len(v)}" for k, v in sorted(db.records.items()) if v)
    print(f"lint: {len(lint.errors)} error(s), {len(lint.warnings)} warning(s) [{counts}]")
    return 1 if lint.errors or (args.strict and lint.warnings) else 0


if __name__ == "__main__":
    sys.exit(main())
