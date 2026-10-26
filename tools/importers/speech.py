# OWNER: sm-voice
"""Scripted mission speech: `audio/PLAYLIST/Speech.bin` (+ `.lst`) + `Config/Text/<Language>.img` -> per-line WAVs and
a manifest the game reads at runtime (`<out>/speech/manifest.json`). Everything written here is derived from the
owner's install and stays git-ignored; this module is original code only.

Speech.lst names scripted lines `<Speaker>_<mission>_<NNN>[_v<k>].rsm` (mission in cutscene form, `1-03`); `_v<k>` are
alternate takes. The script's `SoundPlayScriptedSpeechEvent(ped, "M_1_03", n)` id is NOT the file number (1_03 uses
ids up to 42 while its files stop at 016); the event -> file map lives in `audio/CONFIG/SpeechDB.bin`, whose key hash is
OPEN. So the manifest is keyed by file stem, and mission data picks lines by stem (`"voice": "Davis_1-03_002"`).

Subtitle text: the mission's own string table holds the original line for many files, under a key spelling that is
OPEN (see docs/re-notes/cutscenes-format.md section 7). With `transcribe=True` (needs the optional `faster-whisper`
package and a cached model) each take is transcribed locally and matched to the closest table entry: a close match
gives the ORIGINAL text (`text_source: "table"`), otherwise the transcript is kept (`"transcript"`). Without it,
`text` is null and the mission's own line text is shown.
"""
from __future__ import annotations

import difflib
import json
import os
import re
from pathlib import Path

from . import audio
from .cutscene import clean_text, parse_text_table
from .img import ImgArchive

# Missions of the Chapter 1 opening, as Speech.lst spells them, and the text tables that can hold their lines.
CHAPTER1_OPENING = ("1-01", "1-02", "1-03", "1-04")
# Lines the opening's scripts play from outside those sets: 1_02's intro-tour narration (`NARRATION` 101-104 on the
# player, Speech.lst `Jimmy_NARR1_0x_10y`) and 1_02B's locker-success line, borrowed from the 1-11 bank (`M_1_11` 21).
CHAPTER1_OPENING_EXTRA = ("Jimmy_NARR1_01_100", "Jimmy_NARR1_01_101", "Jimmy_NARR1_02_100", "Jimmy_NARR1_02_101",
                          "Gary_1-11_021")
TEXT_TABLES = ("1_01.bin", "1_02.bin", "1_02A.bin", "1_02B.bin", "1_02C.bin", "1_03.bin", "1_04.bin")

_STEM = re.compile(r"^(?P<speaker>.+?)_(?P<mission>\d+-\d+[A-Z]?|NARR\d+_\d+)_(?P<num>\d+)(?:_v(?P<take>\d+))?$")
MATCH_MIN = 0.80   # transcript vs table similarity needed to adopt the table's original text


def parse_stem(name: str) -> dict | None:
    """'Davis_1-03_002_v2' -> {speaker, mission, num, take, line}. line = the stem without the take suffix."""
    m = _STEM.match(name)
    if not m:
        return None
    take = int(m.group("take")) if m.group("take") else 1
    line = f"{m.group('speaker')}_{m.group('mission')}_{m.group('num')}"
    return {"speaker": m.group("speaker"), "mission": m.group("mission"), "num": int(m.group("num")),
            "take": take, "line": line}


def _norm(s: str) -> str:
    return re.sub(r"[^a-z0-9 ]", "", s.lower().replace("\n", " ")).strip()


def best_text_match(heard: str, candidates: list) -> tuple:
    """-> (score, text) of the closest candidate text to a transcript (difflib ratio on normalised text)."""
    nh = _norm(heard)
    best = (0.0, None)
    for text in candidates:
        r = difflib.SequenceMatcher(None, nh, _norm(text)).ratio()
        if r > best[0]:
            best = (r, text)
    return best


def table_texts(install, language: str = "American") -> list:
    path = Path(install) / "Config" / "Text" / f"{language}.img"
    out = []
    with ImgArchive(str(path)) as img:
        names = {n.lower(): n for n in img.names()}
        for t in TEXT_TABLES:
            if t.lower() in names:
                out += [clean_text(v) for v in parse_text_table(img.read(names[t.lower()])).values()]
    return [t for t in out if len(_norm(t)) > 1]


def build(install, out_dir, missions=CHAPTER1_OPENING, language: str = "American", transcribe: bool = False,
          model: str = "small", log=print, extra=CHAPTER1_OPENING_EXTRA, merge: bool = False) -> dict:
    """Extract the missions' speech takes to <out>/speech/<line>/<take>.wav and write <out>/speech/manifest.json:
    {"version": 1, "lines": {line: {speaker, mission, num, takes: [{wav, seconds}], text, text_source}}}.
    `extra` adds single lines by stem; `merge` keeps the lines of an existing manifest that this run does not redo."""
    rx = "|".join([re.escape(f"_{m}_") for m in missions] + [f"^{re.escape(s)}(?:_v\\d+)?$" for s in extra])
    if not rx:
        raise ValueError("nothing selected (no missions and no extra lines)")
    tmp = Path(out_dir) / "speech" / "_raw"
    rows = audio.extract_stream_bank(install, "Speech", tmp, names=rx)
    lines = {}
    for r in rows:
        p = parse_stem(r["name"])
        if not p:
            continue
        dest = Path(out_dir) / "speech" / p["line"] / f"v{p['take']}.wav"
        dest.parent.mkdir(parents=True, exist_ok=True)
        os.replace(Path(tmp) / r["wav"], dest)
        e = lines.setdefault(p["line"], {"speaker": p["speaker"], "mission": p["mission"], "num": p["num"],
                                         "takes": [], "text": None, "text_source": None})
        e["takes"].append({"take": p["take"], "wav": dest.relative_to(Path(out_dir) / "speech").as_posix(),
                           "seconds": r["seconds"]})
    for e in lines.values():
        e["takes"].sort(key=lambda t: t["take"])
    if transcribe and lines:
        from faster_whisper import WhisperModel   # optional dependency, local inference only
        wm = WhisperModel(model, device="cpu", compute_type="int8")
        texts = table_texts(install, language)
        for name, e in sorted(lines.items()):
            wav = Path(out_dir) / "speech" / e["takes"][0]["wav"]
            segs, _ = wm.transcribe(str(wav), language="en", beam_size=5)
            heard = " ".join(s.text.strip() for s in segs).strip()
            score, text = best_text_match(heard, texts)
            e["text"], e["text_source"] = (text, "table") if score >= MATCH_MIN else (heard or None, "transcript")
            e["match_score"] = round(score, 3)
            log(f"{name}: {e['text_source']} {score:.2f}")
    try:
        os.rmdir(tmp / "Speech")
        os.rmdir(tmp)
    except OSError:
        pass
    mp = Path(out_dir) / "speech" / "manifest.json"
    done = list(missions) + [s for s in extra if s not in missions]
    if merge and mp.exists():
        old = json.loads(mp.read_text())
        lines = {**old.get("lines", {}), **lines}
        done = list(dict.fromkeys(old.get("missions", []) + done))
    man = {"version": 1, "language": language, "missions": done, "lines": lines}
    mp.write_text(json.dumps(man, indent=1))
    return man
