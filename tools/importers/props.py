# OWNER: builder-A (importers)
"""Rides and hand props as standalone glTF models plus a rig sidecar (vehicles).

`vehicle <MODEL>...` converts a ride the way the game assembles it:

- the body: every intact part, damage-state subtrees (`*_dam`) pruned, untextured helper shapes (rider IK
  targets, the ARROW marker, collision proxies) dropped;
- its wheel model, if the `cars` row of Objects/default.ide names one (wheelmodelId, WheelScale = tyre
  diameter in metres), recentred on its own hub so it can spin about its axle;
- `vehicles/<MODEL>.json` (format `bully-remake.vehicle-rig/1`): the wheel hubs (`wheel_??_dummy` nodes),
  seat and door points, the ground offset and the body bounds, all in the model's UE space (cm, before the
  actor yaw; the model faces UE -Y because game +y is forward).

Bikes and the skateboard carry their wheels inside the body (one skinned mesh in the bind pose), so their
rig has no wheel entries. `prop <MODEL>...` is the same filtered conversion for hand props / projectiles.
"""

from __future__ import annotations

import json
import os

from . import coords
from .convert import TextureStore, convert_model
from .nif import parse_nif

RIG_FORMAT = "bully-remake.vehicle-rig/1"
POINT_NODES = ("ped_frontseat", "ped_backseat", "enter_exit", "exhaust", "headlights", "taillights")


def skip_damaged(name: str) -> bool:
    n = name.lower()
    return n.endswith("_dam") or "_dam_" in n


def car_rows(install: str) -> dict:
    """lower-case model -> (wheel model id, wheel scale) from the `cars` section of Objects/default.ide."""
    rows = {}
    path = os.path.join(install, "Objects", "default.ide")
    section = None
    with open(path, "r", encoding="latin-1") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            low = line.lower()
            if section is None:
                section = low
                continue
            if low == "end":
                section = None
                continue
            if section != "cars":
                continue
            cols = line.split()
            try:
                rows[cols[1].lower()] = (int(cols[-2]), float(cols[-1]))
            except (ValueError, IndexError):
                continue
    return rows


def node_points(data) -> dict:
    """Named NiNode origins in entity space (root and model node treated as identity, as convert_model does)."""
    nif = parse_nif(data)
    out = {}

    def visit(ref, xf, depth):
        b = nif.block(ref)
        if b is None or b.data is None or b.type_name != "NiNode" or depth > 64:
            return
        d = b.data
        here = xf if depth <= 1 else xf.compose(coords.Xform.from_av(d))
        name = nif.string(d["name"]) or ""
        if name and name not in out:
            out[name] = list(here.t)
        for c in d["children"]:
            visit(c, here, depth + 1)

    for r in nif.roots or [0]:
        visit(r, coords.Xform(), 0)
    return out


def ue_cm(v) -> list:
    return [round(c, 2) for c in coords.game_to_ue_position(v)]


def ue_bounds(lo, hi) -> list:
    a, b = ue_cm(lo), ue_cm(hi)
    return [[min(x, y) for x, y in zip(a, b)], [max(x, y) for x, y in zip(a, b)]]


def export_vehicle(img, defs, install: str, name: str, out_dir: str, log=print) -> dict:
    store = TextureStore(img, out_dir)
    e = img.find(name + ".nif")
    if e is None:
        raise KeyError(f"{name}: not in World.img")
    ide = defs.get(e.stem)
    data = img.read_entry(e)
    body = convert_model(e.stem, data, ide.txd if ide else None, store, out_dir,
                         skip_node=skip_damaged, drop_untextured=True)
    if not body.gltf:
        raise ValueError(f"{name}: no visible geometry after filtering")
    pts = node_points(data)
    (bmin, bmax) = body.bounds_game
    ground_z = bmin[2]
    rig = {"format": RIG_FORMAT, "model": e.stem, "mesh": f"/Game/Imported/Meshes/{e.stem}",
           "source": f"World.img/{e.stem}.nif", "wheels": [], "points": {},
           "body_bounds_cm": ue_bounds(bmin, bmax)}
    for p in POINT_NODES:
        if p in pts:
            rig["points"][p] = ue_cm(pts[p])

    row = car_rows(install).get(e.stem.lower())
    wheel_ide = defs.by_id.get(row[0]) if row else None
    hubs = sorted(n for n in pts if n.lower().startswith("wheel_") and n.lower().endswith("_dummy"))
    if wheel_ide and hubs:
        we = img.find(wheel_ide.model + ".nif")
        wdata = img.read_entry(we)
        probe = convert_model(we.stem, wdata, wheel_ide.txd, store, out_dir, drop_untextured=True)
        lo, hi = probe.bounds_game
        centre = [(a + b) / 2 for a, b in zip(lo, hi)]
        wheel = convert_model(we.stem, wdata, wheel_ide.txd, store, out_dir, drop_untextured=True, origin=centre)
        size = [b - a for a, b in zip(lo, hi)]
        axle = min(range(3), key=lambda k: size[k])           # thinnest extent = the axle
        diameter = max(size[k] for k in range(3) if k != axle)
        scale = row[1] / diameter if diameter > 1e-6 else 1.0
        radius = row[1] / 2
        rig["wheel_mesh"] = f"/Game/Imported/Meshes/{we.stem}"
        rig["wheel_model"] = we.stem
        rig["wheel_scale"] = round(scale, 4)
        rig["wheel_radius_cm"] = round(radius * 100, 2)
        rig["wheel_axle_game"] = "xyz"[axle]
        for h in hubs:
            tag = h.lower()[len("wheel_"):-len("_dummy")]       # lf, rf, lb, rb
            rig["wheels"].append({"name": tag, "hub_cm": ue_cm(pts[h]),
                                  "front": tag.endswith("f"), "left": tag.startswith("l")})
            ground_z = min(ground_z, pts[h][2] - radius)
        log(f"{e.stem}: wheel {we.stem} x{len(hubs)} diameter {row[1]} m (model {diameter:.3f} m, axle {'xyz'[axle]})")
        rig["wheel_gltf"] = wheel.gltf
    rig["ground_z_cm"] = round(ground_z * 100, 2)
    rig["gltf"] = body.gltf
    folder = os.path.join(out_dir, "vehicles")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, e.stem + ".json"), "w") as f:
        json.dump(rig, f, indent=1)
    log(json.dumps({"model": e.stem, "gltf": body.gltf, "shapes": body.shapes,
                    "dropped_untextured": body.skipped_untextured, "wheels": len(rig["wheels"]),
                    "ground_z_cm": rig["ground_z_cm"], "bounds_cm": rig["body_bounds_cm"]}))
    return rig


def export_prop(img, defs, name: str, out_dir: str, log=print) -> dict:
    store = TextureStore(img, out_dir)
    e = img.find(name + ".nif")
    if e is None:
        raise KeyError(f"{name}: not in World.img")
    ide = defs.get(e.stem)
    r = convert_model(e.stem, img.read_entry(e), ide.txd if ide else None, store, out_dir,
                      skip_node=skip_damaged, drop_untextured=True)
    info = {"model": e.stem, "gltf": r.gltf, "shapes": r.shapes, "dropped_untextured": r.skipped_untextured,
            "bounds_cm": ue_bounds(*r.bounds_game) if r.bounds_game else None}
    log(json.dumps(info))
    return info
