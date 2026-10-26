# OWNER: builder-A (importers)
"""Command line: `python3 -m tools.importers <command> ...`

The game install path comes from, in order: --install, the BULLY_INSTALL
environment variable, or `tools/importers/local_config.json` (git-ignored,
{"install": "..."}). It is never hard-coded. The install is opened
read-only; output goes under --out (default `imported/`).
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time

from .binreader import FormatError
from .gltf import validate_gltf

HERE = os.path.dirname(os.path.abspath(__file__))
LOCAL_CONFIG = os.path.join(HERE, "local_config.json")


def resolve_install(arg: str | None) -> str:
    path = arg or os.environ.get("BULLY_INSTALL")
    if not path and os.path.exists(LOCAL_CONFIG):
        with open(LOCAL_CONFIG) as f:
            path = json.load(f).get("install")
    if not path:
        sys.exit("game install not given: pass --install, set BULLY_INSTALL, or create "
                 f"{os.path.relpath(LOCAL_CONFIG)} with {{\"install\": \"...\"}}")
    if not os.path.isdir(os.path.join(path, "Stream")):
        sys.exit(f"{path}: not a Bully: Scholarship Edition install (no Stream/ folder)")
    return path


def _archive(install: str, rel: str):
    from .img import ImgArchive
    p = rel if os.path.isabs(rel) else os.path.join(install, rel)
    return ImgArchive(p)


def cmd_img(a) -> int:
    install = resolve_install(a.install)
    with _archive(install, a.archive) as img:
        if a.action == "list":
            names = img.names(a.ext)
            for e in img.entries:
                if e.name in names or a.ext is None:
                    if a.ext is None or e.ext == a.ext.lower().lstrip("."):
                        print(f"{e.index:6d} {e.offset_sectors:9d} {e.size_sectors:6d} {e.name}")
            print(f"# {len(img)} entries; census {img.census()}; overlaps {len(img.check_overlaps())}; "
                  f"duplicate names {len(img.duplicates)}", file=sys.stderr)
        else:
            dest = os.path.join(a.out, "raw", os.path.splitext(os.path.basename(a.archive))[0])
            os.makedirs(dest, exist_ok=True)
            names = a.names or img.names(a.ext)
            for n in names:
                e = img.find(n)
                if e is None:
                    print(f"missing: {n}", file=sys.stderr)
                    continue
                with open(os.path.join(dest, e.name), "wb") as f:
                    f.write(img.read_entry(e))
            print(f"extracted {len(names)} entries to {dest}")
    return 0


def cmd_nif(a) -> int:
    from .convert import TextureStore, convert_model
    from .ide import ModelDefs
    install = resolve_install(a.install)
    with _archive(install, "Stream/World.img") as img:
        defs = ModelDefs.load_install(install)
        store = TextureStore(img, a.out)
        rc = 0
        for name in a.names:
            stem = os.path.splitext(name)[0]
            e = img.find(stem + ".nif")
            if e is None:
                print(f"{name}: not in World.img", file=sys.stderr)
                rc = 1
                continue
            ide = defs.get(e.stem)
            txd = a.txd or (ide.txd if ide else None)
            try:
                r = convert_model(e.stem, img.read_entry(e), txd, store, a.out, entity_space=not a.file_space)
            except FormatError as ex:
                print(f"{name}: FAILED {ex}", file=sys.stderr)
                rc = 1
                continue
            print(json.dumps({"model": e.stem, "gltf": r.gltf, "txd": txd, "shapes": r.shapes,
                              "vertices": r.vertices, "triangles": r.triangles,
                              "missing_textures": r.missing_textures, "warnings": r.warnings[:10]}))
    return rc


def cmd_vehicle(a) -> int:
    from .ide import ModelDefs
    from .props import export_prop, export_vehicle
    install = resolve_install(a.install)
    rc = 0
    with _archive(install, "Stream/World.img") as img:
        defs = ModelDefs.load_install(install)
        for name in a.names:
            try:
                if a.cmd == "vehicle":
                    export_vehicle(img, defs, install, name, a.out)
                else:
                    export_prop(img, defs, name, a.out)
            except (KeyError, ValueError, FormatError) as ex:
                print(f"{name}: FAILED {ex}", file=sys.stderr)
                rc = 1
    return rc


def cmd_ped(a) -> int:
    from .convert import TextureStore
    from .ide import ModelDefs
    from .ped import PLAYER_LOCOMOTION, clip_requests, export_ped
    install = resolve_install(a.install)
    with _archive(install, "Stream/World.img") as img:
        defs = ModelDefs.load_install(install)
        e = img.find(a.name + ".nif")
        if e is None:
            print(f"{a.name}: not in World.img", file=sys.stderr)
            return 1
        ide = defs.get(e.stem)
        txd = a.txd or (ide.txd if ide else None)
        store = None if a.no_textures else TextureStore(img, a.out)
        names = a.anims if a.anims else (PLAYER_LOCOMOTION if a.group.lower() == "c_player" else [])
        if a.no_anims:
            names = []
        try:
            reqs = []
            groups = [g for g in a.group.split(",") if g]
            if a.cutscene:   # sm-cutscenes: the actor's baked cutscene clips (Cuts.img), face merged, raw root motion
                from .cutscene import actor_clip_requests
                actor = a.cutscene_actor or (e.stem[:-5] if e.stem.upper().endswith("_BODY") else e.stem)
                with _archive(install, "Cuts/Cuts.img") as cuts:
                    reqs = actor_clip_requests(cuts, [c for c in a.cutscene.split(",") if c], actor)
                if not reqs:
                    print(f"{a.name}: actor {actor} has no clip in {a.cutscene}", file=sys.stderr)
                    return 1
            elif names and len(groups) == 1:
                reqs = clip_requests(install, img, groups[0], names)
            elif names:
                # several groups (rides, weapons): "*" takes every clip of every group, otherwise each name
                # resolves in the first group that holds it
                from .ped import load_hxd
                hxd = load_hxd(install)
                left = list(names)
                for g in groups:
                    if names == ["*"]:
                        reqs += clip_requests(install, img, g, ["*"], hxd=hxd)
                        continue
                    have = {x for r in hxd.group_anims(g) for x in (r.name.lower(), r.short_name.lower())}
                    mine = [n for n in left if n.lower() in have]
                    left = [n for n in left if n.lower() not in have]
                    if mine:
                        reqs += clip_requests(install, img, g, mine, hxd=hxd)
                if left and names != ["*"]:
                    raise FormatError(f"clips not in {groups}: {left}")
                seen = set()
                for r in reqs:   # a later group must not overwrite an earlier clip of the same name
                    if r.label in seen:
                        raise FormatError(f"clip name {r.label} appears in two of {groups}; ask for fewer clips")
                    seen.add(r.label)
            head = None
            if a.head:
                he = img.find(a.head + ".nif")
                if he is None:
                    print(f"{a.head}: not in World.img", file=sys.stderr)
                    return 1
                hide = defs.get(he.stem)
                head = (he.stem, img.read_entry(he), a.head_txd or (hide.txd if hide else he.stem))   # clothing rows: model == txd name
            r = export_ped(e.stem, img.read_entry(e), txd, store, a.out, reqs, root_motion=a.root_motion, head=head)
        except FormatError as ex:
            print(f"{a.name}: FAILED {ex}", file=sys.stderr)
            return 1
    problems = validate_gltf(os.path.join(a.out, r.gltf), check_images=store is not None)
    out = {"model": r.name, "gltf": r.gltf, "joints": r.joints, "skins": r.skins, "shapes": r.shapes,
           "vertices": r.vertices, "triangles": r.triangles, "max_influences": r.max_influences,
           "unweighted_vertices": r.unweighted_vertices,
           "weight_sum_max_error": round(r.weight_sum_max_error, 6),
           "bind_vs_stored_max_mm": round(r.bind_vs_stored_max_m * 1000, 3),
           "ibm_vs_file_pose_max": round(r.bind_vs_file_pose_max, 6),
           "skipped_shapes": r.skipped_shapes, "missing_textures": r.missing_textures,
           "animations": [{k: c[k] for k in ("name", "duration", "key_type", "tracks", "root_motion",
                                              "loops", "start_end_max_deg")} for c in r.clips],
           "warnings": r.warnings[:10], "validation_problems": problems}
    print(json.dumps(out, indent=1))
    if a.sheet:
        from .skinpreview import contact_sheet
        for view in a.sheet_views:
            stem, ext = os.path.splitext(a.sheet)
            contact_sheet(os.path.join(a.out, r.gltf), f"{stem}_{view}{ext or '.png'}", frames=a.sheet_frames,
                          view=view, log=lambda m: print(m, file=sys.stderr))
    return 1 if problems else 0


def cmd_audio(a) -> int:
    from . import audio
    install = resolve_install(a.install)
    if a.action == "survey":
        r = audio.survey(install)
        r["streams"] = audio.survey_streams(install)
        print(json.dumps(r, indent=1))
        return 0
    if a.action == "list":
        for p in audio.sea_files(install):
            if a.banks and p.stem.lower() not in {b.lower() for b in a.banks}:
                continue
            b = audio.parse_sea(p)
            print(f"# {p.stem}: bank '{b.bank_name}', {len(b.entries)} entries")
            for e in b.entries:
                print(f"{e.index:4d} {e.name:40s} {e.seconds:7.3f}s {e.sample_rate:6d} Hz {e.channels}ch "
                      f"{'adpcm' if e.tag == 2 else e.tag}")
        return 0
    if a.action == "cues":
        res = {}
        for p in audio.sea_files(install):
            if a.banks and p.stem.lower() not in {b.lower() for b in a.banks}:
                continue
            b = audio.parse_sea(p)
            res[p.stem] = {"wave_bank": b.bank_name, "cues": audio.cue_table(b)}
        dest = os.path.join(a.out, "audio", "cues")
        os.makedirs(dest, exist_ok=True)
        for stem, r in res.items():
            with open(os.path.join(dest, stem + ".json"), "w") as f:
                json.dump({"format": "bully-remake.sdbk-cues/1", "bank": stem, **r}, f, indent=1)
            print(f"# {stem}: {len(r['cues'])} cues -> {dest}/{stem}.json")
            if a.print_cues:
                for n, c in r["cues"].items():
                    print(f"{n:28s} -> {c['wave']:36s} pitch {c['pitch_cents']:+5d} [{c['pitch_min']:+d}..{c['pitch_max']:+d}] {c['volume_db']:+.1f} dB")
        return 0
    if a.action == "footsteps":
        cfg = json.load(open(a.config))
        res = audio.resolve_footsteps(install, cfg, log=lambda m: print(m, file=sys.stderr))
        dest = os.path.join(a.out, "audio", "footstep_cues.json")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w") as f:
            json.dump(res, f, indent=1)
        n = sum(len(g) for c in res["contexts"].values() for sf in c.values() for r in sf.values() for g in r.values())
        print(json.dumps({"out": dest, "cue_lists": n, "banks": {b: len(w) for b, w in res["waves"].items()},
                          "unresolved": len(res["unresolved"])}))
        return 0
    out = os.path.join(a.out, "audio")
    man = audio.extract(install, out, a.banks or None, a.names, log=lambda m: print(m, file=sys.stderr))
    print(json.dumps({"out": out, "wavs": man["count"]}))
    return 0


def cmd_speech(a) -> int:
    """Scripted mission speech (PLAYLIST/Speech.bin) -> <out>/speech/<line>/v<take>.wav + speech/manifest.json."""
    from . import speech
    install = resolve_install(a.install)
    missions = () if a.extra_only else tuple(a.missions or speech.CHAPTER1_OPENING)
    man = speech.build(install, a.out, missions=missions, language=a.language, transcribe=a.transcribe, model=a.model,
                       log=lambda m: print(m, file=sys.stderr), merge=a.merge or a.extra_only)
    lines = man["lines"].values()
    print(json.dumps({"out": os.path.join(a.out, "speech"), "lines": len(man["lines"]),
                      "takes": sum(len(e["takes"]) for e in lines),
                      "text": {s: sum(1 for e in lines if e["text_source"] == s) for s in ("table", "transcript", None)}}))
    return 0


def cmd_cutscene(a) -> int:
    """Baked cutscenes (Cuts.img + PLAYLIST/Cuts.bin + Config/Text): manifests + audio into <out>/cutscenes/<id>/."""
    from . import cutscene as cs
    install = resolve_install(a.install)
    if a.action == "list":
        for r in cs.list_cutscenes(install):
            print(f"{r['id']:12s} audio={'y' if r['audio'] else '-'} sub={'y' if r['sub'] else '-'} hxd={','.join(r['hxds'])}")
        return 0
    ids = list(a.ids or [])
    if a.chapter1 or not ids:
        ids = list(cs.CHAPTER1_OPENING) + [i for i in ids if i not in cs.CHAPTER1_OPENING]
    if a.action == "actors":
        return _cutscene_actors(a, install, ids)
    rc = 0
    with _archive(install, "Stream/World.img") as world:
        for cid in ids:
            try:
                m = cs.build(install, cid, a.out, language=a.language, with_audio=not a.no_audio, world_img=world)
            except (FormatError, OSError, ValueError) as ex:
                print(f"{cid}: FAILED {ex}", file=sys.stderr)
                rc = 1
                continue
            print(json.dumps({"id": cid, "duration_s": m["duration_s"],
                              "audio_s": m["audio"]["seconds"] if m["audio"] else None,
                              "subtitles": len(m["subtitles"]),
                              "subtitle_match": sorted({s["match"] or "missing" for s in m["subtitles"]}),
                              "camera_frames": m["camera"]["frames"], "camera_cuts": len(m["camera"]["cuts"]),
                              "actors": [x["name"] for x in m["actors"]], "warnings": m["warnings"],
                              "out": os.path.join(a.out, "cutscenes", cid)}))
    return rc


def _cutscene_actors(a, install: str, ids: list) -> int:
    """Every character actor of the given cutscenes -> <out>/models/<nif>.gltf (ped, raw root motion, one CUT_<id> clip
    per cutscene it is in, face merged) + <out>/cutscenes/actors.json, the list tools/ue/run_import_cutscene_actors.sh
    imports (UE name = the HXD actor name, which is what cutscene.json's ue_mesh / ue_anim point at)."""
    import argparse
    from . import cutscene as cs
    actors = {}
    with _archive(install, "Stream/World.img") as world, _archive(install, "Cuts/Cuts.img") as cuts:
        for cid in ids:
            for c in cs.actor_clips(cuts, cid):
                if c.actor.upper().startswith("F_") or not c.group.upper().endswith("_CHAR"):
                    continue   # facial clips merge into the body; props are optional and skipped
                e = actors.setdefault(c.actor, {"name": c.actor, "model": cs.actor_model(world, c.actor), "cutscenes": []})
                e["cutscenes"].append(cid)
    rc, done = 0, []
    for name, e in actors.items():
        nif, head = e["model"]["nif"], e["model"]["head"]
        if nif is None:
            print(f"{name}: no model in World.img", file=sys.stderr)
            rc = 1
            continue
        pa = argparse.Namespace(install=a.install, out=a.out, name=nif, txd=None, no_textures=False, group="C_Player",
                                anims=None, no_anims=False, root_motion="raw", head=head, head_txd=None,
                                cutscene=",".join(e["cutscenes"]), cutscene_actor=name, sheet=None,
                                sheet_views=["side"], sheet_frames=6)
        import contextlib
        import io
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            r = cmd_ped(pa)
        if r != 0:
            rc = 1
            continue
        info = json.loads(buf.getvalue())
        done.append({"name": name, "model": nif, "head": head, "cutscenes": e["cutscenes"], "gltf": info["gltf"],
                     "clips": [x["name"] for x in info["animations"]], "validation_problems": info["validation_problems"]})
        print(json.dumps({"actor": name, "gltf": info["gltf"], "joints": info["joints"], "clips": len(info["animations"]),
                          "missing_textures": info["missing_textures"], "problems": info["validation_problems"]}))
    os.makedirs(os.path.join(a.out, "cutscenes"), exist_ok=True)
    with open(os.path.join(a.out, "cutscenes", "actors.json"), "w") as f:
        json.dump({"cutscenes": ids, "actors": done}, f, indent=1)
    return rc


def cmd_anim(a) -> int:
    from .agr import arrow_displacement, parse_agr
    from .ped import load_hxd, read_group_agr
    install = resolve_install(a.install)
    hxd = load_hxd(install, a.hxd)
    if not a.group:
        for g in hxd.groups:
            print(f"{g.index:4d} {g.name:24s} {len(hxd.group_anims(g.index)):4d} clips")
        return 0
    recs = hxd.group_anims(a.group)
    with _archive(install, "Stream/World.img") as img:
        data, src = read_group_agr(install, img, a.group)
    clips = parse_agr(data, [r.agr_size - 4 for r in recs]) if recs else parse_agr(data)
    print(f"# {src}: {len(clips)} clips, HXD {a.hxd} lists {len(recs)}")
    for k, c in enumerate(clips):
        r = recs[k] if k < len(recs) else None
        d = r.duration if r else c.duration
        t = arrow_displacement(c)
        speed = (t[0] ** 2 + t[1] ** 2 + t[2] ** 2) ** 0.5 / d if d else 0
        print(f"{k:4d} {(r.name if r else '?'):40s} type {c.key_type} bones {c.bone_count:2d} "
              f"keys {c.key_count:5d} dur {d:6.3f}s travel ({t[0]:+.3f},{t[1]:+.3f},{t[2]:+.3f}) m "
              f"{speed:5.2f} m/s")
    return 0


def cmd_nft(a) -> int:
    from .nft import TextureDictionary, safe_name
    install = resolve_install(a.install)
    with _archive(install, "Stream/World.img") as img:
        rc = 0
        for name in a.names:
            stem = os.path.splitext(os.path.basename(name))[0]
            try:
                if os.path.exists(name):
                    with open(name, "rb") as f:
                        td = TextureDictionary(stem, f.read())
                else:
                    td = TextureDictionary(stem, img.read(stem + ".nft"))
            except (KeyError, FormatError) as ex:
                print(f"{name}: FAILED {ex}", file=sys.stderr)
                rc = 1
                continue
            folder = os.path.join(a.out, "textures", safe_name(td.name))
            os.makedirs(folder, exist_ok=True)
            for t in td.textures.values():
                try:
                    info = td.export(t, os.path.join(folder, safe_name(t.name) + ".png"))
                    print(f"{td.name}/{t.name}: {info['width']}x{info['height']} {info['format']}")
                except FormatError as ex:
                    print(f"{td.name}/{t.name}: FAILED {ex}", file=sys.stderr)
                    rc = 1
    return rc


def cmd_ipb(a) -> int:
    from .ipb import parse_ipb
    install = resolve_install(a.install)
    os.makedirs(os.path.join(a.out, "ipb"), exist_ok=True)
    with _archive(install, "Stream/World.img") as img:
        for name in a.names:
            stem = os.path.splitext(name)[0]
            ipb = parse_ipb(img.read(stem + ".ipb"))
            p = os.path.join(a.out, "ipb", stem + ".json")
            with open(p, "w") as f:
                json.dump({"source": f"World.img/{stem}.ipb", "header_unknown_hex": ipb.header_unknown.hex(),
                           "sections": ipb.section_counts,
                           "placements": [x.to_json() for x in ipb.placements]}, f, indent=1)
            print(f"{stem}: {len(ipb.placements)} placements {ipb.section_counts} -> {p}")
    return 0


def cmd_world(a) -> int:
    from .world import PRESETS, build_world
    install = resolve_install(a.install)
    stems = a.ipb or PRESETS[a.preset]
    report = build_world(install, a.out, stems, entity_space=not a.file_space,
                         convert_invisible=a.convert_invisible)
    print(json.dumps(report, indent=1))
    return 0 if report["models_converted"] else 1


def cmd_townlife(a) -> int:
    from .townlife import build_townlife
    install = resolve_install(a.install)
    res = build_townlife(install, a.out, only=a.districts, evidence_dir=a.evidence, raster_parking=a.raster_parking)
    for d in res["districts"]:
        print(f"{d['name']:20s} loops {len(d['loops'])}  parking {len(d['parking'])}  "
              f"(sidewalk nodes {d['stats']['sidewalk_nodes']}, road nodes {d['stats']['road_nodes']})"
              + ("  NOTE: " + "; ".join(d["notes"]) if d["notes"] else ""))
    return 0


def cmd_trigger(a) -> int:
    from . import trigger as T
    install = resolve_install(a.install)
    with _archive(install, T.ARCHIVE) as arch:
        files = T.read_archive(arch)
    if a.list:
        for stem, f in files.items():
            c = f["counts"]
            print(f"{stem:32s} path {c['path']:3d} point {c['point']:3d} trigger {c['trigger-volume']:3d} "
                  f"poi {c['poi']:3d} orphan-perimeters {len(f['orphan_perimeters']):3d}"
                  + (f"  !{len(f['warnings'])} warnings" if f["warnings"] else ""))
        print(json.dumps(T.summary(files)), file=sys.stderr)
        return 0
    folder = os.path.join(a.out, "triggers")
    T.write_json(os.path.join(folder, "day_one.json"), T.day_one(files))
    print(f"day_one -> {os.path.join(folder, 'day_one.json')}")
    for stem in a.files or []:
        key = next((k for k in files if k.lower() == stem.lower()), None)
        if key is None:
            print(f"{stem}: not in {T.ARCHIVE}", file=sys.stderr)
            return 1
        T.write_json(os.path.join(folder, key + ".json"), files[key])
        print(f"{key} -> {os.path.join(folder, key + '.json')}")
    if a.all:
        T.write_json(os.path.join(folder, "all.json"), {"source": T.ARCHIVE, "summary": T.summary(files), "files": files})
        print(f"all -> {os.path.join(folder, 'all.json')}")
    print(json.dumps(T.summary(files)))
    return 0


def cmd_exits(a) -> int:
    """Config/dat/ExitLocs.dat + areabbox.dat -> imported/door_links_game.json (+ exits_report.json, optional previews)."""
    from . import exits_build as E
    install = resolve_install(a.install)
    dat = os.path.join(install, "Config", "dat")

    def read(name):
        with open(os.path.join(dat, name), encoding="latin-1") as f:
            return f.read()

    manifest = None
    mpath = a.manifest or os.path.join(a.out, "scenes", "academy.json")
    if os.path.exists(mpath):
        with open(mpath) as f:
            manifest = json.load(f)
    else:
        print(f"note: no scene manifest at {mpath}: pocket checks and geometry are skipped", file=sys.stderr)
    hand_path = a.handmade or os.path.join(os.path.dirname(os.path.dirname(HERE)), "tools", "ue", "door_links.json")
    handmade = []
    if os.path.exists(hand_path):
        with open(hand_path, encoding="utf-8") as f:
            handmade = json.load(f)["links"]
    world = None
    if manifest and not a.no_geometry:
        from .exits_geom import World
        world = World(a.out, manifest)
    doc, report, sides = E.build(read("ExitLocs.dat"), read("areabbox.dat"), manifest, handmade, world, step=a.step)
    out = a.links_out or os.path.join(a.out, "door_links_game.json")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    rep = a.report or os.path.join(a.out, "exits_report.json")
    with open(rep, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    print(E.format_report(report))
    print(f"links -> {out}\nreport -> {rep}")
    if a.previews:
        from .exits_preview import make_previews
        if manifest is None:
            sys.exit("--previews needs the scene manifest")
        areas = set(a.preview_areas.split(",")) if a.preview_areas else None
        for p in make_previews(a.out, manifest, doc, report, sides, os.path.expanduser(a.previews), handmade,
                               areas=areas, outdoor_n=a.preview_outdoor):
            print("preview", p)
    return 0


def cmd_validate(a) -> int:
    t = time.time()
    folder = os.path.join(a.out, "models")
    bad = 0
    files = sorted(f for f in os.listdir(folder) if f.endswith(".gltf"))
    for f in files:
        problems = validate_gltf(os.path.join(folder, f))
        if problems:
            bad += 1
            print(f"{f}: {problems[:5]}")
    print(f"validated {len(files)} glTF files, {bad} with problems, {time.time() - t:.1f}s")
    return 1 if bad else 0


def cmd_preview(a) -> int:
    from .preview import render
    render(a.out, a.scenes, view=a.view, size=a.size, cut=a.cut, out_path=a.png)
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="python3 -m tools.importers", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--install", help="game install root (else BULLY_INSTALL or local_config.json)")
    ap.add_argument("--out", default="imported", help="output folder (default: imported/)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("img", help="list or extract IMG/DIR archive entries")
    p.add_argument("action", choices=["list", "extract"])
    p.add_argument("archive", nargs="?", default="Stream/World.img", help="archive path, relative to the install")
    p.add_argument("names", nargs="*", help="entries to extract (default: all, or all of --ext)")
    p.add_argument("--ext", help="filter by extension, e.g. nif")
    p.set_defaults(fn=cmd_img)

    p = sub.add_parser("nif", help="convert World.img models to glTF")
    p.add_argument("names", nargs="+")
    p.add_argument("--txd", help="override the texture dictionary (default: from the IDE)")
    p.add_argument("--file-space", action="store_true", help="keep root/model node transforms")
    p.set_defaults(fn=cmd_nif)

    p = sub.add_parser("vehicle", help="ride body (damage parts + helpers dropped) + its wheel model + rig JSON (imported/vehicles/)")
    p.add_argument("names", nargs="+", help="World.img model stems, e.g. bike SK8Board GoCart taxicab")
    p.set_defaults(fn=cmd_vehicle, cmd="vehicle")

    p = sub.add_parser("prop", help="hand prop / projectile model, helper shapes and damage parts dropped")
    p.add_argument("names", nargs="+", help="World.img model stems, e.g. slingshot spudg EGGPROJ")
    p.set_defaults(fn=cmd_vehicle, cmd="prop")

    p = sub.add_parser("ped", help="skinned character (e.g. PLAYER) to glTF with skeleton and .agr clips")
    p.add_argument("name", help="World.img model stem, e.g. PLAYER")
    p.add_argument("--group", default="C_Player", help="animation group (.agr) for the clips (default C_Player); "
                   "a comma list (V_Bike,Go_Cart,...) resolves each clip in the first group holding it")
    p.add_argument("--anims", nargs="*", help="clip names (IDLE, C_PLAYER\\RUN, or * for all); "
                                               "default: Jimmy's locomotion set")
    p.add_argument("--no-anims", action="store_true", help="skeleton and skin only")
    p.add_argument("--root-motion", default="inplace", choices=["inplace", "raw", "dummy"])
    p.add_argument("--head", help="clothing head/hair model (e.g. B_Buzz = Jimmy's default hair) that replaces the base head: "
                                  "its shape is skinned onto the ped's skeleton by joint name, the base head triangles are cut out")
    p.add_argument("--head-txd", help="texture dictionary of --head (default: its IDE row)")
    p.add_argument("--cutscene", help="comma list of cutscene ids (1-1-1,1-03,...): export this actor's baked cutscene clips "
                                      "(CUT_<id>, facial clip merged) instead of --group/--anims; use with --root-motion raw")
    p.add_argument("--cutscene-actor", help="actor name in the cutscene HXD (default: the model stem without _BODY)")
    p.add_argument("--txd", help="override the texture dictionary (default: from the IDE)")
    p.add_argument("--no-textures", action="store_true")
    p.add_argument("--sheet", help="also render a CPU contact sheet PNG (bind pose + frames per clip)")
    p.add_argument("--sheet-views", nargs="+", default=["side", "front"], choices=["side", "front"])
    p.add_argument("--sheet-frames", type=int, default=6)
    p.set_defaults(fn=cmd_ped)

    p = sub.add_parser("anim", help="list HXD groups, or the clips of one group with durations and travel")
    p.add_argument("group", nargs="?", help="group name, e.g. C_Player (omit to list groups)")
    p.add_argument("--hxd", default="MAINPED")
    p.set_defaults(fn=cmd_anim)

    p = sub.add_parser("audio", help="audio/xact/*.SEA sound banks: survey, list, cues (SDBK cue graph), footsteps (surface table), or decode to WAV (imported/audio/)")
    p.add_argument("action", choices=["survey", "list", "extract", "cues", "footsteps"])
    p.add_argument("banks", nargs="*", help="bank stems, e.g. FeetAll Birds Wind, or streamed Ambs/Cuts/Music/Speech (default: all .SEA)")
    p.add_argument("--names", help="regex on wave names (extract only)")
    p.add_argument("--print-cues", action="store_true", help="cues: also print every cue -> wave row")
    p.add_argument("--config", default="unreal/BullyRemake/Config/FootstepSurfaces.json",
                   help="footsteps: hand-authored surface -> cue-family rules")
    p.set_defaults(fn=cmd_audio)

    p = sub.add_parser("cutscene", help="baked cutscenes: list, or build camera/actors/subtitles manifest + audio WAV into <out>/cutscenes/<id>/")
    p.add_argument("action", choices=["list", "build", "actors"],
                   help="list | build (manifest + audio) | actors (every character actor -> <out>/models/*.gltf + cutscenes/actors.json)")
    p.add_argument("ids", nargs="*", help="cutscene ids as Cuts.img names them (1-1-1, 1-02B, 1-03, ...); default: the Chapter 1 opening set")
    p.add_argument("--chapter1", action="store_true", help="also build the Chapter 1 opening set (1-1-1 1-1-2 1-02B 1-02E 1-02D 1-03 1-04)")
    p.add_argument("--language", default="American", help="Config/Text/<language>.img for subtitle text (default American)")
    p.add_argument("--no-audio", action="store_true", help="skip decoding the Cuts.bin stream")
    p.set_defaults(fn=cmd_cutscene)

    p = sub.add_parser("speech", help="scripted mission speech: per-line WAV takes + speech/manifest.json (original subtitle text with --transcribe)")
    p.add_argument("missions", nargs="*", help="missions as Speech.lst spells them (1-01 1-02 1-03 1-04 = default Chapter 1 opening)")
    p.add_argument("--language", default="American", help="Config/Text/<language>.img for subtitle text (default American)")
    p.add_argument("--transcribe", action="store_true", help="match each line to its original subtitle via local faster-whisper (optional dependency)")
    p.add_argument("--model", default="small", help="faster-whisper model for --transcribe (default small; must be cached locally)")
    p.add_argument("--merge", action="store_true", help="keep the lines of an existing manifest that this run does not redo")
    p.add_argument("--extra-only", action="store_true", help="only the extra cross-bank lines the opening plays (narration, 1-11), merged")
    p.set_defaults(fn=cmd_speech)

    p = sub.add_parser("nft", help="decode .nft texture dictionaries to PNG")
    p.add_argument("names", nargs="+", help="World.img .nft names or paths to loose .nft/.txd files")
    p.set_defaults(fn=cmd_nft)

    p = sub.add_parser("ipb", help="parse placement files to JSON")
    p.add_argument("names", nargs="+")
    p.set_defaults(fn=cmd_ipb)

    p = sub.add_parser("world", help="convert the academy: placements, models, textures, scene manifest")
    p.add_argument("--preset", default="academy", choices=["academy", "core", "town", "all"])
    p.add_argument("--ipb", nargs="*", help="explicit IPB stems (overrides --preset)")
    p.add_argument("--file-space", action="store_true", help="keep root/model node transforms in models")
    p.add_argument("--convert-invisible", action="store_true", help="also convert nogo/walkable/_nd markers")
    p.set_defaults(fn=cmd_world)

    p = sub.add_parser("townlife", help="town pedestrian loops + kerbside parking spots from GRIDS.DAT and terrain -> <out>/townlife.json")
    p.add_argument("--districts", nargs="*", help="only these districts (output name or IPB stem), default all 8")
    p.add_argument("--evidence", help="directory for per-district top-down PNGs")
    p.add_argument("--raster-parking", action="store_true",
                   help="also derive parking from road-textured ground in districts with no road graph (low confidence)")
    p.set_defaults(fn=cmd_townlife)

    p = sub.add_parser("trigger", help="DAT/Trigger.img mission points, paths, trigger volumes -> JSON (always writes day_one.json)")
    p.add_argument("--list", action="store_true", help="list every .dat with entity counts and warnings, write nothing")
    p.add_argument("--files", nargs="*", help="also write <out>/triggers/<stem>.json for these .dat stems")
    p.add_argument("--all", action="store_true", help="also write all.json with every .dat file")
    p.set_defaults(fn=cmd_trigger)

    p = sub.add_parser("exits", help="Config/dat/ExitLocs.dat + areabbox.dat -> paired door links (imported/door_links_game.json)")
    p.add_argument("--manifest", help="scene manifest (default <out>/scenes/academy.json); without it no pocket/geometry checks")
    p.add_argument("--no-geometry", action="store_true", help="skip the imported-geometry walkability check (heuristic prior only)")
    p.add_argument("--handmade", help="hand-made links that stay authoritative (default tools/ue/door_links.json)")
    p.add_argument("--links-out", help="output (default <out>/door_links_game.json)")
    p.add_argument("--report", help="JSON report (default <out>/exits_report.json)")
    p.add_argument("--step", type=float, default=2.5, help="arrival distance from the trigger in metres (default 2.5)")
    p.add_argument("--previews", metavar="DIR", help="also write CPU top-down evidence PNGs into DIR")
    p.add_argument("--preview-areas", help="comma list of interior area ids to render (default all emitted)")
    p.add_argument("--preview-outdoor", type=int, default=999, help="how many outdoor close-ups (default all)")
    p.set_defaults(fn=cmd_exits)

    p = sub.add_parser("validate", help="structurally validate every glTF under --out/models")
    p.set_defaults(fn=cmd_validate)

    p = sub.add_parser("preview", help="CPU software render of imported scenes to PNG (evidence only)")
    p.add_argument("scenes", nargs="+", help="scene names from the manifest, e.g. tschool")
    p.add_argument("--view", choices=["top", "oblique"], default="top")
    p.add_argument("--size", type=int, default=1600)
    p.add_argument("--cut", type=float, help="drop triangles above this game-space height (see inside)")
    p.add_argument("--png", help="output path (default: <out>/previews/...)")
    p.set_defaults(fn=cmd_preview)

    a = ap.parse_args(argv)
    return a.fn(a)
