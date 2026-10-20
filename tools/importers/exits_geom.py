# OWNER: builder (phase2-doors)
"""Geometry checks for generated door links (CPU, numpy). Used by `exits` to refine and verify the arrival
direction: is there floor under the exit point and a clear walkable run in each of the four world axis directions?

Triangles are the imported models placed with the manifest's GAME-space transform (the same data UE gets, where
every mesh is complex-as-simple collision). All units are game metres, +Z up.
"""

from __future__ import annotations

import os

import numpy as np

BODY_HEIGHTS = (0.3, 0.8, 1.25)     # horizontal rays at these heights above the floor stand in for the capsule
CARDINALS = {"E": (1.0, 0.0), "W": (-1.0, 0.0), "N": (0.0, 1.0), "S": (0.0, -1.0)}


class _NoTex:
    def get(self, path):
        return None


def aabb_of_placement(pl, bounds):
    """World AABB (lo, hi) of a placement from its model's game-space bounds."""
    r = np.array(pl["game"]["rotation_rows"], dtype=float)
    s = np.array(pl["game"]["scale"], dtype=float)
    t = np.array(pl["game"]["position"], dtype=float)
    lo, hi = np.array(bounds[0], dtype=float), np.array(bounds[1], dtype=float)
    corners = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
    w = (r @ (corners * s).T).T + t
    return w.min(0), w.max(0)


class World:
    """Placement index + lazily loaded placed triangles from the converted glTF models."""

    def __init__(self, out_dir, manifest, log=print):
        from .preview import load_model
        self._load_model = load_model
        self.out_dir = out_dir
        self.log = log
        bounds = {m["gltf"]: m["bounds_game"] for m in manifest["models"].values() if m.get("gltf") and m.get("bounds_game")}
        self.pl = []
        lo, hi = [], []
        for pl in manifest["placements"]:
            if not pl.get("gltf") or not pl.get("visible") or pl["gltf"] not in bounds:
                continue
            a, b = aabb_of_placement(pl, bounds[pl["gltf"]])
            self.pl.append(pl)
            lo.append(a)
            hi.append(b)
        self.lo = np.array(lo) if lo else np.zeros((0, 3))
        self.hi = np.array(hi) if hi else np.zeros((0, 3))
        self._models = {}
        self._tris = {}
        self._tex = _NoTex()

    def scene_extents(self):
        """{scene: (lo, hi)}: union of the placed model boxes (not just the origins) per scene, game metres."""
        out = {}
        for i, pl in enumerate(self.pl):
            lo, hi = self.lo[i], self.hi[i]
            if pl["scene"] in out:
                out[pl["scene"]] = (np.minimum(out[pl["scene"]][0], lo), np.maximum(out[pl["scene"]][1], hi))
            else:
                out[pl["scene"]] = (lo.copy(), hi.copy())
        return {k: (list(map(float, v[0])), list(map(float, v[1]))) for k, v in out.items()}

    def solids(self, x, y, z, radius=15.0, min_height=2.0, max_extent=40.0):
        """[(cx, cy, volume)] of nearby tall, non-terrain placements: input for exits.prior_outdoor."""
        if not len(self.lo):
            return []
        ext = self.hi - self.lo
        near = ((self.lo[:, 0] <= x + radius) & (self.hi[:, 0] >= x - radius) & (self.lo[:, 1] <= y + radius)
                & (self.hi[:, 1] >= y - radius) & (self.lo[:, 2] <= z + 3) & (self.hi[:, 2] >= z + min_height)
                & (ext[:, :2].max(1) <= max_extent) & (ext[:, 2] >= min_height))
        out = []
        for i in np.nonzero(near)[0]:
            c = (self.lo[i] + self.hi[i]) / 2
            out.append((float(c[0]), float(c[1]), float(np.prod(ext[i]))))
        return out

    def rects(self, x, y, radius=30.0, max_extent=40.0):
        if not len(self.lo):
            return []
        ext = self.hi - self.lo
        near = ((np.abs((self.lo[:, 0] + self.hi[:, 0]) / 2 - x) <= radius) & (np.abs((self.lo[:, 1] + self.hi[:, 1]) / 2 - y) <= radius)
                & (ext[:, :2].max(1) <= max_extent))
        return [(float((self.lo[i, 0] + self.hi[i, 0]) / 2), float((self.lo[i, 1] + self.hi[i, 1]) / 2)) for i in np.nonzero(near)[0]]

    def _placed(self, i):
        if i not in self._tris:
            pl = self.pl[i]
            if pl["gltf"] not in self._models:
                self._models[pl["gltf"]] = self._load_model(os.path.join(self.out_dir, pl["gltf"]), self._tex)
            r = np.array(pl["game"]["rotation_rows"], dtype=float)
            s = np.array(pl["game"]["scale"], dtype=float)
            t = np.array(pl["game"]["position"], dtype=float)
            parts = []
            for p in self._models[pl["gltf"]]:
                w = (r @ (p["pos"] * s).T).T + t
                parts.append(w[p["idx"]])
            self._tris[i] = np.concatenate(parts) if parts else np.zeros((0, 3, 3))
        return self._tris[i]

    def local(self, x, y, z, radius=14.0, zlo=-3.0, zhi=4.5):
        """Triangles within `radius` (XY) and the z band [z+zlo, z+zhi] of a point."""
        if not len(self.lo):
            return Local(np.zeros((0, 3, 3)), z)
        sel = ((self.lo[:, 0] <= x + radius) & (self.hi[:, 0] >= x - radius) & (self.lo[:, 1] <= y + radius)
               & (self.hi[:, 1] >= y - radius) & (self.lo[:, 2] <= z + zhi) & (self.hi[:, 2] >= z + zlo))
        parts = []
        for i in np.nonzero(sel)[0]:
            t = self._placed(int(i))
            if not len(t):
                continue
            mn, mx = t.min(1), t.max(1)
            keep = ((mx[:, 0] >= x - radius) & (mn[:, 0] <= x + radius) & (mx[:, 1] >= y - radius) & (mn[:, 1] <= y + radius)
                    & (mx[:, 2] >= z + zlo) & (mn[:, 2] <= z + zhi))
            if keep.any():
                parts.append(t[keep])
        return Local(np.concatenate(parts) if parts else np.zeros((0, 3, 3)), z)


class Local:
    """Walkability queries on a bag of world triangles."""

    def __init__(self, tris, ref_z):
        self.ref_z = ref_z
        self.n = len(tris)
        if self.n:
            a, b, c = tris[:, 0], tris[:, 1], tris[:, 2]
            nrm = np.cross(b - a, c - a)
            ln = np.linalg.norm(nrm, axis=1)
            ok = ln > 1e-9
            tris, nrm, ln = tris[ok], nrm[ok], ln[ok]
            nz = np.abs(nrm[:, 2]) / ln
            self.floor = tris[nz > 0.5]
            walls = tris[nz <= 0.5]
            self.wall = walls
            self.wall_n = nrm[nz <= 0.5]
            self.wall_zlo = walls[:, :, 2].min(1) if len(walls) else np.zeros(0)
            self.wall_zhi = walls[:, :, 2].max(1) if len(walls) else np.zeros(0)
        else:
            self.floor = np.zeros((0, 3, 3))
            self.wall = np.zeros((0, 3, 3))
            self.wall_n = np.zeros((0, 3))
            self.wall_zlo = self.wall_zhi = np.zeros(0)

    def _hits(self, x, y):
        """Heights of every up/down-facing triangle that contains (x, y)."""
        f = self.floor
        if not len(f):
            return np.zeros(0)
        a, b, c = f[:, 0], f[:, 1], f[:, 2]
        v0x, v0y = c[:, 0] - a[:, 0], c[:, 1] - a[:, 1]
        v1x, v1y = b[:, 0] - a[:, 0], b[:, 1] - a[:, 1]
        v2x, v2y = x - a[:, 0], y - a[:, 1]
        d00 = v0x * v0x + v0y * v0y
        d01 = v0x * v1x + v0y * v1y
        d11 = v1x * v1x + v1y * v1y
        d02 = v0x * v2x + v0y * v2y
        d12 = v1x * v2x + v1y * v2y
        den = d00 * d11 - d01 * d01
        good = np.abs(den) > 1e-12
        den = np.where(good, den, 1.0)
        u = (d11 * d02 - d01 * d12) / den
        v = (d00 * d12 - d01 * d02) / den
        eps = 1e-6
        inside = good & (u >= -eps) & (v >= -eps) & (u + v <= 1 + eps)
        if not inside.any():
            return np.zeros(0)
        return (a[:, 2] + u * (c[:, 2] - a[:, 2]) + v * (b[:, 2] - a[:, 2]))[inside]

    def floor_z(self, x, y, ref, lo=-1.0, hi=0.6):
        """Height of the floor at (x, y) within [ref+lo, ref+hi] (closest to ref), or None."""
        z = self._hits(x, y)
        z = z[(z >= ref + lo) & (z <= ref + hi)]
        if not len(z):
            return None
        return float(z[np.argmin(np.abs(z - ref))])

    def _ray_hits(self, p0, p1, zz):
        m = (self.wall_zlo <= zz) & (self.wall_zhi >= zz)
        if not m.any():
            return False
        w = self.wall[m]
        a, b, c = w[:, 0], w[:, 1], w[:, 2]
        o = np.array([p0[0], p0[1], zz])
        d = np.array([p1[0] - p0[0], p1[1] - p0[1], 0.0])
        e1, e2 = b - a, c - a
        pv = np.cross(d, e2)
        det = np.einsum("ij,ij->i", e1, pv)
        ok = np.abs(det) > 1e-9
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = o - a
        u = np.einsum("ij,ij->i", tv, pv) * inv
        qv = np.cross(tv, e1)
        v = (qv @ d) * inv
        t = np.einsum("ij,ij->i", e2, qv) * inv
        return bool((ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t >= 0) & (t <= 1)).any())

    def wall_between(self, p0, p1, z, width=0.35):
        """True if the way from p0 to p1 is walled off at body height above z. Three parallel horizontal rays
        (centre and +-`width` m); blocked when two of them hit at the same height, so a lamp post does not count."""
        if not len(self.wall):
            return False
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        ln = (dx * dx + dy * dy) ** 0.5 or 1.0
        nx, ny = -dy / ln * width, dx / ln * width
        for h in BODY_HEIGHTS:
            n = 0
            for k in (-1, 0, 1):
                if self._ray_hits((p0[0] + nx * k, p0[1] + ny * k), (p1[0] + nx * k, p1[1] + ny * k), z + h):
                    n += 1
            if n >= 2:
                return True
        return False

    def enclosure(self, p, floor, d, samples=(1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0), reach=1.8):
        """0..1: how walled-in the way along d is (a wall within `reach` m on each side, averaged over samples).
        A roofed entry passage scores high, an open forecourt 0."""
        px, py = -d[1], d[0]
        tot = 0.0
        for s in samples:
            q = (p[0] + d[0] * s, p[1] + d[1] * s)
            for sign in (-1.0, 1.0):
                if self._side_wall(q, (q[0] + px * reach * sign, q[1] + py * reach * sign), floor):
                    tot += 0.5
        return tot / len(samples)

    def _side_wall(self, q, r, floor):
        for h in BODY_HEIGHTS:
            if self._ray_hits(q, r, floor + h):
                return True
        return False

    def run(self, p, z, d, max_run=6.0, step=0.5, skip=0.0):
        """Walkable run (m) from p along unit d: floor continues under every `step` and no wall in the body band.
        Returns (run, floor_z_at_end)."""
        px, py = p
        zprev = z
        prev = (px, py)
        good = 0.0
        s = step
        while s <= max_run + 1e-9:
            q = (px + d[0] * s, py + d[1] * s)
            fz = self.floor_z(q[0], q[1], zprev)
            if fz is None:
                break
            if s > skip and self.wall_between(prev, q, zprev):
                break
            prev, zprev, good = q, fz, s
            s += step
        return good, zprev

    def choose(self, p, z, prior, want_open=True, max_run=6.0, min_arrive=1.4, skip=0.3):
        """Pick the arrival direction among the four world axes.
        Preference order: (1) a walkable run of at least `min_arrive`; (2) outdoor only: the side behind is a walled-in
        entry passage and the side ahead is not (`enclosure`); (3) a run of 3 m or more; (3b) wall/void directly behind; (4) longer clear run
        (capped at max_run); (5) the prior.
        -> dict(direction, name, runs, enclosure, floor_z, ok, back_blocked)"""
        fz0 = self.floor_z(p[0], p[1], z, lo=-1.6, hi=1.0)
        zz = fz0 if fz0 is not None else z
        runs = {n: self.run(p, zz, d, max_run=max_run, skip=skip)[0] for n, d in CARDINALS.items()}
        encl = {n: self.enclosure(p, zz, d) for n, d in CARDINALS.items()} if want_open else {n: 0.0 for n in CARDINALS}
        opposite = {"E": "W", "W": "E", "N": "S", "S": "N"}
        best = None
        for name, d in CARDINALS.items():
            back = opposite[name]
            pri = (d[0] * prior[0] + d[1] * prior[1]) if prior else 0.0
            diff = round(encl[back] - encl[name], 1)
            sc = (1 if runs[name] >= min_arrive else 0, diff, 1 if runs[name] >= 3.0 else 0,
                  1 if runs[back] < 1.0 else 0, round(min(runs[name], max_run) * 2) / 2, pri)
            if best is None or sc > best[0]:
                best = (sc, name)
        name = best[1]
        back_blocked = runs[opposite[name]] < 1.0
        diff = encl[opposite[name]] - encl[name]
        # outdoor: the facing is only trusted when the building side is clearly walled-in or walled-off
        confident = (diff >= 0.5 or back_blocked) if want_open else True
        return {"direction": CARDINALS[name], "name": name, "runs": runs, "enclosure": encl, "floor_z": fz0,
                "ok": runs[name] >= min_arrive, "back_blocked": back_blocked, "confident": confident}
