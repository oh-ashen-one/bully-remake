# OWNER: builder (phase2-doors)
"""`exits` command: Config/dat/ExitLocs.dat + areabbox.dat -> paired door links.

ExitLocs.dat lists, per area (`MainMap` = the outdoor world, plus one id per interior), the places where the
player leaves that area: `AreaID, ExitID, x, y, z   #->Destination` in game metres. The coordinates are in the
same world space as our imported placements (UE cm = (100x, -100y, 100z)), interiors sitting in distant pockets.
areabbox.dat gives each area's box (`Left Right Top Bottom Base Height`: x in [Left,Right], y in [Top,Bottom],
z in [Base,Height]; an all-zero row means the area is not in the game).

This module (pure Python, no numpy) parses both files, pairs exits (an outdoor exit with the interior area's exit
that leads back outdoors; two interior exits that list each other), checks every exit point against the area's
box and against the imported scene pockets, and builds door-link entries in the existing
`bully-remake.door-links/1` format (tools/ue/door_links.json). The optional geometry refinement of the arrival
direction lives in `exits_geom.py`.

Arrival-direction heuristic (documented here, tested in tools/tests/test_exits.py). The generated `exit` placement
is `step` (2.5 m) from the trigger, on the side the pawn should arrive on, so it never lands inside its own trigger:
  * prior, interior side: toward the centre of the area box, snapped to the dominant world axis;
  * prior, outdoor side: away from the building mass (volume- and distance-weighted mean of the nearby tall
    placements' boxes), snapped to the dominant world axis; if there is none, away from the nearest imported
    placement bounds edge i.e. toward the open side;
  * with the imported geometry available (`exits_geom`), the four axis directions are tested for a walkable run
    (floor under every half metre, no wall in the body band), the direction with a wall or void directly behind
    it and the longest clear run wins, and the prior only breaks ties. Without geometry the prior is used as is.
"""

from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass, field

FORMAT = "bully-remake.door-links/1"
STEP_M = 2.5                      # arrival distance from the trigger
TRIGGER_THIN_CM = 50.0            # half extent across the doorway axis (like the hand-made links)
TRIGGER_WIDE_CM = 150.0           # half extent along the wall
TRIGGER_HALF_Z_CM = 250.0
TRIGGER_Z_LIFT_M = 1.0            # trigger centre above the floor

OUTDOOR_KEYS = {"mainmap", "island1", "island2", "island3", "businessarea", "everywhere"}
# destination labels (and area ids) that do not normalise to the same key as the area id they mean
ALIASES = {
    "comicshop": "comicshoprich",
    "bikeshop": "bikeshoprich",
    "richbarb": "hairsalon",
    "tenements": "tenement",
    "gokarttrack": "tgokart",
    "funhouse": "houseofmirrors",
}


def key(name: str | None) -> str:
    """Case/punctuation-insensitive area key: "Boy'sDorm?" -> "boysdorm", "Chem_Lab" -> "chemlab"."""
    return re.sub(r"[^a-z0-9]", "", (name or "").lower())


def canon(name: str | None) -> str:
    k = key(name)
    return ALIASES.get(k, k)


def slug(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


@dataclass
class Exit:
    area: str
    exit_id: int
    x: float
    y: float
    z: float
    dest: str | None
    line: int
    instance: int = 0             # nth listing of this (area, exit id) in the file (Staff_Room#0 appears twice)
    disabled: bool = False        # commented out in the file

    @property
    def akey(self) -> str:
        return canon(self.area)

    @property
    def dkey(self) -> str | None:
        return canon(self.dest) if self.dest is not None else None

    @property
    def outdoor_dest(self) -> bool:
        return self.dkey in OUTDOOR_KEYS

    @property
    def zero(self) -> bool:
        return self.x == 0.0 and self.y == 0.0 and self.z == 0.0

    @property
    def ref(self) -> str:
        return f"{self.area}#{self.exit_id}" + (f"(entry {self.instance + 1})" if self.instance else "")


@dataclass
class Box:
    area: str
    x0: float
    x1: float
    y0: float
    y1: float
    z0: float
    z1: float
    radar: str = ""

    @property
    def enabled(self) -> bool:
        return not (self.x0 == self.x1 == self.y0 == self.y1 == 0.0)

    @property
    def centre(self):
        return ((self.x0 + self.x1) / 2, (self.y0 + self.y1) / 2, (self.z0 + self.z1) / 2)

    def contains(self, x, y, z=None, pad_xy=0.0, pad_z=0.0):
        if not (self.x0 - pad_xy <= x <= self.x1 + pad_xy and self.y0 - pad_xy <= y <= self.y1 + pad_xy):
            return False
        return z is None or self.z0 - pad_z <= z <= self.z1 + pad_z


def _split_comment(raw: str):
    data, _, comment = raw.partition("#")
    return data.strip(), comment.strip()


def parse_exit_locs(text: str):
    """-> (exits, warnings). Commented-out rows that still parse come back with disabled=True."""
    exits, warnings = [], []
    seen: dict[tuple, int] = {}
    for n, raw in enumerate(text.splitlines(), 1):
        s = raw.strip()
        if not s:
            continue
        disabled = False
        if s.startswith("#"):
            s = s.lstrip("#").strip()
            disabled = True
        data, comment = _split_comment(s)
        fields = [f.strip() for f in data.split(",")]
        while fields and fields[-1] == "":
            fields.pop()
        if len(fields) < 5:
            if not disabled and fields:
                warnings.append(f"line {n}: unparsed row {s[:60]!r}")
            continue
        try:
            eid = int(fields[1])
            x, y, z = (float(v) for v in fields[2:5])
        except ValueError:
            if not disabled:
                warnings.append(f"line {n}: bad numbers in {s[:60]!r}")
            continue
        dest = None
        m = re.match(r"->\s*(.*)$", comment)
        if m:
            dest = m.group(1).strip().rstrip(",").strip() or None
        area = fields[0]
        inst = 0
        if not disabled:
            inst = seen.get((area.lower(), eid), 0)
            seen[(area.lower(), eid)] = inst + 1
        exits.append(Exit(area, eid, x, y, z, dest, n, inst, disabled))
    return exits, warnings


def parse_area_bbox(text: str):
    """-> dict canonical-area-key -> Box (first row wins). Columns: Left Right Top Bottom Base Height RadarMap."""
    boxes: dict[str, Box] = {}
    for raw in text.splitlines():
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        parts = re.split(r"[\s,]+", s)
        if len(parts) < 7:
            continue
        try:
            l, r, t, b, base, h = (float(v) for v in parts[1:7])
        except ValueError:
            continue
        box = Box(parts[0], min(l, r), max(l, r), min(t, b), max(t, b), min(base, h), max(base, h),
                  parts[7] if len(parts) > 7 else "")
        boxes.setdefault(canon(parts[0]), box)
    return boxes


# ------------------------------------------------------------------ pairing

@dataclass
class Pair:
    a: Exit                       # the outdoor / first side
    b: Exit
    kind: str                     # "outdoor-interior" | "interior-interior"


def pick_entry(candidates, boxes):
    """Of several entries for one area id (duplicates in the file) prefer the one inside the area box."""
    if not candidates:
        return None
    box = boxes.get(candidates[0].akey) if boxes else None
    if box is not None and box.enabled:
        for c in candidates:
            if box.contains(c.x, c.y, pad_xy=2.0):
                return c
    return candidates[0]


def pair_exits(exits, boxes=None):
    """Pair exits. -> (pairs, unpaired) where unpaired is a list of (Exit, reason)."""
    boxes = boxes or {}
    live = [e for e in exits if not e.disabled]
    unpaired: list[tuple[Exit, str]] = [(e, "commented out in ExitLocs.dat") for e in exits if e.disabled]
    shadowed: set[int] = set()
    used: set[int] = set()
    pairs: list[Pair] = []
    # the same (area, exit id) listed twice (Staff_Room#0): keep the entry inside the area box, shadow the others
    groups: dict[tuple, list] = {}
    for e in live:
        groups.setdefault((e.akey, e.exit_id), []).append(e)
    for grp in groups.values():
        if len(grp) > 1:
            keep = pick_entry(grp, boxes)
            for e in grp:
                if e is not keep:
                    shadowed.add(id(e))

    def free(e):
        return id(e) not in used and id(e) not in shadowed

    outdoor = [e for e in live if e.akey == "mainmap"]
    interior = [e for e in live if e.akey != "mainmap"]

    # outdoor exit <-> the interior's exit that leads back outdoors
    for o in outdoor:
        if o.zero:
            continue
        cands = [e for e in interior if free(e) and e.akey == o.dkey and e.outdoor_dest and not e.zero]
        back = pick_entry(cands, boxes)
        if back is not None and free(o):
            pairs.append(Pair(o, back, "outdoor-interior"))
            used.update((id(o), id(back)))

    # interior <-> interior, only when both sides list each other
    for e1 in interior:
        if not free(e1) or e1.zero or e1.outdoor_dest or e1.dkey is None:
            continue
        cands = [e for e in interior if free(e) and e is not e1 and e.akey == e1.dkey and e.dkey == e1.akey
                 and not e.zero]
        e2 = pick_entry(cands, boxes)
        if e2 is not None:
            pairs.append(Pair(e1, e2, "interior-interior"))
            used.update((id(e1), id(e2)))

    areas = {e.akey for e in live}
    for e in live:
        if id(e) in used:
            continue
        if id(e) in shadowed:
            reason = "duplicate listing of the same area+exit id (the entry inside the area box was used)"
        elif e.akey == "everywhere" or (e.zero and e.dest is None):
            reason = "placeholder row (0,0,0)"
        elif e.zero:
            reason = "placeholder coordinates (0,0,0)"
        elif e.dkey is None:
            reason = "no #-> destination comment"
        elif e.akey == "mainmap":
            if e.dkey not in areas:
                reason = f"destination {e.dest!r} has no interior area in ExitLocs.dat"
            else:
                targets = sorted({x.dest for x in live if x.akey == e.dkey and x.dest})
                reason = f"{e.dest} has no exit back to MainMap (its exits lead to: {', '.join(targets) or 'nothing'})"
        elif e.outdoor_dest:
            reason = f"no MainMap exit leads to {e.area}"
        elif e.dkey not in areas:
            reason = f"destination {e.dest!r} has no area entry in ExitLocs.dat"
        else:
            reason = f"{e.dest} lists no exit back to {e.area}"
        unpaired.append((e, reason))
    # an interior entry shadowed by a better duplicate of the same area is reported as such
    return pairs, unpaired


# ------------------------------------------------------------------ pockets / sanity

def scene_pockets(manifest, extents=None):
    """-> list of dict(name, kind, lo, hi) from the scene manifest, game metres. Default bounds are the placement
    ORIGINS (`bounds_game_positions`); `extents` = {scene: (lo, hi)} (placed model boxes, exits_geom) replaces them,
    since a room mesh can extend well past its origin."""
    out = []
    for s in manifest.get("scenes", []):
        b = s.get("bounds_game_positions")
        if extents and s["name"] in extents:
            b = extents[s["name"]]
        if b:
            out.append({"name": s["name"], "kind": s.get("kind", ""), "lo": [float(v) for v in b[0]],
                        "hi": [float(v) for v in b[1]]})
    return out


def _inside(lo, hi, x, y, z, pad_xy, pad_z):
    return (lo[0] - pad_xy <= x <= hi[0] + pad_xy and lo[1] - pad_xy <= y <= hi[1] + pad_xy
            and lo[2] - pad_z <= z <= hi[2] + pad_z)


def _overlap_box(box: Box, p, pad_xy=2.0, pad_z=3.0):
    lo, hi = p["lo"], p["hi"]
    return (box.x0 - pad_xy <= hi[0] and lo[0] <= box.x1 + pad_xy and box.y0 - pad_xy <= hi[1]
            and lo[1] <= box.y1 + pad_xy and box.z0 - pad_z <= hi[2] and lo[2] <= box.z1 + pad_z)


def check_exit(e: Exit, boxes, pockets, pad_xy=3.0, pad_z=4.0):
    """Sanity of one exit point against its area box and the imported pockets.
    -> dict(in_box, scenes, in_pocket, nearest_pocket_m, problems[])"""
    box = boxes.get(e.akey)
    res = {"in_box": None, "scenes": [], "in_pocket": None, "nearest_pocket_m": None, "problems": []}
    outdoor = e.akey == "mainmap"
    if box is None or not box.enabled:
        res["problems"].append("area has no (enabled) entry in areabbox.dat")
    else:
        res["in_box"] = box.contains(e.x, e.y, e.z, pad_xy=2.0, pad_z=3.0)
        if not res["in_box"]:
            res["problems"].append(
                f"exit point ({e.x:.1f},{e.y:.1f},{e.z:.1f}) outside the area box "
                f"x[{box.x0:.1f},{box.x1:.1f}] y[{box.y0:.1f},{box.y1:.1f}] z[{box.z0:.1f},{box.z1:.1f}]")
    if outdoor:
        cands = [p for p in pockets if p["kind"] == "exterior"]
    elif box is not None and box.enabled:
        # interior areas normally map to interior scenes; JunkYard / BMXTrack / Asylum grounds live in exterior scenes
        cands = [p for p in pockets if _overlap_box(box, p)]
        inter = [p for p in cands if p["kind"] == "interior"]
        cands = inter or cands
    else:
        cands = []
    res["scenes"] = [p["name"] for p in cands]
    if not cands:
        res["in_pocket"] = False
        res["problems"].append("no imported " + ("exterior scene" if outdoor else "interior pocket") + " covers this area")
    else:
        best = None
        for p in cands:
            lo, hi = p["lo"], p["hi"]
            dx = max(lo[0] - e.x, 0, e.x - hi[0])
            dy = max(lo[1] - e.y, 0, e.y - hi[1])
            dz = max(lo[2] - e.z, 0, e.z - hi[2])
            d = math.sqrt(dx * dx + dy * dy)
            if best is None or d < best:
                best = d
            if _inside(lo, hi, e.x, e.y, e.z, pad_xy, pad_z):
                res["in_pocket"] = True
        res["nearest_pocket_m"] = round(best, 2)
        if not res["in_pocket"]:
            res["in_pocket"] = False
            res["problems"].append(
                f"exit point is {best:.1f} m outside the placement bounds of imported scene(s) {', '.join(res['scenes'])}")
    return res


# ------------------------------------------------------------------ direction priors

CARD = {"E": (1.0, 0.0), "W": (-1.0, 0.0), "N": (0.0, 1.0), "S": (0.0, -1.0)}


def snap(dx, dy):
    """Dominant world axis of a vector, unit length. (0,0) -> +x."""
    if abs(dx) >= abs(dy):
        return (1.0 if dx >= 0 else -1.0, 0.0)
    return (0.0, 1.0 if dy >= 0 else -1.0)


def prior_interior(e: Exit, box: Box | None):
    """Toward the area box centre."""
    if box is None or not box.enabled:
        return None
    cx, cy, _ = box.centre
    if math.hypot(cx - e.x, cy - e.y) < 0.5:
        # on the centre: pick the long axis of the box
        return (1.0, 0.0) if (box.x1 - box.x0) >= (box.y1 - box.y0) else (0.0, 1.0)
    return snap(cx - e.x, cy - e.y)


def prior_outdoor(e: Exit, solids, radius=15.0):
    """Away from the building mass. `solids` = [(cx, cy, volume)] of nearby tall placements (game metres).
    None if there are none."""
    sx = sy = wsum = 0.0
    for cx, cy, vol in solids:
        d = math.hypot(cx - e.x, cy - e.y)
        if d > radius:
            continue
        w = (max(vol, 1e-3) ** (1 / 3)) / (1.0 + d)
        sx += w * (cx - e.x)
        sy += w * (cy - e.y)
        wsum += w
    if wsum == 0.0:
        return None
    return snap(-sx, -sy)


def open_side(e: Exit, rects, radius=25.0):
    """Fallback outdoor prior: the cardinal direction with the fewest placements in a 12 m wide, 25 m long strip.
    `rects` = [(cx, cy)] of placements. Ties -> E, W, N, S order."""
    best, best_n = (1.0, 0.0), None
    for dxy in ((1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0)):
        n = 0
        for cx, cy in rects:
            rx, ry = cx - e.x, cy - e.y
            along = rx * dxy[0] + ry * dxy[1]
            across = abs(-rx * dxy[1] + ry * dxy[0])
            if 0 < along <= radius and across <= 6.0:
                n += 1
        if best_n is None or n < best_n:
            best, best_n = dxy, n
    return best


# ------------------------------------------------------------------ link building

def ue_cm(x, y, z):
    return [round(x * 100.0, 1), round(-y * 100.0, 1), round(z * 100.0, 1)]


def ue_yaw(dx, dy):
    """UE yaw (deg) of a game-space direction: UE = (x, -y), yaw 0 = +X_ue, 90 = +Y_ue."""
    return round(math.degrees(math.atan2(-dy, dx)), 1)


@dataclass
class Side:
    exit: Exit
    direction: tuple              # unit game-space (dx, dy)
    method: str                   # how the direction was chosen
    floor_z: float | None = None  # measured under the exit point (geometry) or None
    arrive_dist: float = STEP_M
    arrive_z: float | None = None
    warnings: list = field(default_factory=list)


def make_link(side: Side, partner: Side, link_id: str, partner_id: str, label: str):
    e = side.exit
    dx, dy = side.direction
    fz = side.floor_z if side.floor_z is not None else e.z
    az = side.arrive_z if side.arrive_z is not None else fz
    half = [TRIGGER_THIN_CM, TRIGGER_WIDE_CM, TRIGGER_HALF_Z_CM] if abs(dx) >= abs(dy) \
        else [TRIGGER_WIDE_CM, TRIGGER_THIN_CM, TRIGGER_HALF_Z_CM]
    ex, ey = e.x + dx * side.arrive_dist, e.y + dy * side.arrive_dist
    link = {
        "id": link_id,
        "partner": partner_id,
        "label": label,
        "game_m": {"door": [round(e.x, 3), round(e.y, 3), round(e.z, 3)],
                   "note": f"ExitLocs.dat {e.ref} -> {e.dest}; arrival direction {side.method}"},
        "trigger": {"center_cm": ue_cm(e.x, e.y, fz + TRIGGER_Z_LIFT_M), "half_extent_cm": half},
        "exit": {"location_cm": ue_cm(ex, ey, az), "yaw": ue_yaw(dx, dy)},
    }
    if side.warnings:
        link["warnings"] = list(side.warnings)
    return link


def _box_overlap_cm(a, b):
    ca, ha = a["trigger"]["center_cm"], a["trigger"]["half_extent_cm"]
    cb, hb = b["trigger"]["center_cm"], b["trigger"]["half_extent_cm"]
    return all(abs(ca[i] - cb[i]) < ha[i] + hb[i] for i in range(3))


def is_duplicate_of(link, handmade, tol_cm=500.0):
    """True if the trigger sits within tol_cm (XY) of a hand-made link's trigger."""
    c = link["trigger"]["center_cm"]
    for h in handmade:
        hc = h["trigger"]["center_cm"]
        if math.hypot(c[0] - hc[0], c[1] - hc[1]) <= tol_cm and abs(c[2] - hc[2]) <= 400.0:
            return h["id"]
    return None


def link_ids(pairs):
    """Stable, readable unique ids per side: g_<area>__<partner area> (+ _<exit id> on collision)."""
    taken: dict[str, int] = {}
    out = []
    for p in pairs:
        ids = []
        for me, other in ((p.a, p.b), (p.b, p.a)):
            base = f"g_{slug(me.area)}__{slug(other.area)}"
            n = taken.get(base, 0)
            taken[base] = n + 1
            ids.append(base if n == 0 else f"{base}_{me.exit_id}_{n + 1}")
        out.append(tuple(ids))
    return out


def trigger_conflicts(links):
    """Pairs of link ids whose trigger boxes overlap, and links whose arrival point lies in another trigger."""
    overlaps, arrivals = [], []
    for i, a in enumerate(links):
        for b in links[i + 1:]:
            if _box_overlap_cm(a, b):
                overlaps.append((a["id"], b["id"]))
    for a in links:
        ex = a["exit"]["location_cm"]
        for b in links:
            if b is a:
                continue
            c, h = b["trigger"]["center_cm"], b["trigger"]["half_extent_cm"]
            if all(abs(ex[i] - c[i]) < h[i] for i in range(3)):
                arrivals.append((a["id"], b["id"]))
    return overlaps, arrivals
