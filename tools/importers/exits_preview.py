# OWNER: builder (phase2-doors)
"""CPU top-down evidence for generated door links: textured pocket renders (preview.render with a crop window)
with the link triggers, exit points and arrival points drawn on top, plus a cheap plan view of the whole outdoor
map (placement footprints) with every exit. Needs numpy + Pillow. Evidence only."""

from __future__ import annotations

import os

import numpy as np

from .exits import slug
from .exits_geom import aabb_of_placement

RED, YELLOW, GREEN, CYAN, WHITE = (230, 40, 40), (255, 220, 0), (40, 230, 70), (0, 220, 255), (255, 255, 255)


def pixel_map(crop, size):
    """Game XY -> pixel for a top-down render of `crop` = (x0, x1, y0, y1) at `size` (matches preview.render)."""
    span = max(crop[1] - crop[0], crop[3] - crop[2])
    scale = (size - 20) / span
    return (lambda x, y: ((x - crop[0]) * scale + 10, (crop[3] - y) * scale + 10)), scale


def _link_game(l):
    c = l["trigger"]["center_cm"]
    h = l["trigger"]["half_extent_cm"]
    e = l["exit"]["location_cm"]
    return {"id": l["id"], "tc": (c[0] / 100, -c[1] / 100, c[2] / 100), "th": (h[0] / 100, h[1] / 100),
            "ex": (e[0] / 100, -e[1] / 100, e[2] / 100)}


def draw_links(img, links, pm, label=True, small=False):
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    r = 3 if small else 6
    for l in links:
        g = _link_game(l)
        (x0, y0), (x1, y1) = pm(g["tc"][0] - g["th"][0], g["tc"][1] + g["th"][1]), pm(g["tc"][0] + g["th"][0], g["tc"][1] - g["th"][1])
        d.rectangle([x0, y0, x1, y1], outline=RED, width=1 if small else 2)
        cx, cy = pm(g["tc"][0], g["tc"][1])
        ax, ay = pm(g["ex"][0], g["ex"][1])
        d.line([cx, cy, ax, ay], fill=YELLOW, width=1 if small else 2)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=YELLOW, outline=(0, 0, 0))
        d.ellipse([ax - r, ay - r, ax + r, ay + r], fill=GREEN, outline=(0, 0, 0))
        if label:
            d.text((cx + r + 2, cy - r - 2), l["id"].replace("g_", "")[:34], fill=WHITE)


def render_crop(out_dir, scenes, crop, path, links, size=1400, cut=None, log=print):
    from PIL import Image
    from .preview import render
    render(out_dir, scenes, view="top", size=size, cut=cut, out_path=path, log=log, crop=crop)
    img = Image.open(path).convert("RGB")
    pm, _ = pixel_map(crop, size)
    draw_links(img, links, pm)
    img.save(path)
    return path


def outdoor_overview(manifest, doc, handmade, path, size=2000, log=print):
    """Footprint plan of the exterior scenes + the interior pocket boxes, with all link triggers/arrivals."""
    from PIL import Image, ImageDraw
    bounds = {m["gltf"]: m["bounds_game"] for m in manifest["models"].values() if m.get("gltf") and m.get("bounds_game")}
    rects = []
    kinds = {s["name"]: s.get("kind") for s in manifest["scenes"]}
    for pl in manifest["placements"]:
        if not pl.get("gltf") or not pl.get("visible") or pl["gltf"] not in bounds or kinds.get(pl["scene"]) != "exterior":
            continue
        lo, hi = aabb_of_placement(pl, bounds[pl["gltf"]])
        if (hi - lo)[:2].max() > 60:
            continue
        rects.append((lo, hi))
    allr = np.array([[lo[0], lo[1], hi[0], hi[1]] for lo, hi in rects])
    crop = (float(np.percentile(allr[:, 0], 0.2)) - 10, float(np.percentile(allr[:, 2], 99.8)) + 10,
            float(np.percentile(allr[:, 1], 0.2)) - 10, float(np.percentile(allr[:, 3], 99.8)) + 10)
    pm, _ = pixel_map(crop, size)
    W = int((crop[1] - crop[0]) * (size - 20) / max(crop[1] - crop[0], crop[3] - crop[2])) + 20
    H = int((crop[3] - crop[2]) * (size - 20) / max(crop[1] - crop[0], crop[3] - crop[2])) + 20
    img = Image.new("RGB", (W, H), (28, 34, 40))
    d = ImageDraw.Draw(img)
    for lo, hi in rects:
        tall = hi[2] - lo[2] >= 2.5
        x0, y0 = pm(lo[0], hi[1])
        x1, y1 = pm(hi[0], lo[1])
        d.rectangle([x0, y0, max(x1, x0 + 1), max(y1, y0 + 1)], fill=(120, 124, 130) if tall else (60, 82, 60))
    inside = [l for l in doc["links"]
              if crop[0] <= l["trigger"]["center_cm"][0] / 100 <= crop[1] and crop[2] <= -l["trigger"]["center_cm"][1] / 100 <= crop[3]]
    draw_links(img, inside, pm, label=False, small=True)
    # hand-made links are ringed in cyan (only those inside this window show)
    for l in handmade:
        g = _link_game(l)
        cx, cy = pm(g["tc"][0], g["tc"][1])
        if 0 <= cx < W and 0 <= cy < H:
            d.ellipse([cx - 7, cy - 7, cx + 7, cy + 7], outline=CYAN, width=3)
    img.save(path)
    log(f"overview {path}: {W}x{H}, {len(rects)} footprints")
    return path


def make_previews(out_dir, manifest, doc, report, sides, dest, handmade=(), areas=None, outdoor_n=999, log=print):
    """Write the evidence set into `dest`. `sides` maps link id -> exits.Side. -> list of written paths."""
    os.makedirs(dest, exist_ok=True)
    written = [outdoor_overview(manifest, doc, list(handmade), os.path.join(dest, "outdoor_overview.png"), log=log)]
    links = {l["id"]: l for l in doc["links"]}
    pockets = {s["name"]: s for s in manifest["scenes"]}
    # interiors: one render per (area, floor)
    groups: dict[tuple, list] = {}
    for lid, side in sides.items():
        e = side.exit
        if e.akey == "mainmap":
            continue
        groups.setdefault((e.area, round(e.z / 4.0)), []).append(lid)
    for (area, fl), lids in sorted(groups.items()):
        if areas and area not in areas:
            continue
        chk = report["exit_checks"].get(f"{sides[lids[0]].exit.area}#{sides[lids[0]].exit.exit_id}@L{sides[lids[0]].exit.line}", {})
        scenes = chk.get("scenes") or []
        if not scenes:
            continue
        xs = [sides[l].exit.x for l in lids] + [pockets[s]["bounds_game_positions"][i][0] for s in scenes for i in (0, 1)]
        ys = [sides[l].exit.y for l in lids] + [pockets[s]["bounds_game_positions"][i][1] for s in scenes for i in (0, 1)]
        crop = (min(xs) - 5, max(xs) + 5, min(ys) - 5, max(ys) + 5)
        zmin = min(sides[l].exit.z for l in lids)
        path = os.path.join(dest, f"interior_{slug(area)}_f{fl}.png")
        try:
            render_crop(out_dir, scenes, crop, path, [links[l] for l in lids], cut=zmin + 2.8, log=log)
            written.append(path)
        except SystemExit as ex:  # nothing to render
            log(f"skip {area}: {ex}")
    # outdoor details: the first few exits
    n = 0
    for lid, side in sides.items():
        e = side.exit
        if e.akey != "mainmap" or n >= outdoor_n:
            continue
        crop = (e.x - 22, e.x + 22, e.y - 22, e.y + 22)
        ext = [s["name"] for s in manifest["scenes"] if s.get("kind") == "exterior" and
               s["bounds_game_positions"][0][0] - 5 <= e.x <= s["bounds_game_positions"][1][0] + 5 and
               s["bounds_game_positions"][0][1] - 5 <= e.y <= s["bounds_game_positions"][1][1] + 5]
        if not ext:
            continue
        path = os.path.join(dest, f"outdoor_{slug(lid)}.png")
        try:
            render_crop(out_dir, ext, crop, path, [links[lid]], size=700, cut=e.z + 6, log=log)
            written.append(path)
            n += 1
        except SystemExit as ex:
            log(f"skip {lid}: {ex}")
    written += contact_sheets(dest, [w for w in written if os.path.basename(w).startswith("outdoor_g_")], log=log)
    return written


def contact_sheets(dest, paths, per=6, cols=3, cell=700, log=print):
    """Tile the outdoor close-ups, `per` to a sheet (3 x 2), so they can be reviewed at a glance."""
    from PIL import Image, ImageDraw
    out = []
    for k in range(0, len(paths), per):
        chunk = paths[k:k + per]
        rows = (len(chunk) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * cell, rows * cell), (20, 20, 20))
        for i, pth in enumerate(chunk):
            im = Image.open(pth).convert("RGB").resize((cell, cell))
            ImageDraw.Draw(im).text((6, cell - 14), os.path.basename(pth)[10:-4], fill=WHITE)
            sheet.paste(im, ((i % cols) * cell, (i // cols) * cell))
        sp = os.path.join(dest, f"outdoor_sheet_{k // per + 1}.png")
        sheet.save(sp)
        out.append(sp)
    return out
