#!/usr/bin/env python3
# OWNER: sm-visual
"""Score rendered mission stills (tools/ue/run_mission_shots.sh output).

  python3 tools/ue/shot_metrics.py DIR [DIR ...]            one table per shots dir
  python3 tools/ue/shot_metrics.py --root ROOT [ids ...]     ROOT/shots_<id> for each id (all shots_* when no ids)
  --json OUT                                                 also write the rows as JSON

Per still: mean luma (Rec.709 on a 96x54 bilinear downsample of the PNG; DARK when < 20 -- the metric the story-mode
brief uses), plus, from the sidecar shots_meta.jsonl when present, why it fired (settled / flush / forced), whether a
cutscene camera was live, the fade amount, and which mission characters had their head on screen and unoccluded.
Needs Pillow + numpy.
"""
import argparse
import glob
import json
import os
import sys

import numpy as np
from PIL import Image

DARK = 20.0


def luma(path):
    im = Image.open(path).convert("RGB").resize((96, 54), Image.BILINEAR)
    a = np.asarray(im).astype(np.float64)
    return float((0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]).mean())


def load_meta(d):
    meta = {}
    p = os.path.join(d, "shots_meta.jsonl")
    if os.path.exists(p):
        for line in open(p, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            try:
                m = json.loads(line)
            except ValueError:
                continue
            meta[m.get("file")] = m
    return meta


def score_dir(d):
    meta = load_meta(d)
    rows = []
    for f in sorted(glob.glob(os.path.join(d, "[0-9][0-9]_*.png"))):
        name = os.path.basename(f)
        m = meta.get(name, {})
        chars = m.get("chars", [])
        seen = [c["id"] for c in chars if c.get("head_on_screen") and not c.get("occluded")]
        npcs = [c["id"] for c in chars if c["id"] != "player"]
        rows.append({
            "dir": os.path.basename(d), "file": name, "luma": round(luma(f), 1),
            "dark": luma(f) < DARK, "reason": m.get("reason", "-"), "cutscene_cam": m.get("cutscene_cam"),
            "fade": m.get("fade"), "letterbox": m.get("letterbox"), "seen": seen, "npcs": npcs,
        })
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="*")
    ap.add_argument("--root")
    ap.add_argument("--json")
    a = ap.parse_args()
    dirs = []
    if a.root:
        ids = a.dirs
        dirs = [os.path.join(a.root, "shots_" + i) for i in ids] if ids else sorted(glob.glob(os.path.join(a.root, "shots_*")))
    else:
        dirs = a.dirs
    allrows = []
    tot = dark = 0
    for d in dirs:
        if not os.path.isdir(d):
            print("missing: %s" % d, file=sys.stderr)
            continue
        rows = score_dir(d)
        allrows += rows
        nd = sum(r["dark"] for r in rows)
        tot += len(rows)
        dark += nd
        print("%s: %d/%d dark" % (os.path.basename(d), nd, len(rows)))
        for r in rows:
            print("  %-28s luma %5.1f%s  %-8s cam=%s fade=%s  on-screen: %s%s" % (
                r["file"], r["luma"], " DARK" if r["dark"] else "     ", r["reason"],
                "cut" if r["cutscene_cam"] else "plr" if r["cutscene_cam"] is not None else "-",
                "-" if r["fade"] is None else "%.2f" % r["fade"], ",".join(r["seen"]) or "-",
                ("  (npcs: %s)" % ",".join(r["npcs"])) if r["npcs"] else ""))
    print("TOTAL %d/%d dark (mean luma < %.0f on 96x54)" % (dark, tot, DARK))
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"total": tot, "dark": dark, "rows": allrows}, f, indent=1)


if __name__ == "__main__":
    main()
