# OWNER: sm-campus
"""Campus life (Bullworth Academy during Chapter 1): everything the runtime campus system (UBullyCampusSubsystem)
reads from the owner's install, written under `<out>/campus/` (git-ignored). This module is original code only.

    python3 -m tools.importers campus [--takes 3] [--no-speech] [--transcribe]

What it writes:
  campus/cast.json        one row per cast model listed in unreal/BullyRemake/Config/Campus.json: the ped type
                          (faction), stat row, female flag and voice name from Objects/default.ide's `peds` block, the
                          stat row's vision cone / range / aggression / chicken from Config/dat/pedstats.dat, plus
                          the install's faction attitude table (Config/dat/Attitude.dat), the time-cycle periods
                          (Config/timeCycl.dat) and the population per period (Config/dat/PedPop.dat, MainMap and
                          School_Hallways rows). The runtime has the same values as fallbacks in Config/Campus.json.
  campus/speech/<Speaker>/<CODE>_v<k>.wav + campus/speech/manifest.json
                          the cast's NON-mission ambient speech from audio/PLAYLIST/Speech.bin. Speech.lst names those
                          `<Speaker>_<CODE>_v<k>.rsm` (Speaker = the ide row's `N_<Name>` voice, truncated to 8 letters
                          the way the list spells it). Only the context codes in Config/Campus.json `speech.codes` are
                          taken, at most --takes takes per (speaker, code).
  campus/speech/<Lecture>/... the principal's office lecture lines (Crabble_PLECT_<nnn>, PriOff.lua's
                          PRINCIPAL_LECTURING ids). --transcribe (local faster-whisper, optional) stores a transcript
                          per lecture take so the HUD can subtitle it (git-ignored like everything here).
  campus/bell/loop.wav, campus/bell/end.wav
                          the school bell: SB_Int.SEA's "Schoolbell Ext Loop" + "School Bell Ext End" waves, which
                          both the "School Bell 2D" and "Half Bell 2D" cues (SGlFunc.lua F_RingSchoolBell) play.
  triggers/<stem>.json    the campus trigger files (patrol paths, class locations, the school-area volume, the
                          principal's office points) through the existing trigger importer.
"""
from __future__ import annotations

import json
import os
import re
from pathlib import Path

from . import audio

TRIGGER_FILES = ("Patrol_School", "Patrol_SchoolGrounds", "PrefectsMainBuilding", "PrefectsSchoolGround", "ClassLoc",
                 "PrincipalCutscene", "PriOffice", "School_Exterior", "Population", "SP_School_Hallways",
                 "tschool_doors", "ischool_doors", "tschool_hangouts")
BELL_BANK = "SB_Int"
BELL_WAVES = {"loop": "Schoolbell Ext Loop", "end": "School Bell Ext End"}
VOICE_ALIASES = {"Algernon": "Algie", "Constantinos": "Constant", "MrBurton": "Burton", "DrCrabblesnitch": "Crabble"}
FACTION_OF_TYPE = {"PREFECT": "prefect", "NERD": "nerds", "JOCK": "jocks", "DROPOUT": "townies", "GREASER": "greasers",
                   "PREPPY": "preppies", "STUDENT": "students", "COP": "cops", "TEACHER": "teachers",
                   "TOWNPERSON": "townspeople", "SHOPKEEP": "shopkeepers", "BULLY": "bullies"}
_STEM = re.compile(r"^(?P<speaker>[A-Za-z.]+)_(?P<code>[A-Za-z]+[0-9]?)_v(?P<take>\d+)$")
_LECT = re.compile(r"^(?P<speaker>[A-Za-z]+)_PLECT_(?P<num>\d{3})_v(?P<take>\d+)$")


def _lines(path: Path) -> list:
    return [l.rstrip("\r\n") for l in path.read_text(errors="replace").splitlines()]


def parse_peds(install) -> dict:
    """Objects/default.ide `peds` rows -> {model: {id, type, faction, stat, female, voice_name}} (tab separated)."""
    out, section = {}, None
    for raw in _lines(Path(install) / "Objects" / "default.ide"):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if section is None:
            section = line.lower()
            continue
        if line.lower() == "end":
            section = None
            continue
        if section != "peds":
            continue
        cols = line.split()
        if len(cols) < 7 or not cols[0].isdigit():
            continue
        name = next((c for c in cols[12:] if c.startswith("N_")), "")
        out[cols[1]] = {"id": int(cols[0]), "type": cols[5], "faction": FACTION_OF_TYPE.get(cols[5], cols[5].lower()),
                        "stat": cols[6], "female": cols[3] == "1", "name": name[2:] if name else ""}
    return out


def parse_pedstats(install) -> dict:
    """Config/dat/pedstats.dat -> {STAT_*: {vision_yaw_deg, vision_range_m, max_health, chicken, aggression, class}}."""
    out = {}
    for raw in _lines(Path(install) / "Config" / "dat" / "pedstats.dat"):
        if not raw.startswith("STAT_"):
            continue
        c = raw.split("\t")
        c = [x.strip() for x in c if x.strip() != ""]
        try:
            out[c[0]] = {"vision_yaw_deg": float(c[3]), "vision_range_m": float(c[4]), "max_health": float(c[5]),
                         "chicken": float(c[8]), "aggression": float(c[15]), "class": c[18]}
        except (IndexError, ValueError):
            continue
    return out


def parse_attitudes(install) -> dict:
    """Config/dat/Attitude.dat: one row per faction (comment line names it) -> {faction: {toward: attitude 0..4}}.
    The file's words are Adore, Dig, Dispassionate, Averse, Abhor; the script enum (PedSetDefaultTypeToTypeAttitude in
    SFaction.lua, where 4 is the end-game "everyone loves you") runs Abhor 0 .. Adore 4."""
    words = {"abhor": 0, "averse": 1, "dispassionate": 2, "dig": 3, "adore": 4}
    order = ["prefect", "nerds", "jocks", "townies", "greasers", "preppies", "students", "cops", "teachers",
             "townspeople", "shopkeepers", "bullies", "player"]
    rows = []
    for raw in _lines(Path(install) / "Config" / "dat" / "Attitude.dat"):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        vals = [words.get(w.strip().lower()) for w in s.split(",")]
        if len(vals) == len(order) and None not in vals:
            rows.append(vals)
    return {order[i]: dict(zip(order, r)) for i, r in enumerate(rows) if i < len(order) - 1}


def parse_timecycle(install) -> list:
    """Config/timeCycl.dat: `<PERIOD> <start hour> <end hour> <colours...>` -> [{id, start, end}]."""
    out = []
    for raw in _lines(Path(install) / "Config" / "timeCycl.dat"):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        c = s.split()
        if len(c) >= 3 and c[1].isdigit() and c[2].isdigit():
            out.append({"id": c[0], "start": int(c[1]), "end": int(c[2])})
    return out


def parse_pedpop(install, areas=("MainMap", "School_Hallways")) -> dict:
    """Config/dat/PedPop.dat: per area, four rows (DAY_OUTOFCLASS, DAY_INCLASS, NIGHT_EVENING, NIGHT_CURFEW) of
    total + 12 per-type counts (prefect, nerd, jock, dropout, greaser, preppy, student, cop, teacher, town, shop, bully)."""
    periods = ("day_out_of_class", "day_in_class", "night_evening", "night_curfew")
    kinds = ("prefect", "nerds", "jocks", "townies", "greasers", "preppies", "students", "cops", "teachers",
             "townspeople", "shopkeepers", "bullies")
    out, cur, rows = {}, None, []
    for raw in _lines(Path(install) / "Config" / "dat" / "PedPop.dat"):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        if "," not in s:
            if cur and cur in areas:
                out[cur] = rows
            cur, rows = s, []
            continue
        nums = [int(x) for x in re.findall(r"-?\d+", s)]
        if len(nums) >= 13 and len(rows) < 4:
            rows.append({"period": periods[len(rows)], "total": nums[0], **dict(zip(kinds, nums[1:13]))})
    if cur and cur in areas:
        out[cur] = rows
    return out


def speaker_for(name: str, speakers: set) -> str | None:
    """'Constantinos' -> 'Constant' (Speech.lst truncates speaker names to 8 letters); aliases cover renamed voices."""
    for cand in (VOICE_ALIASES.get(name), name, name[:8]):
        if cand and cand in speakers:
            return cand
    return None


def build(install, out_dir, config_path, takes: int = 3, speech: bool = True, transcribe: bool = False,
          model: str = "small", log=print) -> dict:
    cfg = json.loads(Path(config_path).read_text())
    out = Path(out_dir) / "campus"
    out.mkdir(parents=True, exist_ok=True)

    # --- triggers (patrols, class doors, school area, principal's office) via the trigger importer
    from . import trigger as T
    from .img import ImgArchive
    with ImgArchive(os.path.join(install, T.ARCHIVE)) as arch:
        files = T.read_archive(arch)
    wrote = []
    for stem in TRIGGER_FILES:
        key = next((k for k in files if k.lower() == stem.lower()), None)
        if key:
            T.write_json(os.path.join(out_dir, "triggers", key + ".json"), files[key])
            wrote.append(key)
    log(f"triggers: {', '.join(wrote)}")

    # --- cast table
    peds, stats = parse_peds(install), parse_pedstats(install)
    pl = Path(install) / "audio" / "PLAYLIST"
    lst = audio.read_stream_names(pl / "Speech.lst")
    speakers = set()
    for nm in lst:
        m = _STEM.match(Path(nm).stem)
        if m:
            speakers.add(m.group("speaker"))
    cast = {}
    for row in cfg.get("cast", []):
        mdl = row["model"]
        p = peds.get(mdl)
        if not p:
            log(f"cast {mdl}: not in default.ide peds, skipped")
            continue
        voice = row.get("voice") or speaker_for(p["name"], speakers)
        cast[mdl] = {**p, "voice": voice, **({"stats": stats[p["stat"]]} if p["stat"] in stats else {})}
    data = {"format": "bully-remake.campus-import/1", "cast": cast, "attitudes": parse_attitudes(install),
            "timecycle": parse_timecycle(install), "population": parse_pedpop(install),
            "prefect_stats": stats.get("STAT_PF_BASIC_A")}
    (out / "cast.json").write_text(json.dumps(data, indent=1))
    log(f"cast: {len(cast)} models, voices: {sorted({c['voice'] for c in cast.values() if c['voice']})}")

    # --- school bell waves
    bell = out / "bell"
    bell.mkdir(exist_ok=True)
    rows = audio.extract_bank(Path(install) / "audio" / "xact" / f"{BELL_BANK}.SEA", bell / "_raw",
                              names="^(" + "|".join(re.escape(v) for v in BELL_WAVES.values()) + ")$")
    for key, wave in BELL_WAVES.items():
        r = next((r for r in rows if r["name"] == wave), None)
        if r:
            os.replace(bell / "_raw" / r["wav"], bell / f"{key}.wav")
    _rmtree_empty(bell / "_raw")
    log(f"bell: {sorted(p.name for p in bell.glob('*.wav'))}")

    if not speech:
        return data
    # --- ambient speech for the cast + the principal's lecture
    codes = cfg.get("speech", {}).get("codes", [])
    lecture = cfg.get("speech", {}).get("lecture", {})
    voices = sorted({c["voice"] for c in cast.values() if c.get("voice")})
    sp = out / "speech"
    tmp = sp / "_raw"
    rx = "^(?:(?:" + "|".join(map(re.escape, voices)) + ")_(?:" + "|".join(map(re.escape, codes)) + r")_v\d+"
    if lecture.get("speaker"):
        nums = "|".join(f"{int(n):03d}" for n in lecture.get("ids", []))
        rx += "|" + re.escape(lecture["speaker"]) + "_PLECT_(?:" + nums + r")_v\d+"
    rx += ")$"
    # pick the takes first (Speech.lst order), so only --takes per (speaker, code) get decoded
    keep, count = [], {}
    for nm in lst:
        st = Path(nm).stem
        if not re.match(rx, st):
            continue
        m = _LECT.match(st) or _STEM.match(st)
        key = (m.group("speaker"), m.groupdict().get("code") or m.group("num"))
        if count.get(key, 0) >= takes:
            continue
        count[key] = count.get(key, 0) + 1
        keep.append(st)
    names = "^(" + "|".join(map(re.escape, keep)) + ")$" if keep else "^$"
    rows = audio.extract_stream_bank(install, "Speech", tmp, names=names) if keep else []
    man = {"format": "bully-remake.campus-speech/1", "speakers": {}, "lecture": {}}
    wm = None
    if transcribe:
        from faster_whisper import WhisperModel   # optional dependency, local inference only
        wm = WhisperModel(model, device="cpu", compute_type="int8")
    for r in sorted(rows, key=lambda r: r["name"]):
        m = _LECT.match(r["name"])
        if m:
            spk, code, take = m.group("speaker"), f"PLECT_{m.group('num')}", int(m.group("take"))
        else:
            m = _STEM.match(r["name"])
            if not m:
                continue
            spk, code, take = m.group("speaker"), m.group("code"), int(m.group("take"))
        dest = sp / spk / f"{code}_v{take}.wav"
        dest.parent.mkdir(parents=True, exist_ok=True)
        os.replace(tmp / r["wav"], dest)
        e = {"wav": dest.relative_to(sp).as_posix(), "seconds": r["seconds"]}
        if code.startswith("PLECT_"):
            if wm is not None:
                segs, _ = wm.transcribe(str(dest), language="en", beam_size=5)
                e["text"] = " ".join(s.text.strip() for s in segs).strip()
            man["lecture"].setdefault(code[6:], []).append(e)
        else:
            man["speakers"].setdefault(spk, {}).setdefault(code, []).append(e)
    _rmtree_empty(tmp)
    (sp / "manifest.json").write_text(json.dumps(man, indent=1))
    n = sum(len(t) for s in man["speakers"].values() for t in s.values())
    log(f"speech: {len(man['speakers'])} speakers, {n} takes; lecture lines {sorted(man['lecture'])}")
    data["speech"] = {"speakers": len(man["speakers"]), "takes": n, "lecture": len(man["lecture"])}
    return data


def _rmtree_empty(p: Path) -> None:
    if not p.exists():
        return
    for q in sorted(p.rglob("*"), key=lambda x: len(x.parts), reverse=True):
        try:
            q.rmdir() if q.is_dir() else None
        except OSError:
            pass
    try:
        p.rmdir()
    except OSError:
        pass
