#!/usr/bin/env python3
# OWNER: mission-fw
"""Offline "wrong way" audit of Config/Missions/*.json (no engine needed).

The headless counterpart is `tools/ue/run_mission_smoke.sh <mission> --wrong-way` (UMissionSubsystem::TickSmoke); this script
applies the same rule to the mission data alone, so it runs on any machine in milliseconds and in CI:

  * a stage whose `complete` is positional (reach_location / trigger_enter, alone or inside any/all) is "skippable";
  * a teleport lands the player on the far side, i.e. at the primary goal of the NEXT stage when that is positional too (the
    first reach point / trigger centre in its condition, pawn centre 100 cm above it, as the smoke harness teleports); that
    spot must also satisfy the CURRENT stage, otherwise the stage never completes and its waypoint points back at a spot
    the player can no longer reach (the leave_office / exit_building soft-lock);
  * a door link (tools/ue/door_links.json) teleports a pawn that touches its trigger to the partner's exit: any stage with a
    goal on a link's doorway must be satisfied by standing on that link's exit (the real race: the mission trigger is a thin box
    on the same doorway, polled per frame, and the link fades and teleports before the pawn's centre is inside it);
  * lints: a reach radius / z tolerance under the runtime minimum (200 / 150 cm), and a thin trigger_enter (box under 200 cm
    across) that is the stage's only completion.

  python3 tools/ue/audit_mission_skips.py [mission_id ...] [--missions DIR] [--warn-only]

Exit code 1 when any stage is OPEN or a lint fires (0 with --warn-only). Interact / event / timer stages cannot be skipped by
position and are reported as n/a.
"""
import argparse
import glob
import json
import math
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_DIR = os.path.join(REPO, "unreal", "BullyRemake", "Config", "Missions")
MIN_RADIUS, MIN_Z = 200.0, 150.0   # keep in step with MinReachRadiusCm / MinReachZCm in MissionSubsystem.cpp
THIN_CM = 200.0
PAWN_LIFT = 100.0                  # TickSmoke teleports to goal + (0, 0, 100)


def vec(v):
    return (float(v[0]), float(v[1]), float(v[2]))


def game_to_ue(m):
    return (m[0] * 100.0, -m[1] * 100.0, m[2] * 100.0)


class Mission:
    def __init__(self, path):
        with open(path, encoding="utf-8") as f:
            self.d = json.load(f)
        self.id = self.d.get("id", os.path.basename(path)[:-5])
        self.points = {}
        self.triggers = {}
        for k, p in self.d.get("points", {}).items():
            self.points[k] = self.loc(p)
        for t in self.d.get("triggers", []):
            if "point" in t and not any(k in t for k in ("location_cm", "center_cm", "game_m")):
                if t["point"] not in self.points:   # placed on a trigger-file point: not resolvable offline
                    continue
                at = self.points[t["point"]]
            else:
                at = self.loc(t)
            self.triggers[t["id"]] = (at, vec(t.get("half_extent_cm", [100, 100, 150])), float(t.get("yaw", 0.0)))

    @staticmethod
    def loc(o):
        if "location_cm" in o:
            return vec(o["location_cm"])
        if "center_cm" in o:
            return vec(o["center_cm"])
        if "game_m" in o:
            return game_to_ue(vec(o["game_m"]))
        raise KeyError("no location in %r" % (o,))

    def point_of(self, c):
        if "point" in c:
            if c["point"] in self.points:
                return self.points[c["point"]]
            if c["point"] in self.triggers:
                return self.triggers[c["point"]][0]
            raise KeyError("unknown point %s" % c["point"])
        return self.loc(c)


def skippable(c):
    t = c["type"]
    if t in ("reach_location", "reach", "trigger_enter"):
        return True
    if t == "any":
        return any(skippable(k) for k in c["conditions"])
    if t == "all":
        return all(skippable(k) for k in c["conditions"])
    return False


def goals(m, c):
    """Goals of a positional condition (reach points, trigger centres) in declaration order; the first is the primary goal."""
    t = c["type"]
    if t in ("reach_location", "reach"):
        return [m.point_of(c)]
    if t == "trigger_enter":
        return [m.triggers[c["trigger"]][0]]
    if t in ("any", "all"):
        out = []
        for k in c["conditions"]:
            out += goals(m, k)
        return out
    return []


def satisfied(m, c, p):
    """Is a pawn centred at p satisfying c? Event / timer / interact / flag are never satisfied by position."""
    t = c["type"]
    if t in ("reach_location", "reach"):
        g = m.point_of(c)
        return math.hypot(p[0] - g[0], p[1] - g[1]) <= c.get("radius_cm", 300.0) and abs(p[2] - g[2]) <= c.get("z_tolerance_cm", 400.0)
    if t == "trigger_enter":
        ctr, half, yaw = m.triggers[c["trigger"]]
        a = math.radians(-yaw)
        dx, dy = p[0] - ctr[0], p[1] - ctr[1]
        lx, ly = dx * math.cos(a) - dy * math.sin(a), dx * math.sin(a) + dy * math.cos(a)
        return abs(lx) <= half[0] and abs(ly) <= half[1] and abs(p[2] - ctr[2]) <= half[2]
    if t == "any":
        return any(satisfied(m, k, p) for k in c["conditions"])
    if t == "all":
        return all(satisfied(m, k, p) for k in c["conditions"])
    return False


def leaves(c):
    if c["type"] in ("any", "all"):
        for k in c["conditions"]:
            yield from leaves(k)
    else:
        yield c


def load_links():
    """[(link id, trigger centre, trigger half extents, exit-on-arrival point of the PARTNER)] from tools/ue/door_links.json."""
    path = os.path.join(REPO, "tools", "ue", "door_links.json")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        links = json.load(f)["links"]
    by_id = {l["id"]: l for l in links}
    out = []
    for l in links:
        partner = by_id.get(l.get("partner"))
        if partner:
            out.append((l["id"], vec(l["trigger"]["center_cm"]), vec(l["trigger"]["half_extent_cm"]), vec(partner["exit"]["location_cm"])))
    return out


COUPLE_CM = 60.0    # a goal this close to a doorway box (beyond its half-diagonal) is on the doorway itself, not on its exit point


def audit(m):
    rows, bad = [], 0
    links = load_links()
    stages = m.d["stages"]
    for i, s in enumerate(stages):
        c = s["complete"]
        if not skippable(c):
            rows.append((i + 1, s["id"], "n/a", "not positional (%s)" % c["type"]))
            continue
        missing = [lf["trigger"] for lf in leaves(c) if lf["type"] == "trigger_enter" and lf["trigger"] not in m.triggers]
        if missing:
            rows.append((i + 1, s["id"], "n/a", "trigger %s sits on a trigger-file point that is not imported" % missing[0]))
            continue
        notes = []
        for lf in leaves(c):
            if lf["type"] in ("reach_location", "reach"):
                if lf.get("radius_cm", 300.0) < MIN_RADIUS or lf.get("z_tolerance_cm", 400.0) < MIN_Z:
                    notes.append("LINT reach below minimum radius/z")
            elif lf["type"] == "trigger_enter":
                _, half, _ = m.triggers[lf["trigger"]]
                if min(half[0], half[1]) * 2.0 < THIN_CM and c["type"] != "any":
                    notes.append("LINT thin trigger '%s' (%d x %d cm) is the only completion" % (lf["trigger"], half[0] * 2, half[1] * 2))
        nxt = i + 1 if i + 1 < len(stages) and skippable(stages[i + 1]["complete"]) else None
        for lid, ctr, half, arrive in links:
            if any(math.hypot(g[0] - ctr[0], g[1] - ctr[1]) <= math.hypot(half[0], half[1]) + COUPLE_CM and abs(g[2] - ctr[2]) <= half[2] + COUPLE_CM for g in goals(m, c)):
                if not satisfied(m, c, (arrive[0], arrive[1], arrive[2] + PAWN_LIFT)):
                    notes.append("LINT door link '%s' lands the player at (%d,%d,%d) and does not complete it" % ((lid,) + arrive))
        if nxt is None:
            status = "n/a (next stage is not positional)"
        else:
            open_to = []
            for g in goals(m, stages[nxt]["complete"])[:1]:
                if not satisfied(m, c, (g[0], g[1], g[2] + PAWN_LIFT)):
                    open_to.append("(%d,%d,%d)" % g)
            if open_to:
                status = "OPEN: landing at stage %d '%s' goal %s leaves it incomplete" % (nxt + 1, stages[nxt]["id"], ", ".join(open_to))
            else:
                status = "ok (the primary goal of stage %d '%s' completes it)" % (nxt + 1, stages[nxt]["id"])
        if status.startswith("OPEN") or any(n.startswith("LINT") for n in notes):
            bad += 1
        rows.append((i + 1, s["id"], status, "; ".join(notes)))
    return rows, bad


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("missions", nargs="*", help="mission ids (default: every Config/Missions/*.json)")
    ap.add_argument("--missions-dir", default=DEFAULT_DIR)
    ap.add_argument("--warn-only", action="store_true")
    a = ap.parse_args()
    paths = [os.path.join(a.missions_dir, x + ".json") for x in a.missions] or sorted(glob.glob(os.path.join(a.missions_dir, "*.json")))
    total = 0
    for p in paths:
        m = Mission(p)
        rows, bad = audit(m)
        total += bad
        print("== %s (%d stages, %d flagged)" % (m.id, len(rows), bad))
        for n, sid, status, note in rows:
            if status == "n/a" and not note.startswith("LINT"):
                continue
            print("  %2d %-18s %s%s" % (n, sid, status, ("  [" + note + "]") if note else ""))
    print("TOTAL flagged: %d" % total)
    raise SystemExit(0 if (total == 0 or a.warn_only) else 1)


if __name__ == "__main__":
    main()
