#!/usr/bin/env python3
"""Content linter: schema, vocabulary and reference checks for content/ (N008, N011).

Errors fail the run; warnings are printed (use --strict to fail on warnings too).
"""

from __future__ import annotations

import argparse
import re
import sys

from common import Db, load
from derive import ResolveError, mass_estimate, resolve_item

REQ_RE = re.compile(r"^([a-z_]+)>=(\d+)$")


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
        "divisible_mode", "flammable_min", "fits_socket_of", "fits_slot_of", "param_or_dim_min",
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
                      "outdoor_set", "profile"):
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
            for c in rec.get("loose", []):
                self.item_or_loot(w, c)
        for bid, rec in self.db["building"].items():
            w = f"{self.db.where('building', bid)} building.{bid}"
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

    def run(self) -> None:
        self.lint_registry()
        self.lint_simple("material")
        self.lint_simple("substance")
        self.lint_items()
        self.lint_modifiers()
        self.lint_refs()


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
