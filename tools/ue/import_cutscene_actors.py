# OWNER: sm-cutscenes
"""Headless UE import of every baked-cutscene actor listed in <BR_IMPORTED>/cutscenes/actors.json
(written by `python3 -m tools.importers cutscene actors`). Each actor is imported by tools/ue/import_character.py
(run once per actor in this one editor process) into /Game/Imported/Characters/<ACTOR>/SK_<ACTOR> + Anims/CUT_<id>,
which is where cutscene.json's ue_mesh / ue_anim point and what UBullyCutsceneSubsystem::Probe checks for the Full tier.

Env: BR_IMPORTED (repo imported/), BR_CS_ONLY (optional comma list of actor names), BR_CS_REPORT_DIR.
Log lines: BR_CS_ACTORS <actor> OK|FAILED ..., then BR_CS_ACTORS_DONE ok=N failed=M.
"""
import json
import os
import runpy
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))


def log(msg):
    unreal.log("BR_CS_ACTORS " + msg)


def main():
    imported = os.environ.get("BR_IMPORTED", "")
    listing = os.path.join(imported, "cutscenes", "actors.json")
    if not os.path.isfile(listing):
        log("FAILED no %s (run python3 -m tools.importers cutscene actors)" % listing)
        return 0, 1
    with open(listing) as f:
        actors = json.load(f)["actors"]
    only = {x.strip().upper() for x in os.environ.get("BR_CS_ONLY", "").split(",") if x.strip()}
    if only:
        actors = [a for a in actors if a["name"].upper() in only]
    rep_dir = os.environ.get("BR_CS_REPORT_DIR") or os.path.join(imported, "cutscenes", "ue_import")
    os.makedirs(rep_dir, exist_ok=True)
    ok = bad = 0
    for a in actors:
        rep = os.path.join(rep_dir, "character_%s.json" % a["name"])
        os.environ.update({"BR_IMPORTED": imported, "BR_CHAR_MODEL": a["model"], "BR_CHAR_NAME": a["name"],
                           "BR_CHAR_ANIMS": "all", "BR_REPORT": rep})
        os.environ.pop("BR_CHAR_ANIMS_ONLY", None)
        try:
            runpy.run_path(os.path.join(HERE, "import_character.py"), run_name="__main__")
            with open(rep) as f:
                r = json.load(f)
            errs = r.get("errors", [])
            anims = r.get("animations", {})
            if errs:
                bad += 1
                log("%s FAILED errors=%s" % (a["name"], errs[:3]))
            else:
                ok += 1
                log("%s OK mesh=%s anims=%s" % (a["name"], (r.get("mesh") or {}).get("imported") or "unchanged", anims))
        except Exception as e:  # noqa: BLE001
            bad += 1
            log("%s FAILED %s\n%s" % (a["name"], e, traceback.format_exc()))
    return ok, bad


try:
    n_ok, n_bad = main()
except Exception as e:  # noqa: BLE001
    unreal.log_error("BR_CS_ACTORS fatal %s\n%s" % (e, traceback.format_exc()))
    n_ok, n_bad = 0, 1
unreal.log("BR_CS_ACTORS_DONE ok=%d failed=%d" % (n_ok, n_bad))
