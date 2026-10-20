# OWNER: builder-A (importers)
"""`townlife` command: pedestrian walking loops and kerbside parked-car spots for the outdoor town districts.

DATA SOURCE (read this first). The brief for this module was "build a walkable surface from the WALKABLE_*
marker meshes". On the shipped game those meshes are placeholders: every `WALKABLE_*OP.nif` is a single
2 m triangle (3 vertices, 2048-byte NIF) and every `NOGO_*OP.nif` is the same stub (the one exception is
`NOGO_SHOREA_B`, a 22-triangle vertical wall along the shore). The original game keeps its pedestrian and
road navigation in `Config/GRIDS.DAT` instead: a sidewalk graph (1234 nodes / 1365 links) and a road graph
(476 nodes / 507 links with lane counts), both in world coordinates (metres). This module therefore uses

  * GRIDS.DAT sidewalk graph  -> the walkable network (a segment is "on the walkable surface" when it stays
                                 within `WALK_RADIUS` of a sidewalk link);
  * GRIDS.DAT road graph      -> where roads run (parking candidates lie along road links);
  * the converted terrain     -> which ground is road: triangles whose base texture is a street / asphalt /
                                 cobble-road texture are rasterised into a road mask, the kerb is the edge
                                 of that mask, and a spot must lie fully on it;
  * non-stub NOGO markers     -> keep-out geometry (stubs are ignored and counted);
  * placement bounds          -> which district a graph node belongs to.

Everything is game space (metres, +Z up) internally; the JSON is UE centimetres (coords.game_to_ue_position).
Pure geometry helpers carry no numpy/PIL dependency (unit-tested with synthetic fixtures); terrain rasters and
the evidence renders need numpy and Pillow.
"""

from __future__ import annotations

import heapq
import json
import math
import os
import re
import time

from . import coords
from .nft import safe_name

FORMAT = "bully-remake.townlife/1"

# (output name, IPB stem, description). Order follows the brief.
DISTRICTS = [
    ("business_strip", "tbusines", "Business strip (Bullworth town centre)"),
    ("old_bullworth_vale", "trich", "Old Bullworth Vale (rich area)"),
    ("new_coventry", "tindust", "New Coventry (industrial)"),
    ("carnival", "tcarni", "Carnival"),
    ("junkyard", "tjyard", "Junkyard"),
    ("bmx_park", "tBMX", "BMX park"),
    ("go_kart_track", "tgokart", "Go-kart track"),
    ("tglobal", "tGlobal", "tGlobal (always-loaded relief, shore)"),
]

WALK_RADIUS = 1.75          # m: a point within this of a sidewalk link counts as walkable
SAMPLE_STEP = 0.5           # m: spacing of the on-surface checks along a segment
LOOP_MIN_M, LOOP_MAX_M = 25.0, 120.0
LOOP_MIN_WP, LOOP_MAX_WP = 4, 10
LOOPS_MIN, LOOPS_MAX = 3, 6
CAR_LENGTH, CAR_WIDTH = 5.0, 2.4
KERB_INSET = 1.6            # m: spot centre this far out from the kerb onto the road
PARK_SPACING = 6.0          # m: minimum centre-to-centre distance between spots
PARK_MIN, PARK_MAX = 4, 10
JUNCTION_CLEARANCE = 7.0    # m from a road node with 3+ links
ROAD_TEXTURE = re.compile(r"(street|asphalt|cobroad|alleyroad|roadway)", re.I)
# ground that is neither road nor somewhere to stroll (fallback walkable raster only)
NONWALK_TEXTURE = re.compile(r"(roof|tarpaper|wtr_|water|cliff|metal|rail|tracks|contain|traincont|trailer|glass|"
                             r"fence|wall|tree|leaf|branch|conifer|bush)", re.I)
STRICT_ROAD_TEXTURE = re.compile(r"(street|(?<!gk_)asphalt)", re.I)   # sealed street only (no cobble ground, no kart track)
WATER_TEXTURE = re.compile(r"(wtr_|water)", re.I)
MASK_CELL = 0.25            # m per road-mask pixel


# ---------------------------------------------------------------------------
# GRIDS.DAT
# ---------------------------------------------------------------------------

class Grid:
    """One navigation graph: nodes {id: dict(pos, width, flags)} and links [dict(id, src, dest, dist, ...)]."""

    def __init__(self, nodes, links):
        self.nodes = nodes
        self.links = links

    def adjacency(self):
        adj = {nid: {} for nid in self.nodes}
        for lk in self.links:
            a, b = lk["src"], lk["dest"]
            if a == b or a not in adj or b not in adj:
                continue
            d = math.dist(self.nodes[a]["pos"], self.nodes[b]["pos"])
            adj[a][b] = d
            adj[b][a] = d
        return adj


class Grids:
    def __init__(self, ped: Grid, car: Grid):
        self.ped = ped
        self.car = car


_NODE_RE = re.compile(r"NODEID (\d+)\s*NODE\s*BEGIN(.*?)\n\s*END\s*\n", re.S)
_LINK_RE = re.compile(r"LINKID (\d+)\s*LINK\s*BEGIN(.*?)\n\s*END\s*\n", re.S)


def _parse_section(sec: str) -> Grid:
    nodes = {}
    for m in _NODE_RE.finditer(sec):
        body = m.group(2)
        pm = re.search(r"Position\s+([-\d.eE+]+)\s*,\s*([-\d.eE+]+)\s*,\s*([-\d.eE+]+)", body)
        if not pm:
            continue
        wm = re.search(r"WIDTH\s+(\d+)", body)
        flags = {k: v == "TRUE" for k, v in re.findall(
            r"(ONDEADEND|SWITCHEDOFF|BETWEENLEVELS|WATERNODE|UNDERBRIDGE)\s+(\w+)", body)}
        nodes[int(m.group(1))] = {"pos": (float(pm.group(1)), float(pm.group(2)), float(pm.group(3))),
                                  "width": int(wm.group(1)) if wm else 0, "flags": flags}
    links = []
    for m in _LINK_RE.finditer(sec):
        vals = {k: float(v) for k, v in re.findall(
            r"(Dist|DEST|SRC|LANESTO|LANESFRO|WIDTH)\s+([-\d.eE+]+)", m.group(2))}
        if "SRC" not in vals or "DEST" not in vals:
            continue
        links.append({"id": int(m.group(1)), "src": int(vals["SRC"]), "dest": int(vals["DEST"]),
                      "dist": vals.get("Dist", 0.0), "lanes_to": int(vals.get("LANESTO", 0)),
                      "lanes_fro": int(vals.get("LANESFRO", 0))})
    return Grid(nodes, links)


def parse_grids(text: str) -> Grids:
    """Parse Config/GRIDS.DAT (CRLF text): a PEDGRID section then a CARGRID section."""
    text = text.replace("\r", "") + "\n"
    i = text.find("\nCARGRID")
    ped_sec, car_sec = (text[:i], text[i:]) if i >= 0 else (text, "")
    return Grids(_parse_section(ped_sec), _parse_section(car_sec))


# ---------------------------------------------------------------------------
# 2D geometry (tuples; z ignored)
# ---------------------------------------------------------------------------

def seg_point_dist(p, a, b) -> float:
    ax, ay = a[0], a[1]
    dx, dy = b[0] - ax, b[1] - ay
    l2 = dx * dx + dy * dy
    if l2 < 1e-12:
        return math.hypot(p[0] - ax, p[1] - ay)
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / l2))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def segments_intersect(a, b, c, d) -> bool:
    """Proper or touching intersection of segments ab and cd."""
    d1, d2 = _cross(c, d, a), _cross(c, d, b)
    d3, d4 = _cross(a, b, c), _cross(a, b, d)
    if ((d1 > 0 > d2) or (d1 < 0 < d2)) and ((d3 > 0 > d4) or (d3 < 0 < d4)):
        return True

    def on(p, q, r):
        return (min(p[0], q[0]) - 1e-9 <= r[0] <= max(p[0], q[0]) + 1e-9
                and min(p[1], q[1]) - 1e-9 <= r[1] <= max(p[1], q[1]) + 1e-9)
    return ((abs(d1) < 1e-9 and on(c, d, a)) or (abs(d2) < 1e-9 and on(c, d, b))
            or (abs(d3) < 1e-9 and on(a, b, c)) or (abs(d4) < 1e-9 and on(a, b, d)))


def seg_seg_dist(a, b, c, d) -> float:
    if segments_intersect(a, b, c, d):
        return 0.0
    return min(seg_point_dist(a, c, d), seg_point_dist(b, c, d), seg_point_dist(c, a, b), seg_point_dist(d, a, b))


def point_in_polygon(p, poly) -> bool:
    inside = False
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        if (a[1] > p[1]) != (b[1] > p[1]):
            x = a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1])
            if p[0] < x:
                inside = not inside
    return inside


def rect_corners(cx, cy, yaw_rad, length, width):
    """Oriented rectangle, `length` along the heading (cos yaw, sin yaw), `width` across. CCW corners."""
    c, s = math.cos(yaw_rad), math.sin(yaw_rad)
    hl, hw = length / 2.0, width / 2.0
    out = []
    for lx, ly in ((hl, -hw), (hl, hw), (-hl, hw), (-hl, -hw)):
        out.append((cx + lx * c - ly * s, cy + lx * s + ly * c))
    return out


def segment_rect_distance(a, b, corners) -> float:
    """Distance between segment ab and the filled convex rectangle (0 when they touch or overlap)."""
    if point_in_polygon(a, corners) or point_in_polygon(b, corners):
        return 0.0
    best = float("inf")
    for i in range(4):
        c, d = corners[i], corners[(i + 1) % 4]
        best = min(best, seg_seg_dist(a, b, c, d))
        if best == 0.0:
            return 0.0
    return best


def sample_segment(a, b, step=SAMPLE_STEP):
    """Points from a to b inclusive, at most `step` apart."""
    l = math.hypot(b[0] - a[0], b[1] - a[1])
    n = max(1, int(math.ceil(l / step)))
    return [(a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n) for i in range(n + 1)]


def polyline_length(pts, closed=False) -> float:
    n = len(pts)
    total = sum(math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]) for i in range(n - 1))
    if closed and n > 1:
        total += math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1])
    return total


def polyline_self_intersects(pts, closed=True) -> bool:
    n = len(pts)
    segs = [(pts[i], pts[(i + 1) % n]) for i in range(n if closed else n - 1)]
    for i in range(len(segs)):
        for j in range(i + 1, len(segs)):
            if j == i + 1 or (closed and i == 0 and j == len(segs) - 1):
                continue  # neighbours share an end point
            if segments_intersect(segs[i][0], segs[i][1], segs[j][0], segs[j][1]):
                return True
    return False


def heading_yaw_deg_ue(dx, dy) -> float:
    """UE yaw (degrees, in (-180, 180]) of a game-space direction. UE y = -game y, yaw measured from +X
    toward +Y, so a game direction (dx, dy) has yaw atan2(-dy, dx)."""
    yaw = math.degrees(math.atan2(-dy, dx))
    if yaw <= -180.0:
        yaw += 360.0
    return yaw


class SegIndex:
    """Uniform-grid index of 2D segments for 'is anything within r of this point' queries."""

    def __init__(self, segs, cell=8.0):
        self.cell = cell
        self.segs = list(segs)
        self.grid = {}
        for i, (a, b) in enumerate(self.segs):
            x0, x1 = sorted((a[0], b[0]))
            y0, y1 = sorted((a[1], b[1]))
            for cx in range(int(math.floor(x0 / cell)), int(math.floor(x1 / cell)) + 1):
                for cy in range(int(math.floor(y0 / cell)), int(math.floor(y1 / cell)) + 1):
                    self.grid.setdefault((cx, cy), []).append(i)

    def _cells(self, x0, y0, x1, y1):
        c = self.cell
        for cx in range(int(math.floor(x0 / c)), int(math.floor(x1 / c)) + 1):
            for cy in range(int(math.floor(y0 / c)), int(math.floor(y1 / c)) + 1):
                yield (cx, cy)

    def point_within(self, p, r) -> bool:
        seen = set()
        for key in self._cells(p[0] - r, p[1] - r, p[0] + r, p[1] + r):
            for i in self.grid.get(key, ()):
                if i in seen:
                    continue
                seen.add(i)
                if seg_point_dist(p, *self.segs[i]) <= r:
                    return True
        return False

    def rect_within(self, corners, r) -> bool:
        """True when some segment comes within r of the rectangle."""
        xs = [c[0] for c in corners]
        ys = [c[1] for c in corners]
        seen = set()
        for key in self._cells(min(xs) - r, min(ys) - r, max(xs) + r, max(ys) + r):
            for i in self.grid.get(key, ()):
                if i in seen:
                    continue
                seen.add(i)
                if segment_rect_distance(self.segs[i][0], self.segs[i][1], corners) <= r:
                    return True
        return False

    def segment_crosses(self, a, b) -> bool:
        seen = set()
        for key in self._cells(min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])):
            for i in self.grid.get(key, ()):
                if i in seen:
                    continue
                seen.add(i)
                if segments_intersect(a, b, *self.segs[i]):
                    return True
        return False


class Nogo:
    """Keep-out geometry in the XY plane: 2D triangles (flat-ish markers) and wall segments (vertical ones)."""

    def __init__(self):
        self.polys = []   # (bbox, [p0, p1, p2])
        self.walls = []   # (a, b)
        self._wall_index = None

    @classmethod
    def from_triangles(cls, tris, min_area=0.05):
        """tris: iterable of three 3D points. Triangles whose XY projection is (nearly) degenerate are vertical
        faces: their XY extent becomes a wall segment."""
        n = cls()
        for t in tris:
            p = [(v[0], v[1]) for v in t]
            area = abs(_cross(p[0], p[1], p[2])) / 2.0
            if area >= min_area:
                xs, ys = [q[0] for q in p], [q[1] for q in p]
                n.polys.append(((min(xs), min(ys), max(xs), max(ys)), p))
            else:
                best = max(((p[i], p[(i + 1) % 3]) for i in range(3)),
                           key=lambda ab: math.hypot(ab[1][0] - ab[0][0], ab[1][1] - ab[0][1]))
                if math.hypot(best[1][0] - best[0][0], best[1][1] - best[0][1]) > 1e-3:
                    n.walls.append(best)
        return n

    def __bool__(self):
        return bool(self.polys or self.walls)

    def _widx(self):
        if self._wall_index is None:
            self._wall_index = SegIndex(self.walls)
        return self._wall_index

    def hits_point(self, p, margin=0.0) -> bool:
        for (x0, y0, x1, y1), poly in self.polys:
            if x0 - margin <= p[0] <= x1 + margin and y0 - margin <= p[1] <= y1 + margin:
                if point_in_polygon(p, poly) or any(
                        seg_point_dist(p, poly[i], poly[(i + 1) % 3]) <= margin for i in range(3)):
                    return True
        return bool(self.walls) and margin > 0 and self._widx().point_within(p, margin)

    def hits_segment(self, a, b) -> bool:
        if self.walls and self._widx().segment_crosses(a, b):
            return True
        for (x0, y0, x1, y1), poly in self.polys:
            if max(a[0], b[0]) < x0 or min(a[0], b[0]) > x1 or max(a[1], b[1]) < y0 or min(a[1], b[1]) > y1:
                continue
            if point_in_polygon(a, poly) or point_in_polygon(b, poly):
                return True
            if any(segments_intersect(a, b, poly[i], poly[(i + 1) % 3]) for i in range(3)):
                return True
        return False

    def hits_rect(self, corners) -> bool:
        for i in range(4):
            if self.hits_segment(corners[i], corners[(i + 1) % 4]):
                return True
        if any(self.hits_point(c) for c in corners):
            return True
        for _bb, poly in self.polys:  # a small marker entirely inside the rectangle
            if point_in_polygon(poly[0], corners):
                return True
        return any(point_in_polygon(w[0], corners) for w in self.walls)


# ---------------------------------------------------------------------------
# Walkable network
# ---------------------------------------------------------------------------

class WalkableNet:
    """The walkable corridor: sidewalk links inflated by `radius`, minus keep-out geometry."""

    def __init__(self, segs, radius=WALK_RADIUS, nogo: Nogo | None = None):
        self.radius = radius
        self.index = SegIndex(segs)
        self.nogo = nogo or Nogo()

    def point_ok(self, p) -> bool:
        return self.index.point_within(p, self.radius) and not self.nogo.hits_point(p)

    def segment_ok(self, a, b, step=SAMPLE_STEP) -> bool:
        if self.nogo and self.nogo.hits_segment(a, b):
            return False
        return all(self.index.point_within(p, self.radius) for p in sample_segment(a, b, step))

    def rect_touches(self, corners) -> bool:
        return self.index.rect_within(corners, self.radius) or (bool(self.nogo) and self.nogo.hits_rect(corners))


# ---------------------------------------------------------------------------
# Loops
# ---------------------------------------------------------------------------

def find_cycles(adj, pos, min_len=LOOP_MIN_M, max_len=LOOP_MAX_M):
    """Simple cycles of the undirected graph `adj` ({node: {nbr: dist}}), length in [min_len, max_len].

    For each root s a Dijkstra tree is grown to max_len; every non-tree edge (u, v) whose tree paths leave s by
    different first hops closes a simple cycle through s. Each cycle is returned once as a node list."""
    seen, out = set(), []
    for s in adj:
        dist, parent, branch = {s: 0.0}, {s: None}, {s: None}
        heap = [(0.0, s)]
        done = set()
        while heap:
            d, u = heapq.heappop(heap)
            if u in done:
                continue
            done.add(u)
            for v, w in adj[u].items():
                nd = d + w
                if nd > max_len * 0.55 or (v in dist and dist[v] <= nd):
                    continue
                dist[v], parent[v] = nd, u
                branch[v] = v if u == s else branch[u]
                heapq.heappush(heap, (nd, v))
        for u in done:
            for v, w in adj[u].items():
                if v not in done or u >= v:
                    continue
                if parent.get(u) == v or parent.get(v) == u:
                    continue  # tree edge
                if branch[u] is not None and branch[u] == branch[v]:
                    continue  # paths share a first hop: not simple through s
                length = dist[u] + dist[v] + w
                if not (min_len <= length <= max_len):
                    continue
                pu, x = [], u
                while x is not None:
                    pu.append(x)
                    x = parent[x]
                pv, x = [], v
                while x is not None:
                    pv.append(x)
                    x = parent[x]
                cyc = pu[::-1] + pv[:-1]  # s ... u, v ... (child of s)
                if len(cyc) < 3 or len(set(cyc)) != len(cyc):
                    continue
                key = frozenset(frozenset((cyc[i], cyc[(i + 1) % len(cyc)])) for i in range(len(cyc)))
                if key in seen:
                    continue
                seen.add(key)
                out.append(cyc)
    return out


def simplify_loop(pts, max_points=LOOP_MAX_WP, ok=None):
    """Drop vertices of a closed loop, smallest triangle area first, until at most `max_points` remain.
    `ok(a, b)` must approve the new chord; vertices whose removal is rejected are kept."""
    pts = list(pts)
    while len(pts) > max_points:
        n = len(pts)
        order = sorted(range(n), key=lambda i: abs(_cross(pts[i - 1], pts[i], pts[(i + 1) % n])))
        for i in order:
            a, b = pts[i - 1], pts[(i + 1) % n]
            if ok is None or ok(a, b):
                del pts[i]
                break
        else:
            break
    return pts


def subdivide_loop(pts, min_points=LOOP_MIN_WP):
    """Insert midpoints of the longest chords until the loop has at least `min_points` vertices."""
    pts = list(pts)
    while len(pts) < min_points:
        n = len(pts)
        i = max(range(n), key=lambda k: math.dist(pts[k][:2], pts[(k + 1) % n][:2]))
        a, b = pts[i], pts[(i + 1) % n]
        pts.insert(i + 1, tuple((a[k] + b[k]) / 2.0 for k in range(len(a))))
    return pts


def simplify_chain(pts, tol, ok=None):
    """Douglas-Peucker on an open chain (3D points, XY distance); the chord must pass `ok` or the span is split."""
    if len(pts) <= 2:
        return list(pts)
    a, b = pts[0], pts[-1]
    far, fd = 0, -1.0
    for i in range(1, len(pts) - 1):
        d = seg_point_dist(pts[i], a, b)
        if d > fd:
            far, fd = i, d
    if fd <= tol and (ok is None or ok(a, b)):
        return [a, b]
    left = simplify_chain(pts[:far + 1], tol, ok)
    right = simplify_chain(pts[far:], tol, ok)
    return left[:-1] + right


def _offset_chain(pts, off):
    """Shift an open chain sideways by `off` m (positive = right of travel), by vertex normals."""
    out = []
    n = len(pts)
    for i, p in enumerate(pts):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, n - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        l = math.hypot(dx, dy) or 1.0
        out.append((p[0] + dy / l * off, p[1] - dx / l * off, p[2]))
    return out


def out_and_back_loop(chain, offset, net: WalkableNet, tol=0.6):
    """A closed loop that walks `chain` out on one side and back on the other (lateral `offset` m each way, so it
    is a true closed polygon rather than a retraced line). Returns waypoints or None."""
    chain = simplify_chain(chain, tol, net.segment_ok)
    for off in (offset, offset * 0.5, 0.0):
        fwd = _offset_chain(chain, off)
        back = _offset_chain(chain[::-1], off)
        loop = fwd + back
        if off == 0.0:
            loop = chain + chain[-2:0:-1]
        if all(net.point_ok(p) for p in loop) and all(
                net.segment_ok(loop[i], loop[(i + 1) % len(loop)]) for i in range(len(loop))):
            return loop
    return None


def _loop_ok(wps, net: WalkableNet) -> bool:
    n = len(wps)
    if not (LOOP_MIN_WP <= n <= LOOP_MAX_WP):
        return False
    length = polyline_length([(p[0], p[1]) for p in wps], closed=True)
    if not (LOOP_MIN_M <= length <= LOOP_MAX_M):
        return False
    return all(net.segment_ok(wps[i], wps[(i + 1) % n]) for i in range(n))


def _centroid(pts):
    return (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))


def candidate_loops(adj, pos, net: WalkableNet):
    """All usable loop candidates: dicts(kind, waypoints, nodes, length)."""
    cands = []
    for cyc in find_cycles(adj, pos):
        pts = [pos[n] for n in cyc]
        if polyline_self_intersects([(p[0], p[1]) for p in pts]):
            continue
        pts = simplify_loop(pts, LOOP_MAX_WP, net.segment_ok)
        pts = subdivide_loop(pts, LOOP_MIN_WP)
        if _loop_ok(pts, net):
            cands.append({"kind": "cycle", "waypoints": pts, "nodes": frozenset(cyc),
                          "length": polyline_length([(p[0], p[1]) for p in pts], closed=True)})
    # out-and-back loops along straight-ish chains of sidewalk links
    for s in adj:
        for first in adj[s]:
            for target in (22.0, 35.0, 50.0):
                chain, total, prev, cur = [s], 0.0, None, s
                nxt = first
                while True:
                    chain.append(nxt)
                    total += adj[cur][nxt]
                    prev, cur = cur, nxt
                    if total >= target:
                        break
                    opts = [(v, math.atan2(pos[v][1] - pos[cur][1], pos[v][0] - pos[cur][0]))
                            for v in adj[cur] if v != prev and v not in chain]
                    if not opts:
                        break
                    h = math.atan2(pos[cur][1] - pos[prev][1], pos[cur][0] - pos[prev][0])
                    nxt = min(opts, key=lambda o: abs(math.remainder(o[1] - h, 2 * math.pi)))[0]
                if total < 12.0 or len(chain) < 3:
                    continue
                loop = out_and_back_loop([pos[n] for n in chain], 0.9, net)
                if loop is None:
                    continue
                loop = simplify_loop(loop, LOOP_MAX_WP, net.segment_ok)
                loop = subdivide_loop(loop, LOOP_MIN_WP)
                if _loop_ok(loop, net):
                    cands.append({"kind": "out_and_back", "waypoints": loop, "nodes": frozenset(chain),
                                  "length": polyline_length([(p[0], p[1]) for p in loop], closed=True)})
    return cands


def select_loops(cands, count=LOOPS_MAX, min_count=LOOPS_MIN):
    """Greedy farthest-point pick: spread loops over the district, prefer true cycles, avoid shared sidewalk."""
    if not cands:
        return []
    cents = [_centroid(c["waypoints"]) for c in cands]
    chosen = []

    def score(i, jac_limit, far_min):
        c = cands[i]
        near = min((math.dist(cents[i], cents[j]) for j in chosen), default=1e9)
        if near < far_min:
            return None
        for j in chosen:
            u = len(c["nodes"] | cands[j]["nodes"])
            if u and len(c["nodes"] & cands[j]["nodes"]) / u > jac_limit:
                return None
        bonus = {"cycle": 30.0, "racetrack": -12.0}.get(c["kind"], 0.0)
        return min(near, 80.0) + bonus - abs(c["length"] - 65.0) * 0.1

    for jac, far in ((0.2, 20.0), (0.5, 10.0), (0.8, 4.0)):
        while len(chosen) < count:
            best, bs = None, None
            for i in range(len(cands)):
                if i in chosen:
                    continue
                sc = score(i, jac, far)
                if sc is not None and (bs is None or sc > bs):
                    best, bs = i, sc
            if best is None:
                break
            chosen.append(best)
        if len(chosen) >= min_count:
            break
    return [cands[i] for i in chosen]


# ---------------------------------------------------------------------------
# Parking
# ---------------------------------------------------------------------------

class RoadMask:
    """Raster of a ground class (road, or walkable ground). `data` is a numpy bool array indexed [row, col];
    row 0 is y = y0; `z` (optional, same shape) is the surface height of each covered cell."""

    def __init__(self, data, x0, y0, cell=MASK_CELL, z=None):
        self.data, self.x0, self.y0, self.cell, self.z = data, x0, y0, cell, z

    def at(self, x, y) -> bool:
        c = int(math.floor((x - self.x0) / self.cell))
        r = int(math.floor((y - self.y0) / self.cell))
        h, w = self.data.shape
        return 0 <= r < h and 0 <= c < w and bool(self.data[r, c])

    def z_at(self, x, y, default=0.0) -> float:
        if self.z is None:
            return default
        c = int(math.floor((x - self.x0) / self.cell))
        r = int(math.floor((y - self.y0) / self.cell))
        h, w = self.z.shape
        if 0 <= r < h and 0 <= c < w and self.data[r, c]:
            return float(self.z[r, c])
        return default


class RasterWalkable:
    """Walkable ground from a raster (fallback where the game has no sidewalk graph). Same interface as
    WalkableNet for the parts the loop code uses."""

    def __init__(self, mask: RoadMask):
        self.mask = mask

    def point_ok(self, p) -> bool:
        return self.mask.at(p[0], p[1])

    def segment_ok(self, a, b, step=SAMPLE_STEP) -> bool:
        return all(self.mask.at(x, y) for x, y in sample_segment(a, b, step))

    def rect_touches(self, corners) -> bool:
        return any(self.mask.at(c[0], c[1]) for c in corners)

    def z_at(self, p) -> float:
        return self.mask.z_at(p[0], p[1])


def dilate(a, iterations):
    """4-neighbour binary dilation (numpy bool array)."""
    import numpy as np
    a = a.copy()
    for _ in range(iterations):
        b = a.copy()
        b[1:] |= a[:-1]
        b[:-1] |= a[1:]
        b[:, 1:] |= a[:, :-1]
        b[:, :-1] |= a[:, 1:]
        a = b
    return a


def erode(a, iterations):
    import numpy as np
    return ~dilate(~np.asarray(a, dtype=bool), iterations)


def racetrack(cx, cy, angle, length, width):
    """4-waypoint closed loop: a long thin rectangle (out along one side, back along the other)."""
    c, s = math.cos(angle), math.sin(angle)
    hl, hw = length / 2.0, width / 2.0
    return [(cx + lx * c - ly * s, cy + lx * s + ly * c) for lx, ly in ((hl, -hw), (hl, hw), (-hl, hw), (-hl, -hw))]


def ring_candidates(net: RasterWalkable, bbox, lattice=5.0, radii=(4.6, 6.0, 8.5, 12.0, 16.0, 19.0)):
    """Closed walks inside a walkable raster: rings (6-8 waypoints on an ellipse) and racetracks (thin rectangles
    for strips and paths too narrow for a ring)."""
    cands = []
    x0, y0, x1, y1 = bbox
    gx = x0 + lattice / 2
    while gx < x1:
        gy = y0 + lattice / 2
        while gy < y1:
            if net.point_ok((gx, gy)):
                for ang_i in range(8):
                    ang = math.pi * ang_i / 8
                    for ln in (12.0, 20.0, 32.0, 48.0):
                        pts = racetrack(gx, gy, ang, ln, 1.4)
                        if all(net.point_ok(p) for p in pts) and all(
                                net.segment_ok(pts[i], pts[(i + 1) % 4]) for i in range(4)):
                            wps = [(p[0], p[1], net.z_at(p)) for p in pts]
                            cands.append({"kind": "racetrack", "waypoints": wps,
                                          "nodes": frozenset({("rt", round(gx), round(gy), ang_i, ln)}),
                                          "length": polyline_length(pts, closed=True)})
                for r in radii:
                    for aspect, phase in ((1.0, 0.0), (0.6, 0.0), (0.6, math.pi / 2)):
                        n = 6 if r < 7 else 8
                        rx, ry = r, r * aspect
                        pts = []
                        for k in range(n):
                            ang = 2 * math.pi * k / n + math.pi / 8
                            if phase:
                                pts.append((gx + ry * math.cos(ang), gy + rx * math.sin(ang)))
                            else:
                                pts.append((gx + rx * math.cos(ang), gy + ry * math.sin(ang)))
                        if all(net.point_ok(p) for p in pts) and all(
                                net.segment_ok(pts[i], pts[(i + 1) % n]) for i in range(n)):
                            wps = [(p[0], p[1], net.z_at(p)) for p in pts]
                            length = polyline_length(pts, closed=True)
                            if LOOP_MIN_M <= length <= LOOP_MAX_M:
                                cands.append({"kind": "ring", "waypoints": wps,
                                              "nodes": frozenset({("ring", round(gx), round(gy), r)}),
                                              "length": length})
            gy += lattice
        gx += lattice
    return cands


def parking_candidates_mask(mask: RoadMask, step=1.0, headings=24):
    """Kerbside spots straight from the road raster, for districts without a road graph. A spot (heading h, kerb on
    the right) needs: 5x2.4 m footprint all on road, road still present 1.3 m to the right of the centre line
    and none 1.9 m to the right along +-4.5 m (a straight kerb 1.6 m from the spot centre)."""
    import numpy as np
    data, cell = mask.data, mask.cell
    ys, xs = np.nonzero(data[::int(step / cell), ::int(step / cell)])
    step_px = int(step / cell)
    cx = mask.x0 + (xs * step_px + 0.5) * cell
    cy = mask.y0 + (ys * step_px + 0.5) * cell
    h, w = data.shape
    out = []

    def lookup(px, py):
        c = np.floor((px - mask.x0) / cell).astype(int)
        r = np.floor((py - mask.y0) / cell).astype(int)
        ok = (c >= 0) & (c < w) & (r >= 0) & (r < h)
        v = np.zeros(px.shape, bool)
        v[ok] = data[r[ok], c[ok]]
        return v

    body = [(l, v) for l in (-2.5, -1.25, 0.0, 1.25, 2.5) for v in (-1.2, 0.0, 1.2)]
    road_side = [(l, 1.3) for l in (-4.5, -3.0, -1.5, 0.0, 1.5, 3.0, 4.5)]
    off_side = [(l, 1.9) for l in (-4.5, -3.0, -1.5, 0.0, 1.5, 3.0, 4.5)]
    lane_side = [(l, -4.0) for l in (-4.5, -2.25, 0.0, 2.25, 4.5)]   # a travel lane on the other side (road >= ~6 m wide)
    for k in range(headings):
        th = 2 * math.pi * k / headings
        hx, hy = math.cos(th), math.sin(th)
        nx, ny = hy, -hx   # right of heading
        good = np.ones(cx.shape, bool)
        for pts, want in ((body, True), (road_side, True), (lane_side, True), (off_side, False)):
            for l, v in pts:
                got = lookup(cx + hx * l + nx * v, cy + hy * l + ny * v)
                good &= got if want else ~got
                if not good.any():
                    break
            if not good.any():
                break
        for i in np.nonzero(good)[0]:
            out.append({"pos": (float(cx[i]), float(cy[i]), mask.z_at(float(cx[i]), float(cy[i]))),
                        "heading": (hx, hy), "kerb": KERB_INSET, "link": None})
    return out


def kerb_distance(mask: RoadMask, p, nx, ny, max_d=14.0, gap=0.75, step=0.25):
    """Distance from road point p along unit normal (nx, ny) to the first non-road ground (the kerb), or None
    when p is not on the road or the road runs on past max_d."""
    if not mask.at(p[0], p[1]):
        return None
    d, run = 0.0, 0
    while d < max_d:
        d += step
        if mask.at(p[0] + nx * d, p[1] + ny * d):
            run = 0
        else:
            run += 1
            if run * step >= gap:
                return d - (run - 1) * step
    return None


def spot_corners(cx, cy, hx, hy):
    return rect_corners(cx, cy, math.atan2(hy, hx), CAR_LENGTH, CAR_WIDTH)


def rect_on_mask(mask: RoadMask, corners, step=0.5) -> bool:
    """Every sample on the rectangle's outline and centre cross lies on the road mask."""
    pts = []
    for i in range(4):
        pts.extend(sample_segment(corners[i], corners[(i + 1) % 4], step))
    cx = sum(c[0] for c in corners) / 4.0
    cy = sum(c[1] for c in corners) / 4.0
    pts.append((cx, cy))
    return all(mask.at(x, y) for x, y in pts)


def parking_candidates(car: Grid, node_ids, mask: RoadMask, net: WalkableNet, step=1.0,
                       straight_check=3.0, kerb_tol=0.8):
    """Kerbside spots along the road links among `node_ids`.

    For each side of each link: walk the link every `step` m, find the kerb with a ray along the side normal,
    put the spot centre KERB_INSET m out from the kerb (onto the road), heading parallel to the link and chosen
    so the kerb is on the car's right, and keep it when the kerb is straight over +-straight_check m, the whole
    5x2.4 m footprint is on the road mask, clear of sidewalk corridor and keep-out, and away from junctions."""
    adj = car.adjacency()
    junctions = [car.nodes[n]["pos"] for n in node_ids if len(adj[n]) >= 3]
    ids = set(node_ids)
    out, seen_links = [], set()
    for lk in car.links:
        a, b = lk["src"], lk["dest"]
        if a not in ids or b not in ids or a == b:
            continue
        key = frozenset((a, b))
        if key in seen_links:
            continue
        seen_links.add(key)
        pa, pb = car.nodes[a]["pos"], car.nodes[b]["pos"]
        length = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
        if length < 4.0:
            continue
        ux, uy = (pb[0] - pa[0]) / length, (pb[1] - pa[1]) / length
        for side in (1, -1):
            nx, ny = side * uy, -side * ux     # side=+1 -> right of a->b
            hx, hy = (ux, uy) if side == 1 else (-ux, -uy)   # kerb on the car's right
            t = 0.0
            while t <= length:
                cx0, cy0 = pa[0] + ux * t, pa[1] + uy * t
                t += step
                ks = []
                for off in (-straight_check, 0.0, straight_check):
                    q = (cx0 + ux * off, cy0 + uy * off)
                    k = kerb_distance(mask, q, nx, ny)
                    if k is None:
                        ks = None
                        break
                    ks.append(k)
                if ks is None or max(ks) - min(ks) > kerb_tol:
                    continue
                k = ks[1]
                lat = k - KERB_INSET
                if lat < 1.2:
                    continue
                cx, cy = cx0 + nx * lat, cy0 + ny * lat
                if any(math.hypot(cx - j[0], cy - j[1]) < JUNCTION_CLEARANCE for j in junctions):
                    continue
                corners = spot_corners(cx, cy, hx, hy)
                if not rect_on_mask(mask, corners) or net.rect_touches(corners):
                    continue
                z = pa[2] + (pb[2] - pa[2]) * (t - step) / length
                out.append({"pos": (cx, cy, z), "heading": (hx, hy), "kerb": k, "link": lk["id"]})
    return out


def select_parking(cands, count=PARK_MAX, spacing=PARK_SPACING, valid=None, existing=()):
    """Greedy farthest-point pick with a hard minimum centre spacing. `valid(c)` is a lazy extra check applied only
    to a candidate that is about to be chosen; `existing` spots count for spacing but are not returned."""
    chosen = []
    pool = list(cands)
    while pool and len(chosen) < count:
        def near(c):
            return min((math.dist(c["pos"][:2], s["pos"][:2]) for s in list(chosen) + list(existing)), default=1e9)
        scored = [(min(near(c), 60.0), c) for c in pool]
        scored = [(d, c) for d, c in scored if d >= spacing]
        if not scored:
            break
        scored.sort(key=lambda t: -t[0])
        best, bad = None, set()
        for _d, c in scored[:400]:
            if valid is None or valid(c):
                best = c
                break
            bad.add(id(c))
        pool = [c for c in pool if id(c) not in bad]
        if best is None:
            if bad and len(pool) > 0 and len(bad) >= 400:
                continue
            break
        chosen.append(best)
        pool = [c for c in pool if c is not best]
    return chosen


# ---------------------------------------------------------------------------
# District assignment
# ---------------------------------------------------------------------------

def aabb_distance(p, box) -> float:
    dx = max(box[0] - p[0], 0.0, p[0] - box[2])
    dy = max(box[1] - p[1], 0.0, p[1] - box[3])
    return math.hypot(dx, dy)


def assign_nodes(nodes, footprints, max_dist=25.0, bias=None):
    """{node id: scene stem} for the nearest footprint. footprints: {scene: [(x0, y0, x1, y1), ...]}.
    Ties (overlapping boxes) go to the smaller box, so big relief slabs lose to the tile they cover; `bias`
    {scene: metres} is added to a scene's distance (tGlobal, the always-loaded layer, is penalised)."""
    bias = bias or {}
    flat = [(sc, b, (b[2] - b[0]) * (b[3] - b[1])) for sc, boxes in footprints.items() for b in boxes]
    out = {}
    for nid, nd in nodes.items():
        p = nd["pos"]
        best = None
        for sc, b, area in flat:
            d = aabb_distance(p, b)
            if d > max_dist:
                continue
            key = (round(d + bias.get(sc, 0.0), 1), area)
            if best is None or key < best[0]:
                best = (key, sc)
        if best is not None:
            out[nid] = best[1]
    return out


def placement_aabb(pl, bounds):
    """World XY box of a placement from its model's bounds_game and the manifest transform."""
    lo, hi = bounds
    r = pl["game"]["rotation_rows"]
    s = pl["game"]["scale"]
    t = pl["game"]["position"]
    xs, ys = [], []
    for cx in (lo[0], hi[0]):
        for cy in (lo[1], hi[1]):
            for cz in (lo[2], hi[2]):
                v = (cx * s[0], cy * s[1], cz * s[2])
                xs.append(r[0][0] * v[0] + r[0][1] * v[1] + r[0][2] * v[2] + t[0])
                ys.append(r[1][0] * v[0] + r[1][1] * v[1] + r[1][2] * v[2] + t[1])
    return (min(xs), min(ys), max(xs), max(ys))


# ---------------------------------------------------------------------------
# Terrain, markers (numpy / Pillow)
# ---------------------------------------------------------------------------

def _load_gltf_prims(path):
    import numpy as np
    from .preview import _accessor
    with open(path) as f:
        doc = json.load(f)
    folder = os.path.dirname(path)
    bins = [open(os.path.join(folder, b["uri"]), "rb").read() for b in doc["buffers"]]
    prims = []
    for prim in doc["meshes"][0]["primitives"]:
        pos = _accessor(doc, bins, prim["attributes"]["POSITION"]).astype(np.float64)
        game = np.stack([pos[:, 0], -pos[:, 2], pos[:, 1]], axis=1)
        idx = _accessor(doc, bins, prim["indices"]).astype(np.int64).reshape(-1, 3)
        mat = doc["materials"][prim["material"]] if "material" in prim else {}
        base = mat.get("extras", {}).get("textures", {}).get("base", {})
        uri = base.get("uri")
        prims.append({"pos": game, "idx": idx, "tex_ref": base.get("ref", ""),
                      "tex_path": os.path.normpath(os.path.join(folder, uri)) if uri else None,
                      "blend": mat.get("extras", {}).get("blend_mode", "opaque")})
    return prims


def world_triangles(pl, prims):
    import numpy as np
    r = np.array(pl["game"]["rotation_rows"])
    s = np.array(pl["game"]["scale"])
    t = np.array(pl["game"]["position"])
    out = []
    for p in prims:
        w = (r @ (p["pos"] * s).T).T + t
        out.append((w[p["idx"]], p))
    return out


def load_marker_geometry(out_dir, manifest, scene_stems, log=print):
    """NOGO / WALKABLE marker triangles per scene from the converted marker models. Returns
    (nogo_by_scene {scene: [tri...]}, stats). One-triangle models are the game's placeholders and are ignored."""
    nogo, stats = {}, {"markers_placed": 0, "markers_stub_single_triangle": 0, "markers_real": [], "markers_missing": 0}
    cache = {}
    for pl in manifest["placements"]:
        if pl["scene"] not in scene_stems or pl["visible"]:
            continue
        m = pl["model"].lower()
        if not (m.startswith(("nogo_", "nog_", "walkable_"))):
            continue
        stats["markers_placed"] += 1
        gltf = os.path.join("markers", "models", safe_name(pl["model"]) + ".gltf")
        path = os.path.join(out_dir, gltf)
        if not os.path.exists(path):
            stats["markers_missing"] += 1
            continue
        if pl["model"] not in cache:
            cache[pl["model"]] = _load_gltf_prims(path)
        tris = world_triangles(pl, cache[pl["model"]])
        ntri = sum(len(t) for t, _ in tris)
        if ntri <= 1:
            stats["markers_stub_single_triangle"] += 1
            continue
        stats["markers_real"].append({"model": pl["model"], "scene": pl["scene"], "triangles": ntri})
        if m.startswith(("nogo_", "nog_")):
            lst = nogo.setdefault(pl["scene"], [])
            for t, _ in tris:
                lst.extend([tuple(map(tuple, tr)) for tr in t])
    return nogo, stats


def convert_markers(install, out_dir, manifest, scene_stems, log=print):
    """Convert the nogo_/walkable_ marker models placed in `scene_stems` into <out_dir>/markers/models/
    (a separate tree, so imported/models and the scene manifest are untouched)."""
    from .binreader import FormatError
    from .convert import TextureStore, convert_model
    from .ide import ModelDefs
    from .img import ImgArchive
    names = sorted({pl["model"] for pl in manifest["placements"]
                    if pl["scene"] in scene_stems and not pl["visible"]
                    and pl["model"].lower().startswith(("nogo_", "nog_", "walkable_"))})
    mdir = os.path.join(out_dir, "markers")
    todo = [n for n in names if not os.path.exists(os.path.join(mdir, "models", safe_name(n) + ".gltf"))]
    if not todo:
        return len(names)
    with ImgArchive(os.path.join(install, "Stream", "World.img")) as img:
        defs = ModelDefs.load_install(install)
        store = TextureStore(img, mdir)
        for n in todo:
            e = img.find(n + ".nif")
            if e is None:
                log(f"marker {n}: not in World.img")
                continue
            ide = defs.get(e.stem)
            try:
                convert_model(e.stem, img.read_entry(e), ide.txd if ide else None, store, mdir, entity_space=True)
            except FormatError as ex:
                log(f"marker {n}: FAILED {ex}")
    return len(names)


class Terrain:
    """World-space ground triangles of the exterior scenes plus the data for the road mask and previews."""

    def __init__(self, tris, road, colors, zmax_tris, nonwalk=None, strict=None, water_z=None):
        import numpy as np
        self.tris, self.road, self.colors = tris, road, colors
        self.zmax = zmax_tris
        self.nonwalk = nonwalk if nonwalk is not None else np.zeros(len(tris), bool)
        self.strict = strict if strict is not None else road
        self.water_z = water_z

    @classmethod
    def load(cls, out_dir, manifest, scenes, log=print):
        import numpy as np
        try:
            from PIL import Image
        except ImportError:  # pragma: no cover
            Image = None
        cache, colcache = {}, {}

        def mean_colour(path, ref):
            if path in colcache:
                return colcache[path]
            col = None
            if Image is not None and path and os.path.exists(path):
                with Image.open(path) as im:
                    col = np.asarray(im.convert("RGB").resize((1, 1), Image.BOX), dtype=np.float32)[0, 0]
            if col is None:
                h = (hash(ref) & 0xFFFFFF)
                col = np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], dtype=np.float32) * 0.5 + 64
            colcache[path] = col
            return col

        tri_l, road_l, col_l, nw_l, st_l, wt_l = [], [], [], [], [], []
        for pl in manifest["placements"]:
            if pl["scene"] not in scenes or not pl["gltf"] or not pl["visible"]:
                continue
            if pl["gltf"] not in cache:
                cache[pl["gltf"]] = _load_gltf_prims(os.path.join(out_dir, pl["gltf"]))
            for w, p in world_triangles(pl, cache[pl["gltf"]]):
                if p["blend"] in ("additive",):
                    continue
                tri_l.append(w.astype(np.float32))
                road_l.append(np.full(len(w), bool(ROAD_TEXTURE.search(p["tex_ref"] or ""))))
                st_l.append(np.full(len(w), bool(STRICT_ROAD_TEXTURE.search(p["tex_ref"] or ""))))
                wt_l.append(np.full(len(w), bool(WATER_TEXTURE.search(p["tex_ref"] or ""))))
                nw_l.append(np.full(len(w), bool(NONWALK_TEXTURE.search(p["tex_ref"] or "")
                                                 or p["blend"] != "opaque")))
                col_l.append(np.tile(mean_colour(p["tex_path"], p["tex_ref"]), (len(w), 1)))
        if not tri_l:
            return cls(np.zeros((0, 3, 3), np.float32), np.zeros(0, bool), np.zeros((0, 3), np.float32), np.zeros(0))
        tris = np.concatenate(tri_l)
        n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
        keep = np.linalg.norm(n, axis=1) > 1e-9
        tris, road, colors = tris[keep], np.concatenate(road_l)[keep], np.concatenate(col_l)[keep]
        nonwalk = np.concatenate(nw_l)[keep]
        strict = np.concatenate(st_l)[keep]
        water = np.concatenate(wt_l)[keep]
        water_z = float(np.median(tris[water][:, :, 2].mean(1))) if water.any() else None
        log(f"terrain: {len(tris)} triangles, {int(road.sum())} road-textured, water level {water_z}")
        return cls(tris, road, colors, tris[:, :, 2].max(axis=1), nonwalk, strict, water_z)

    def _in_box(self, bbox):
        t = self.tris
        x0, y0, x1, y1 = bbox
        return (t[:, :, 0].max(1) >= x0) & (t[:, :, 0].min(1) <= x1) \
            & (t[:, :, 1].max(1) >= y0) & (t[:, :, 1].min(1) <= y1)

    def _normals_z(self):
        import numpy as np
        if getattr(self, "_nz", None) is None:
            t = self.tris
            n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
            self._nz = n[:, 2] / np.maximum(np.linalg.norm(n, axis=1), 1e-9)
        return self._nz

    def _paint(self, bbox, sel, cell):
        """Rasterise the selected triangles (lowest first, so the highest surface wins) into a coverage mask and a
        height raster."""
        import numpy as np
        from PIL import Image, ImageDraw
        x0, y0, x1, y1 = bbox
        w, h = int(math.ceil((x1 - x0) / cell)), int(math.ceil((y1 - y0) / cell))
        mimg = Image.new("L", (w, h), 0)
        zimg = Image.new("F", (w, h), -1e9)
        dm, dz = ImageDraw.Draw(mimg), ImageDraw.Draw(zimg)
        idx = np.nonzero(sel)[0]
        zmean = self.tris[idx][:, :, 2].mean(1)
        for i in idx[np.argsort(zmean, kind="stable")]:
            tri = self.tris[i]
            pts = [((v[0] - x0) / cell, (v[1] - y0) / cell) for v in tri]
            dm.polygon(pts, fill=255)
            dz.polygon(pts, fill=float(tri[:, 2].mean()))
        return np.asarray(mimg, dtype=bool), np.asarray(zimg, dtype=np.float32)

    def road_mask(self, bbox, cell=MASK_CELL, strict=False) -> RoadMask:
        sel = self._in_box(bbox) & (self.strict if strict else self.road) & (self._normals_z() > 0.5)
        data, z = self._paint(bbox, sel, cell)
        return RoadMask(data, bbox[0], bbox[1], cell, z)

    def walkable_mask(self, bbox, cell=0.5, clearance=0.5) -> RoadMask:
        """Fallback walkable ground: flat (normal z >= 0.85), not road / roof / water / metal, minus 'blockers' (tall
        steep faces: walls, fences, cliffs), eroded by `clearance` m."""
        import numpy as np
        from PIL import Image, ImageDraw
        inbox = self._in_box(bbox)
        nz = self._normals_z()
        walk = inbox & (nz >= 0.85) & ~self.road & ~self.nonwalk
        if self.water_z is not None:
            walk &= self.tris[:, :, 2].mean(1) > self.water_z + 0.25   # above the waterline
        data, z = self._paint(bbox, walk, cell)
        x0, y0, x1, y1 = bbox
        w, h = data.shape[1], data.shape[0]
        bimg = Image.new("L", (w, h), 0)
        bd = ImageDraw.Draw(bimg)
        tall = inbox & (nz < 0.5) & ((self.tris[:, :, 2].max(1) - self.tris[:, :, 2].min(1)) > 1.5)
        for tri in self.tris[tall]:
            pts = [((v[0] - x0) / cell, (v[1] - y0) / cell) for v in tri]
            bd.polygon(pts, fill=255)
            bd.line(pts + [pts[0]], fill=255, width=1)
        blocked = np.asarray(bimg, dtype=bool)
        ok = data & ~dilate(blocked, int(round(clearance / cell)))
        ok = erode(ok, int(round(clearance / cell)))
        return RoadMask(ok, x0, y0, cell, z)

    def height_at(self, pts, z_hint):
        """Median-style helper: ground height under each (x, y) nearest to its hint z; None when uncovered."""
        import numpy as np
        t = self.tris
        mnx, mxx = t[:, :, 0].min(1), t[:, :, 0].max(1)
        mny, mxy = t[:, :, 1].min(1), t[:, :, 1].max(1)
        res = []
        for (x, y), zh in zip(pts, z_hint):
            c = np.nonzero((mnx <= x) & (mxx >= x) & (mny <= y) & (mxy >= y))[0]
            best = None
            for i in c:
                a, b, d = t[i]
                den = (b[1] - d[1]) * (a[0] - d[0]) + (d[0] - b[0]) * (a[1] - d[1])
                if abs(den) < 1e-9:
                    continue
                w0 = ((b[1] - d[1]) * (x - d[0]) + (d[0] - b[0]) * (y - d[1])) / den
                w1 = ((d[1] - a[1]) * (x - d[0]) + (a[0] - d[0]) * (y - d[1])) / den
                w2 = 1 - w0 - w1
                if min(w0, w1, w2) < -1e-6:
                    continue
                z = w0 * a[2] + w1 * b[2] + w2 * d[2]
                if best is None or abs(z - zh) < abs(best - zh):
                    best = z
            res.append(None if best is None else float(best))
        return res


# ---------------------------------------------------------------------------
# Evidence render
# ---------------------------------------------------------------------------

LOOP_COLOURS = [(230, 25, 75), (60, 180, 75), (255, 225, 25), (0, 130, 200), (245, 130, 48), (145, 30, 180)]


def render_district(path, name, bbox, terrain: Terrain | None, ped_segs, car_segs, loops, spots, radius,
                    nogo: Nogo | None = None, size=1600, overlays=()):
    import numpy as np
    from PIL import Image, ImageDraw
    x0, y0, x1, y1 = bbox
    scale = (size - 20) / max(x1 - x0, y1 - y0)
    W, H = int((x1 - x0) * scale) + 20, int((y1 - y0) * scale) + 20

    def px(p):
        return ((p[0] - x0) * scale + 10, (y1 - p[1]) * scale + 10)

    img = Image.new("RGB", (W, H), (60, 70, 80))
    dr = ImageDraw.Draw(img)
    if terrain is not None and len(terrain.tris):
        t = terrain.tris
        sel = (t[:, :, 0].max(1) >= x0) & (t[:, :, 0].min(1) <= x1) & (t[:, :, 1].max(1) >= y0) & (t[:, :, 1].min(1) <= y1)
        idx = np.nonzero(sel)[0]
        idx = idx[np.argsort(terrain.zmax[idx], kind="stable")]
        sun = np.array([0.4, 0.3, 0.86])
        sun /= np.linalg.norm(sun)
        for i in idx:
            tri = t[i]
            fn = np.cross(tri[1] - tri[0], tri[2] - tri[0])
            ln = np.linalg.norm(fn)
            if ln < 1e-9:
                continue
            nz = abs(float(fn @ sun)) / ln
            c = np.clip(terrain.colors[i] * (0.45 + 0.55 * nz), 0, 255).astype(int)
            dr.polygon([px(v) for v in tri], fill=(int(c[0]), int(c[1]), int(c[2])))
    ov = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    for m, rgba in overlays:                   # walkable-ground rasters (fallback districts)
        a = (m.data.astype(np.uint8) * 255)[::-1]
        im = Image.fromarray(a, "L")
        size_px = (max(1, int(round(m.data.shape[1] * m.cell * scale))), max(1, int(round(m.data.shape[0] * m.cell * scale))))
        im = im.resize(size_px, Image.NEAREST).point(lambda v, al=rgba[3]: al if v else 0)
        lay = Image.new("RGBA", (W, H), rgba[:3] + (0,))
        lay.paste(Image.new("RGBA", size_px, rgba[:3] + (255,)),
                  (int((m.x0 - x0) * scale + 10), int((y1 - (m.y0 + m.data.shape[0] * m.cell)) * scale + 10)), im)
        ov = Image.alpha_composite(ov, lay)
        od = ImageDraw.Draw(ov)
    rw = max(2, int(radius * 2 * scale))
    for a, b in ped_segs:                      # walkable corridor
        od.line([px(a), px(b)], fill=(0, 220, 255, 70), width=rw)
    for a, b in ped_segs:                      # sidewalk graph centreline
        od.line([px(a), px(b)], fill=(0, 90, 160, 200), width=1)
    for a, b in car_segs:                      # road graph
        od.line([px(a), px(b)], fill=(40, 40, 40, 170), width=2)
    if nogo:
        for a, b in nogo.walls:
            od.line([px(a), px(b)], fill=(255, 0, 0, 255), width=3)
        for _bb, poly in nogo.polys:
            od.polygon([px(v) for v in poly], fill=(255, 0, 0, 90))
    for k, lp in enumerate(loops):
        col = LOOP_COLOURS[k % len(LOOP_COLOURS)] + (255,)
        pts = [px(p) for p in lp["waypoints"]]
        od.line(pts + [pts[0]], fill=col, width=4)
        for j, q in enumerate(pts):
            od.ellipse([q[0] - 5, q[1] - 5, q[0] + 5, q[1] + 5], fill=col, outline=(0, 0, 0, 255))
        od.text((pts[0][0] + 7, pts[0][1] - 14), f"L{k + 1}", fill=(255, 255, 255, 255))
    for sp in spots:
        hx, hy = sp["heading"]
        corners = spot_corners(sp["pos"][0], sp["pos"][1], hx, hy)
        od.polygon([px(c) for c in corners], fill=(255, 140, 0, 200), outline=(0, 0, 0, 255))
        c0, c1 = px(sp["pos"]), px((sp["pos"][0] + hx * 2.2, sp["pos"][1] + hy * 2.2))
        od.line([c0, c1], fill=(255, 255, 255, 255), width=2)
    img = Image.alpha_composite(img.convert("RGBA"), ov).convert("RGB")
    d2 = ImageDraw.Draw(img)
    d2.rectangle([0, 0, W, 16], fill=(0, 0, 0))
    d2.text((6, 2), f"{name}: {len(loops)} loops, {len(spots)} parking | x {x0:.0f}..{x1:.0f} y {y0:.0f}..{y1:.0f} m "
                    f"| cyan=walkable corridor, dark=road graph, colour=loops, orange=parked cars", fill=(255, 255, 255))
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    img.save(path)
    return path


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def _ue(p):
    return [round(v, 2) for v in coords.game_to_ue_position(p)]


def district_boxes(stem, manifest, footprints):
    """Boxes (x0, y0, x1, y1) in which fallback loops / parking are generated for a district. A normal district uses
    the robust extent of its own placements; tGlobal (the world-wide relief layer) uses +-90 m around its
    WALKABLE_* marker placements (the shore strips)."""
    if stem == "tGlobal":
        out = []
        for pl in manifest["placements"]:
            if pl["scene"] == stem and pl["model"].lower().startswith("walkable_"):
                x, y = pl["game"]["position"][0], pl["game"]["position"][1]
                out.append((x - 90, y - 90, x + 90, y + 90))
        return out
    boxes = [b for b in footprints.get(stem, []) if max(b[2] - b[0], b[3] - b[1]) < 150]
    if not boxes:
        return []
    xs = sorted((b[0] + b[2]) / 2 for b in boxes)
    ys = sorted((b[1] + b[3]) / 2 for b in boxes)
    lo, hi = int(len(xs) * 0.03), max(int(len(xs) * 0.97) - 1, 0)
    return [(xs[lo] - 20, ys[lo] - 20, xs[hi] + 20, ys[hi] + 20)]


def _union(boxes):
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def build_townlife(install, out_dir, only=None, evidence_dir=None, raster_parking=False, log=print):
    t0 = time.time()
    import numpy as np  # noqa: F401  (terrain needs it; fail early with a clear message)
    with open(os.path.join(out_dir, "scenes", "academy.json")) as f:
        manifest = json.load(f)
    with open(os.path.join(install, "Config", "GRIDS.DAT"), "rb") as f:
        grids = parse_grids(f.read().decode("latin-1"))
    log(f"GRIDS.DAT: ped {len(grids.ped.nodes)} nodes / {len(grids.ped.links)} links, "
        f"car {len(grids.car.nodes)} nodes / {len(grids.car.links)} links")

    wanted = [d for d in DISTRICTS if not only or d[0] in only or d[1] in only]
    stems = {d[1] for d in wanted}
    convert_markers(install, out_dir, manifest, stems, log)
    nogo_tris, marker_stats = load_marker_geometry(out_dir, manifest, stems, log)

    footprints = {}
    for pl in manifest["placements"]:
        m = manifest["models"].get(pl["model"])
        if not m or not m.get("bounds_game"):
            continue
        footprints.setdefault(pl["scene"], []).append(placement_aabb(pl, m["bounds_game"]))
    ext = {s["name"] for s in manifest["scenes"] if s["kind"] == "exterior"}
    terrain = Terrain.load(out_dir, manifest, ext, log)

    # Node ownership: nearest placement footprint (any scene, interiors and the academy included so their nodes are
    # not mistaken for town nodes); nodes near nothing are leftover exterior ground, which is tGlobal's. A node must
    # also sit on terrain (within 3.5 m of a ground surface), which drops interior / floating nodes.
    bias = {"tGlobal": 8.0}
    owners = {}
    for kind, grid in (("ped", grids.ped), ("car", grids.car)):
        own = assign_nodes(grid.nodes, footprints, bias=bias)
        for n in grid.nodes:
            own.setdefault(n, "tGlobal")
        cand = [n for n, sc in own.items() if sc in stems]
        hs = terrain.height_at([grid.nodes[n]["pos"][:2] for n in cand], [grid.nodes[n]["pos"][2] for n in cand])
        drop = {n for n, h in zip(cand, hs) if h is None or abs(h - grid.nodes[n]["pos"][2]) > 3.5}
        owners[kind] = {n: sc for n, sc in own.items() if sc in stems and n not in drop}
        log(f"{kind} nodes: {len(owners[kind])} on town terrain, {len(drop)} dropped (not on terrain)")
    ped_owner, car_owner = owners["ped"], owners["car"]

    ped_adj_all = grids.ped.adjacency()
    car_adj_all = grids.car.adjacency()
    segs_all = [(grids.ped.nodes[n]["pos"][:2], grids.ped.nodes[m]["pos"][:2])
                for n, nb in ped_adj_all.items() for m in nb if n < m]
    result = {
        "format": FORMAT,
        "generated_by": "tools/importers townlife",
        "units": {"waypoints_cm": "UE centimetres (x, y, z): game (x, y, z) -> (100x, -100y, 100z)",
                  "length_m": "metres", "yaw_deg": "UE yaw, degrees, from +X toward +Y"},
        "source": {
            "walkable": "Config/GRIDS.DAT sidewalk graph (the shipped WALKABLE_*OP markers are 1-triangle placeholders); "
                        "districts without sidewalk data fall back to rings on flat terrain (kind 'ring')",
            "roads": "Config/GRIDS.DAT road graph + terrain textures matching street/asphalt/cobroad (the kerb is "
                     "the edge of that ground); districts without a road graph get no parking unless "
                     "--raster-parking is given",
            "walk_radius_m": WALK_RADIUS, "car_footprint_m": [CAR_LENGTH, CAR_WIDTH], "kerb_inset_m": KERB_INSET,
            "markers": marker_stats,
        },
        "districts": [],
    }
    for name, stem, desc in wanted:
        ids = [n for n, s in ped_owner.items() if s == stem
               and not grids.ped.nodes[n]["flags"].get("WATERNODE") and not grids.ped.nodes[n]["flags"].get("SWITCHEDOFF")]
        idset = set(ids)
        pos = {n: grids.ped.nodes[n]["pos"] for n in ids}
        adj = {n: {m: d for m, d in ped_adj_all[n].items() if m in idset and abs(pos[m][2] - pos[n][2]) < 4.0}
               for n in ids}
        nogo = Nogo.from_triangles(nogo_tris.get(stem, []))
        net = WalkableNet(segs_all, WALK_RADIUS, nogo)
        boxes = district_boxes(stem, manifest, footprints)
        notes, overlays = [], []

        # ---- loops: sidewalk graph first, ring fallback on terrain
        cands = candidate_loops(adj, pos, net) if ids else []
        chosen = select_loops(cands)
        for c in chosen:
            c["source"] = "sidewalk_graph"
        if len(chosen) < LOOPS_MIN and boxes and len(terrain.tris):
            before = len(chosen)
            ring = []
            for b in boxes:
                wm = terrain.walkable_mask(b)
                overlays.append((wm, (60, 220, 90, 70)))
                ring.extend(ring_candidates(RasterWalkable(wm), b,
                                            lattice=2.5 if (b[2] - b[0]) * (b[3] - b[1]) < 2e5 else 5.0))
            chosen = chosen + select_loops(ring, count=max(LOOPS_MIN - before, 0) + (3 if not before else 0),
                                           min_count=1)
            for c in chosen[before:]:
                c["source"] = "terrain_ring"
            notes.append(f"{before} loops from the sidewalk graph ({len(ids)} nodes); "
                         f"{len(chosen) - before} ring loops synthesised on flat terrain ({len(ring)} candidates)")
        loops_out = []
        for k, c in enumerate(chosen):
            loops_out.append({"id": f"{name}_loop{k + 1}", "kind": c["kind"], "source": c["source"],
                              "waypoints_cm": [_ue(p) for p in c["waypoints"]],
                              "length_m": round(c["length"], 1)})
        if len(chosen) < LOOPS_MIN:
            notes.append(f"only {len(chosen)} loops (wanted {LOOPS_MIN}-{LOOPS_MAX})")

        # ---- parking: road graph first, road raster for districts / stretches without one
        cids = [n for n, s in car_owner.items() if s == stem]
        cset = set(cids)
        car_segs = [(grids.car.nodes[n]["pos"][:2], grids.car.nodes[m]["pos"][:2])
                    for n in cids for m in car_adj_all[n] if n < m and m in cset]
        pts_all = [pos[n][:2] for n in ids] + [grids.car.nodes[n]["pos"][:2] for n in cids]
        pts_all += [(w[0], w[1]) for c in chosen for w in c["waypoints"]]
        for b in boxes:
            pts_all += [(b[0], b[1]), (b[2], b[3])]
        bbox = None
        if pts_all:
            xs, ys = [p[0] for p in pts_all], [p[1] for p in pts_all]
            bbox = (min(xs) - 25, min(ys) - 25, max(xs) + 25, max(ys) + 25)
        sp = []
        graph_spots = 0
        if len(terrain.tris) and bbox is not None:
            if cids:
                mask = terrain.road_mask(bbox)
                sp = select_parking(parking_candidates(grids.car, cids, mask, net))
                for s in sp:
                    s["source"] = "road_graph"
                graph_spots = len(sp)
            if len(sp) < PARK_MIN and boxes and raster_parking:
                pb = _union(boxes)
                mask2 = terrain.road_mask(pb, strict=True)
                cands2 = parking_candidates_mask(mask2)

                def valid(c, mask2=mask2, net=net):
                    corners = spot_corners(c["pos"][0], c["pos"][1], *c["heading"])
                    return rect_on_mask(mask2, corners) and not net.rect_touches(corners)
                extra = select_parking(cands2, count=PARK_MAX - len(sp), valid=valid, existing=sp)
                for s in extra:
                    s["source"] = "road_raster"
                sp = sp + extra
                if extra:
                    notes.append(f"{graph_spots} parking spots along road-graph links, {len(extra)} from the road "
                                 f"raster ({len(cands2)} raster candidates)")
        if len(sp) < PARK_MIN:
            notes.append(f"only {len(sp)} parking spots (wanted {PARK_MIN}-{PARK_MAX}): {len(cids)} road-graph nodes "
                         f"in this district" + ("" if raster_parking else
                                                " (road-raster fallback is off: pass --raster-parking)"))
        spots_out = [{"location_cm": _ue(s["pos"]), "yaw_deg": round(heading_yaw_deg_ue(*s["heading"]), 1),
                      "source": s["source"]} for s in sp]

        zdev = None
        if ids and len(terrain.tris):
            sample = ids[::max(1, len(ids) // 60)]
            hs = terrain.height_at([pos[n][:2] for n in sample], [pos[n][2] for n in sample])
            diffs = sorted(abs(h - pos[n][2]) for n, h in zip(sample, hs) if h is not None)
            if diffs:
                zdev = {"samples": len(diffs), "median_abs_m": round(diffs[len(diffs) // 2], 2),
                        "p90_abs_m": round(diffs[int(len(diffs) * 0.9)], 2)}
        result["districts"].append({
            "name": name, "ipb": stem, "description": desc,
            "loops": loops_out, "parking": spots_out,
            "stats": {"sidewalk_nodes": len(ids), "road_nodes": len(cids), "loop_candidates": len(cands),
                      "loop_sources": sorted({c["source"] for c in chosen}),
                      "node_z_vs_terrain": zdev, "nogo_polys": len(nogo.polys), "nogo_walls": len(nogo.walls)},
            "notes": notes,
        })
        log(f"{name}: {len(ids)} sidewalk nodes, {len(cids)} road nodes -> {len(loops_out)} loops, "
            f"{len(spots_out)} parking")
        if evidence_dir and bbox is not None:
            seg_d = [(pos[n][:2], pos[m][:2]) for n in ids for m in adj[n] if n < m]
            render_district(os.path.join(evidence_dir, f"{name}.png"), name, bbox, terrain, seg_d, car_segs,
                            chosen, sp, WALK_RADIUS, nogo, overlays=overlays)
    result["wall_seconds"] = round(time.time() - t0, 1)
    path = os.path.join(out_dir, "townlife.json")
    with open(path, "w") as f:
        json.dump(result, f, indent=1)
    log(f"wrote {path}")
    return result
