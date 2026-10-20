#!/usr/bin/env python3
"""Generate Config/InteriorSpots.json teleport anchors from the import manifest.

One spot per named area: median X/Y of the scene's visible placements (UE cm) and the lowest Z + 120 cm. Outdoor areas
(scenes t*) are then pulled onto the game's own sidewalk network (Config/GRIDS.DAT ped nodes, `--install` / BULLY_INSTALL):
`ground_cm` = the nearest usable sidewalk node, so a teleport lands on a street, never a roof or the stacked terrain
under it. The engine still finds open ground around that point at runtime (ABullyCharacter::BRGo, FBullyLanding). Covers the academy exterior + interiors and the town
districts. Re-run after any `tools.importers world` + `run_import.sh` so the teleport menu always matches what is imported.

  python3 tools/ue/gen_teleport_spots.py [--imported imported] [--out unreal/BullyRemake/Config/InteriorSpots.json]
"""
import argparse
import json
import math
import sys
import os
import statistics

# scene name -> friendly teleport key (what the player picks). Scenes not listed are skipped (terrain, street dressing).
FRIENDLY = {
    "tschool": "academy",      # academy exterior / front gates
    "ischool": "ischool",      # main school interior (lobby)
    "isc_art": "art", "isc_audt": "audt", "isc_auto": "auto", "isc_bio": "bio", "isc_boil": "boil",
    "isc_chem": "chem", "isc_clas": "clas", "isc_dorm": "dorm", "isc_Gdrm": "gdrm", "isc_lib": "lib",
    "isc_offi": "offi", "isc_pool": "pool", "isc_prep": "prep", "isc_staf": "staf",
    "tbusines": "town",        # main town / business strip
    "trich": "vale",           # rich district
    "tindust": "coventry",     # industrial district
    "tcarni": "carnival",
    "tjyard": "junkyard",
    "tBMX": "bmx",             # BMX park (bikes)
    "tgokart": "gokart",       # go-kart track
    # standalone interiors (present only after `world --preset all`)
    "igrocery": "grocery", "iboxing": "boxing", "ibkshop": "bookshop", "icomshp": "comics",
    "icloth_p": "clothes_p", "icloth_r": "clothes_r", "itenn": "tennis", "ifunhous": "funhouse",
    "iasylum": "asylum", "ibarber": "barber", "ichem": "chemist", "iobserv": "observatory",
    "iballtos": "balltoss", "isouvnir": "souvenir", "ishoot": "shooting", "itrailer": "trailer",
    "isalon": "salon", "iMGRaceA": "race_a", "iMGRaceB": "race_b", "iMGRaceC": "race_c",
    "iware": "warehouse", "ipbarb": "pirate_barber", "ifreaks": "freakshow",
    "iDropS": "store_dropout", "iPrepS": "store_prep", "iJockS": "store_jock", "iGrsrS": "store_greaser",
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--imported", default="imported")
    ap.add_argument("--out", default="unreal/BullyRemake/Config/InteriorSpots.json")
    ap.add_argument("--install", default=os.environ.get("BULLY_INSTALL"), help="game install (for Config/GRIDS.DAT)")
    a = ap.parse_args()
    ped = []
    if a.install and os.path.isfile(os.path.join(a.install, "Config", "GRIDS.DAT")):
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
        from tools.importers.townlife import parse_grids
        with open(os.path.join(a.install, "Config", "GRIDS.DAT"), encoding="latin-1") as f:
            grids = parse_grids(f.read())
        bad = ("WATERNODE", "UNDERBRIDGE", "BETWEENLEVELS", "SWITCHEDOFF")
        for n in grids.ped.nodes.values():
            if any(n["flags"].get(k) for k in bad):
                continue
            x, y, z = n["pos"]
            ped.append((100.0 * x, -100.0 * y, 100.0 * z))   # game metres -> UE cm
    else:
        print("  (no GRIDS.DAT: outdoor spots keep the manifest point; pass --install or set BULLY_INSTALL)")

    with open(os.path.join(a.imported, "scenes", "academy.json")) as f:
        manifest = json.load(f)

    by_scene = {}
    for p in manifest["placements"]:
        if not p.get("visible", True):
            continue
        loc = p.get("ue", {}).get("location_cm")
        if loc:
            by_scene.setdefault(p["scene"], []).append(loc)

    spots = {}
    skipped = []
    for scene, key in FRIENDLY.items():
        locs = by_scene.get(scene)
        if not locs:
            skipped.append(scene)
            continue
        xs = [l[0] for l in locs]
        ys = [l[1] for l in locs]
        zs = [l[2] for l in locs]
        spots[key] = {
            "location_cm": [round(statistics.median(xs), 1), round(statistics.median(ys), 1), round(min(zs) + 120.0, 1)],
            "scene": scene,
        }
        if ped and scene.startswith("t"):
            mx, my = spots[key]["location_cm"][:2]
            best = min(ped, key=lambda q: (q[0] - mx) ** 2 + (q[1] - my) ** 2)
            if math.hypot(best[0] - mx, best[1] - my) < 15000.0:
                spots[key]["ground_cm"] = [round(best[0], 1), round(best[1], 1), round(best[2], 1)]

    # stable order: academy exterior, town districts, then the rest alphabetically
    order = ["academy", "town", "vale", "coventry", "carnival", "junkyard", "bmx", "gokart", "ischool"]
    ordered = {k: spots[k] for k in order if k in spots}
    for k in sorted(spots):
        if k not in ordered:
            ordered[k] = spots[k]

    out = {
        "_note": "BRGo / teleport-menu spots, generated by tools/ue/gen_teleport_spots.py from the import manifest "
                 "(median XY of a scene's placements, min z + 120; outdoor spots carry ground_cm = nearest game sidewalk node "
                 "from GRIDS.DAT; the runtime finds open ground there). Re-run after a world re-import.",
        "spots": ordered,
    }
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(out, f, indent=1)
    print("wrote %d spots -> %s" % (len(ordered), a.out))
    print("  " + ", ".join(ordered))
    if skipped:
        print("  (not imported yet, skipped: %s)" % ", ".join(skipped))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
