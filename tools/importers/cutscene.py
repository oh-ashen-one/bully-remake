# OWNER: sm-cutscenes
"""Baked cutscenes: `Cuts/Cuts.img` + `audio/PLAYLIST/Cuts.bin` + `Config/Text/<Language>.img` -> a per-cutscene
manifest (camera track, actors and their clips, subtitle timing + text) and the cutscene's audio as WAV.

Formats (reverse engineered from the install, our own description; see docs/re-notes/cutscenes-format.md):

  Cuts.img/<id>.dat     text camera track: four blocks separated by ';' (FOV, roll, position, look-at target).
                        Each block = "<n>," then n rows "t, value(s), in-tangent(s), out-tangent(s), e0, e1,".
                        Scalars have 6 columns, vectors 12. World coordinates in game metres.
  Cuts.img/<id>.inf     text actor list: "<n>" then n model names (F_<name> = that actor's facial clip).
  Cuts.img/<id>*.HXD    animation dictionaries (same HXD layout as Anim/MAINPED.HXD); group = <id>_CHAR / <id>_CAR /
                        <id>_PROP2 ..., clip name "<id>\\<ACTOR>", per-clip vector = the actor's world origin (game m).
  Cuts.img/<group>.agr  the clips (same AGR layout as World.img), 64-bone cutscene skeleton for characters.
  Cuts.img/<id>.SUB     MicroDVD-style timing: "{1}{1}<fps>" then "{start}{end}<KEY>" per line (frames at <fps>).
  Text/<Lang>.img/<id>_SUB.bin  string table (magic 0xABCD1234): key = BKDR hash (seed 131) of the KEY string.
  PLAYLIST/Cuts.bin     streamed bank, entry i = line i of Cuts.lst (`<id>.rsm`).

Nothing derived from the game is written anywhere but `--out` (default `imported/`, git-ignored).
"""
from __future__ import annotations

import json
import math
import os
import re
import struct
from dataclasses import dataclass, field

from .binreader import FormatError

FORMAT = "bully-remake.cutscene/1"
CAMERA_HZ = 30          # the game's cutscene clock (SUB timing header says 30.000; camera keys sit on 1/30 s)
TEXT_MAGIC = 0xABCD1234
TEXT_HEADER = 0x28      # string block starts here
FACE_TRACKS = range(9, 32)   # Facial01..Facial23 in the 64-bone cutscene skeleton (HXD bone table)

# The first-20-minutes set (Chapter 1 opening): ids as Cuts.img / Cuts.lst name them.
CHAPTER1_OPENING = ("1-1-1", "1-1-2", "1-02B", "1-02E", "1-02D", "1-03", "1-04")


# ---------------------------------------------------------------------------------------------------------------
# string tables (Config/Text/<Language>.img)

def string_hash(key: str) -> int:
    """BKDR-style hash, seed 131, over the key's bytes (case as given). VERIFIED: every SUB key of every Chapter-1
    cutscene resolves in its table."""
    h = 0
    for c in key.encode("latin-1"):
        h = (h * 131 + c) & 0xFFFFFFFF
    return h


_CODE = re.compile(r"~([A-Za-z0-9_$]{1,24})~")


def clean_text(s: str) -> str:
    """Display text: `~n~` -> newline, every other `~code~` (button glyphs, colour/italic switches) dropped."""
    s = s.replace("~n~", "\n")
    return _CODE.sub("", s).strip()


def parse_text_table(data: bytes) -> dict:
    """-> {hash: raw text}. Layout: u32 magic, u32 version (1), u32 0, u32 string-block length, u32 table length
    (= 8 x count), u32 count, u32 (2), u32 0, 8 bytes ("BD" + zeros), string block, then count x (u32 hash, u32 offset)
    sorted by hash. An offset points at the second byte of an FF FF pair; the UTF-8 text follows, NUL-terminated."""
    if len(data) < TEXT_HEADER:
        raise FormatError("text table: too short")
    magic, _ver, _z, slen, tlen, n = struct.unpack_from("<6I", data, 0)
    if magic != TEXT_MAGIC:
        raise FormatError(f"text table: bad magic 0x{magic:08x}")
    if tlen != 8 * n or TEXT_HEADER + slen + tlen > len(data):
        raise FormatError(f"text table: inconsistent sizes (strings {slen}, table {tlen}, count {n})")
    tb = TEXT_HEADER + slen
    out = {}
    for i in range(n):
        h, off = struct.unpack_from("<II", data, tb + 8 * i)
        p = TEXT_HEADER + off
        if p >= TEXT_HEADER + slen or data[p] != 0xFF:
            raise FormatError(f"text table: entry {i} offset {off} does not point at a string marker")
        q = data.find(b"\0", p + 1)
        if q < 0:
            raise FormatError(f"text table: entry {i} is not terminated")
        out[h] = data[p + 1:q].decode("utf-8", errors="replace")
    return out


def lookup(table: dict, key: str) -> str | None:
    return table.get(string_hash(key))


# ---------------------------------------------------------------------------------------------------------------
# subtitle timing (Cuts.img/<id>.SUB)

_SUB_LINE = re.compile(r"^\{(-?\d+)\}\{(-?\d+)\}(.*)$")


def parse_sub(text: str) -> tuple:
    """-> (fps, [(start_frame, end_frame, key)]). The first line is "{1}{1}<fps>"."""
    fps = float(CAMERA_HZ)
    rows = []
    for i, line in enumerate(l.strip() for l in text.replace("\r", "").split("\n")):
        if not line:
            continue
        m = _SUB_LINE.match(line)
        if not m:
            raise FormatError(f"SUB line {i + 1}: not {{start}}{{end}}text")
        a, b, rest = int(m.group(1)), int(m.group(2)), m.group(3).strip()
        if i == 0 and a == 1 and b == 1:
            try:
                fps = float(rest)
                continue
            except ValueError:
                pass
        if b < a:
            raise FormatError(f"SUB line {i + 1}: end {b} before start {a}")
        rows.append((a, b, rest))
    if fps <= 0:
        raise FormatError("SUB: frame rate must be positive")
    return fps, rows


_KEY_NUM = re.compile(r"^(.*?)(\d+)$")


def sequence_match(table: dict, keys: list) -> dict | None:
    """Fallback key -> text mapping for tables whose key prefix is not the SUB file's (1-02B/1-02D/1-02E: the
    prefix hash is not the hash of any name we tried). The keys are "<prefix>0001".."<prefix>00NN", so with the
    multiplicative hash h(P + d) = h(P) * 131^len(d) + h(d) the table's sorted hashes differ exactly by
    h(d[k+1]) - h(d[k]). When the table has one entry per key and every difference matches, entry k of the sorted
    table is key k. Returns {key: raw text} or None when the structure does not match."""
    if not table or len(table) != len(keys):
        return None
    parts = [_KEY_NUM.match(k) for k in keys]
    if not all(parts) or len({(m.group(1), len(m.group(2))) for m in parts}) != 1:
        return None
    nums = [m.group(2) for m in parts]
    if [int(n) for n in nums] != sorted(int(n) for n in nums):
        return None
    hs = sorted(table)
    for i in range(len(hs) - 1):
        want = (string_hash(nums[i + 1]) - string_hash(nums[i])) & 0xFFFFFFFF
        if (hs[i + 1] - hs[i]) & 0xFFFFFFFF != want:
            return None
    return {k: table[h] for k, h in zip(keys, hs)}


def subtitles(sub_text: str, table: dict | None) -> tuple:
    """-> (fps, [{start, end, key, text|None, match}]) with times in seconds. match = "hash" (the SUB key hashes to a
    table entry) or "sequence" (sequence_match fallback)."""
    fps, rows = parse_sub(sub_text)
    seq = None
    if table and not any(lookup(table, key) is not None for _a, _b, key in rows):
        seq = sequence_match(table, [key for _a, _b, key in rows])
    out = []
    for a, b, key in rows:
        raw, how = (lookup(table, key), "hash") if table else (None, None)
        if raw is None and seq is not None:
            raw, how = seq.get(key), "sequence"
        out.append({"start": round(a / fps, 4), "end": round(b / fps, 4), "key": key,
                    "text": clean_text(raw) if raw is not None else None, "match": how if raw is not None else None})
    return fps, out


# ---------------------------------------------------------------------------------------------------------------
# camera track (Cuts.img/<id>.dat)

@dataclass
class TrackKey:
    t: float
    v: tuple            # value (1 or 3 components)
    tin: tuple          # in-tangent (arriving at this key), per segment
    tout: tuple         # out-tangent (leaving this key), per segment
    ease: tuple = (0.0, 0.0)   # two trailing columns: OPEN (ease to/from; non-zero on a few keys)


@dataclass
class CameraTrack:
    fov: list = field(default_factory=list)
    roll: list = field(default_factory=list)
    pos: list = field(default_factory=list)
    target: list = field(default_factory=list)


def _parse_block(block: str, width: int, name: str) -> list:
    lines = [l.strip() for l in block.replace("\r", "").split("\n") if l.strip()]
    if not lines:
        return []
    try:
        n = int(lines[0].rstrip(","))
    except ValueError:
        raise FormatError(f"camera {name}: bad key count {lines[0]!r}")
    rows = lines[1:]
    if len(rows) != n:
        raise FormatError(f"camera {name}: header says {n} keys, found {len(rows)}")
    cols = 1 + 3 * width + 2      # t, value, in-tangent, out-tangent, two trailing columns
    keys = []
    for r in rows:
        f = [x for x in r.split(",") if x.strip() != ""]
        if len(f) != cols:
            raise FormatError(f"camera {name}: row {r!r} has {len(f)} columns, expected {cols}")
        v = [float(x) for x in f]
        k = TrackKey(v[0], tuple(v[1:1 + width]), tuple(v[1 + width:1 + 2 * width]),
                     tuple(v[1 + 2 * width:1 + 3 * width]), (v[-2], v[-1]))
        if keys and k.t < keys[-1].t:
            raise FormatError(f"camera {name}: key times go backwards at {k.t}")
        keys.append(k)
    return keys


def parse_camera(text: str) -> CameraTrack:
    blocks = text.replace("\0", "").split(";")
    if len(blocks) < 4:
        raise FormatError(f"camera: {len(blocks)} blocks, expected at least 4 (fov, roll, position, target)")
    return CameraTrack(_parse_block(blocks[0], 1, "fov"), _parse_block(blocks[1], 1, "roll"),
                       _parse_block(blocks[2], 3, "position"), _parse_block(blocks[3], 3, "target"))


def hermite(k0: TrackKey, k1: TrackKey, t: float) -> tuple:
    """Cubic Hermite between two keys with per-segment tangents (k0's out, k1's in). INFERRED from the data: a
    constant-velocity move stores both tangents equal to the segment delta, which this reproduces as a straight line."""
    span = k1.t - k0.t
    s = 0.0 if span <= 0 else min(1.0, max(0.0, (t - k0.t) / span))
    s2, s3 = s * s, s * s * s
    h00, h10, h01, h11 = 2 * s3 - 3 * s2 + 1, s3 - 2 * s2 + s, -2 * s3 + 3 * s2, s3 - s2
    return tuple(h00 * a + h10 * m0 + h01 * b + h11 * m1 for a, m0, b, m1 in zip(k0.v, k0.tout, k1.v, k1.tin))


def sample_track(keys: list, t: float) -> tuple | None:
    if not keys:
        return None
    if t <= keys[0].t:
        return keys[0].v
    if t >= keys[-1].t:
        return keys[-1].v
    lo, hi = 0, len(keys) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if keys[mid].t <= t:
            lo = mid
        else:
            hi = mid
    return hermite(keys[lo], keys[hi], t)


def game_to_ue_cm(p) -> list:
    return [p[0] * 100.0, -p[1] * 100.0, p[2] * 100.0]


def bake_camera(cam: CameraTrack, duration: float, hz: int = CAMERA_HZ, cut_m: float = 0.75) -> dict:
    """Sample the four tracks at `hz` from t=0 to `duration` into UE units. A segment between two frames whose
    position or target jumps more than `cut_m` metres is flagged as a cut (the game holds a key one frame before the
    jump: a hard cut must not be blended)."""
    n = int(math.floor(duration * hz + 1e-6)) + 1
    pos, tgt, fov, roll, cuts = [], [], [], [], []
    prev = None
    for i in range(n):
        t = i / hz
        p = sample_track(cam.pos, t) or (0.0, 0.0, 0.0)
        g = sample_track(cam.target, t) or (p[0] + 1.0, p[1], p[2])
        f = sample_track(cam.fov, t)
        r = sample_track(cam.roll, t)
        if prev is not None:
            jump = max(math.dist(p, prev[0]), math.dist(g, prev[1]))
            if jump > cut_m:
                cuts.append(i - 1)          # segment (i-1 -> i) is a cut
        prev = (p, g)
        pos.extend(round(c, 2) for c in game_to_ue_cm(p))
        tgt.extend(round(c, 2) for c in game_to_ue_cm(g))
        fov.append(round(f[0], 3) if f else 70.0)
        roll.append(round(-(r[0]), 3) if r else 0.0)   # handedness flip mirrors roll (INFERRED)
    return {"hz": hz, "frames": n, "pos_cm": pos, "target_cm": tgt, "fov_deg": fov, "roll_deg": roll, "cuts": cuts,
            "fov_convention": "horizontal at 4:3 (INFERRED); widen Hor+ for wider screens"}


# ---------------------------------------------------------------------------------------------------------------
# actors (Cuts.img/<id>.inf + <id>*.HXD)

def parse_inf(text: str) -> list:
    toks = [t.strip() for t in text.replace("\0", "").replace("\r", "").split("\n") if t.strip()]
    if not toks:
        raise FormatError("inf: empty")
    try:
        n = int(toks[0])
    except ValueError:
        raise FormatError(f"inf: bad count {toks[0]!r}")
    names = toks[1:]
    if len(names) != n:
        raise FormatError(f"inf: header says {n} names, found {len(names)}")
    return names


def anim_label(cut_id: str) -> str:
    """AnimSequence / glTF clip name for one cutscene's clip on an actor (unique per actor folder)."""
    return "CUT_" + re.sub(r"[^A-Za-z0-9]", "_", cut_id)


def cut_hxds(img, cut_id: str) -> list:
    """Every Cuts.img HXD that belongs to `cut_id` (<id>_CS.HXD, <id>_CS_CAR.HXD, <id>_CSPROP2.hxd, ...)."""
    pre = cut_id.lower() + "_"
    return [e for e in img.entries if e.ext == "hxd" and e.name.lower().startswith(pre)]


@dataclass
class ActorClip:
    actor: str
    group: str
    hxd: str
    index: int
    duration: float
    vector: tuple
    record: object = None


def actor_clips(img, cut_id: str) -> list:
    """All (actor, clip) records of a cutscene, in HXD order. Facial clips keep their F_ name."""
    from .hxd import parse_hxd
    out = []
    for e in cut_hxds(img, cut_id):
        h = parse_hxd(img.read_entry(e))
        for g in h.groups:
            for k, r in enumerate(h.group_anims(g.index)):
                out.append(ActorClip(r.short_name, g.name, e.name, k, r.duration, tuple(r.vector), r))
    return out


def load_group_clips(img, hxd_name: str, group: str):
    """Decode one cutscene animation group: -> (hxd, [(record, Clip)])."""
    from .agr import parse_agr
    from .hxd import parse_hxd
    h = parse_hxd(img.read(hxd_name))
    recs = h.group_anims(group)
    e = img.find(group + ".agr")
    if e is None:
        raise FormatError(f"Cuts.img has no {group}.agr")
    clips = parse_agr(img.read_entry(e), [r.agr_size - 4 for r in recs])
    if len(clips) != len(recs):
        raise FormatError(f"{group}.agr: {len(clips)} clips, {hxd_name} lists {len(recs)}")
    return h, list(zip(recs, clips))


def merge_face(body, face, tracks=FACE_TRACKS):
    """Body clip with the facial bones' rotation tracks taken from the F_ clip (the two clips animate disjoint bone
    sets: the body clip holds two constant keys on Facial01..23, the face clip only animates those). Returns a new Clip."""
    import copy
    out = copy.copy(body)
    out.tracks = list(body.tracks)
    out.positions = list(body.positions)
    for k in tracks:
        if k < len(face.tracks) and k < len(out.tracks) and face.tracks[k]:
            out.tracks[k] = face.tracks[k]
            if face.positions[k]:
                out.positions[k] = face.positions[k]
    return out


def actor_clip_requests(cut_img, cut_ids, actor: str) -> list:
    """ped ClipRequests for one cutscene actor: per cutscene id, its body clip (labelled CUT_<id>) with the actor's
    F_ facial clip merged in. The cutscene skeleton (64 bones) is index-aligned with the CS_* NIF skeletons (same order,
    different bone names), which is how `ped` binds tracks to joints. Cutscenes the actor is not in are skipped."""
    from .ped import ClipRequest
    out = []
    for cid in cut_ids:
        clips = actor_clips(cut_img, cid)
        body = next((c for c in clips if c.actor.upper() == actor.upper()), None)
        if body is None:
            continue
        face = next((c for c in clips if c.actor.upper() == "F_" + actor.upper()), None)
        _h, pairs = load_group_clips(cut_img, body.hxd, body.group)
        clip = pairs[body.index][1]
        meta = {"cutscene": cid, "actor": body.actor, "group": body.group, "hxd": body.hxd, "index_in_group": body.index,
                "offset_game_m": [round(v, 4) for v in body.vector], "face_clip": None}
        if face is not None:
            fh = pairs if (face.hxd, face.group) == (body.hxd, body.group) else load_group_clips(cut_img, face.hxd, face.group)[1]
            clip = merge_face(clip, fh[face.index][1])
            meta["face_clip"] = face.actor
        clip = offset_relative(clip, body.vector)
        out.append(ClipRequest(anim_label(cid), clip, body.duration, meta))
    return out


OFFSET_TRACK = 1   # "Offset_Node": carries the scene yaw (rotation) and, when keyed, the actor's world origin


def offset_relative(clip, vector):
    """The actor origin (HXD vector) is applied by the player at spawn; when the clip ALSO keys Offset_Node positions
    (they start at that same world origin in every case seen), make them relative so the origin is not applied twice."""
    import copy
    if len(clip.positions) <= OFFSET_TRACK or not clip.positions[OFFSET_TRACK]:
        return clip
    out = copy.copy(clip)
    out.positions = list(clip.positions)
    out.positions[OFFSET_TRACK] = [(t, tuple(p[i] - vector[i] for i in range(3))) for t, p in clip.positions[OFFSET_TRACK]]
    return out


# ---------------------------------------------------------------------------------------------------------------
# build

def stream_index_of(install: str, cut_id: str) -> int | None:
    from .audio import read_stream_names
    lst = os.path.join(install, "audio", "PLAYLIST", "Cuts.lst")
    if not os.path.exists(lst):
        return None
    for i, n in enumerate(read_stream_names(lst)):
        if os.path.splitext(n)[0].lower() == cut_id.lower():
            return i
    return None


def extract_audio(install: str, cut_id: str, dest_wav: str) -> dict | None:
    from . import audio
    i = stream_index_of(install, cut_id)
    if i is None:
        return None
    binp = os.path.join(install, "audio", "PLAYLIST", "Cuts.bin")
    idx = audio.read_stream_index(binp)
    bank = audio.stream_entry(binp, idx, i)
    if not bank.entries:
        return None
    e = bank.entries[0]
    pcm = audio.decode_entry(bank, e)
    audio.write_wav(dest_wav, pcm, e.channels, e.sample_rate)
    return {"wav": os.path.basename(dest_wav), "stream_index": i, "sample_rate": e.sample_rate,
            "channels": e.channels, "seconds": round(len(pcm) / e.channels / e.sample_rate, 4)}


def text_table(install: str, cut_id: str, language: str = "American") -> dict | None:
    from .img import ImgArchive
    p = os.path.join(install, "Config", "Text", language + ".img")
    if not os.path.exists(p):
        return None
    with ImgArchive(p) as t:
        e = t.find(cut_id + "_SUB.bin")
        return parse_text_table(t.read_entry(e)) if e is not None else None


def actor_model(world_img, actor: str) -> dict:
    """World.img model(s) for a cutscene actor: <ACTOR>.nif, plus <ACTOR>_BODY.nif when the actor is a head + body
    pair (CS_PLAY)."""
    body = world_img.find(actor + "_BODY.nif") if world_img is not None else None
    head = world_img.find(actor + ".nif") if world_img is not None else None
    if body is not None and head is not None:
        return {"nif": body.stem, "head": head.stem}
    return {"nif": head.stem if head is not None else None, "head": None}


def build(install: str, cut_id: str, out_dir: str, language: str = "American", with_audio: bool = True,
          world_img=None) -> dict:
    """Write <out_dir>/cutscenes/<id>/cutscene.json (+ <id>.wav) and return the manifest."""
    from .img import ImgArchive
    dest = os.path.join(out_dir, "cutscenes", cut_id)
    os.makedirs(dest, exist_ok=True)
    warnings = []
    with ImgArchive(os.path.join(install, "Cuts", "Cuts.img")) as img:
        def text_of(name):
            e = img.find(name)
            return img.read_entry(e).rstrip(b"\0").decode("latin-1") if e is not None else None

        cam_text = text_of(cut_id + ".dat")
        if cam_text is None:
            raise FormatError(f"Cuts.img has no {cut_id}.dat")
        cam = parse_camera(cam_text)
        inf_text = text_of(cut_id + ".inf")
        inf = parse_inf(inf_text) if inf_text else []
        clips = actor_clips(img, cut_id)
        sub_text = text_of(cut_id + ".SUB")

    table = text_table(install, cut_id, language)
    subs, sub_fps = [], CAMERA_HZ
    if sub_text:
        sub_fps, subs = subtitles(sub_text, table)
        missing = [s["key"] for s in subs if s["text"] is None]
        if missing:
            warnings.append(f"{len(missing)} subtitle key(s) without text: {missing[:3]}")
    aud = extract_audio(install, cut_id, os.path.join(dest, cut_id + ".wav")) if with_audio else None
    if with_audio and aud is None:
        warnings.append("no audio stream in Cuts.lst")

    cam_end = max([k[-1].t for k in (cam.pos, cam.target, cam.fov) if k] or [0.0])
    clip_end = max([c.duration for c in clips] or [0.0])
    duration = max(cam_end, clip_end, aud["seconds"] if aud else 0.0)

    face = {c.actor[2:]: c for c in clips if c.actor.upper().startswith("F_")}
    actors = []
    for c in clips:
        if c.actor.upper().startswith("F_"):
            continue
        model = actor_model(world_img, c.actor) if world_img is not None else {"nif": c.actor, "head": None}
        f = face.get(c.actor)
        kind = "character" if c.group.upper().endswith("_CHAR") else "prop"
        actors.append({
            "name": c.actor, "kind": kind, "group": c.group, "hxd": c.hxd, "clip_index": c.index,
            "clip": anim_label(cut_id), "duration": round(c.duration, 4),
            "offset_game_m": [round(v, 4) for v in c.vector], "offset_cm": [round(v, 2) for v in game_to_ue_cm(c.vector)],
            "face_clip": f.actor if f else None, "model": model,
            "ue_mesh": f"/Game/Imported/Characters/{c.actor}/SK_{c.actor}.SK_{c.actor}",
            "ue_anim": f"/Game/Imported/Characters/{c.actor}/Anims/{anim_label(cut_id)}.{anim_label(cut_id)}",
        })
    listed = {n.upper() for n in inf}
    for a in actors:
        if a["name"].upper() not in listed:
            warnings.append(f"actor {a['name']} has a clip but is not in {cut_id}.inf")

    man = {
        "_format": FORMAT, "id": cut_id, "duration_s": round(duration, 4), "language": language,
        "audio": aud, "subtitle_fps": sub_fps, "subtitles": subs,
        "camera": bake_camera(cam, duration),
        "camera_keys": {"fov": len(cam.fov), "roll": len(cam.roll), "position": len(cam.pos), "target": len(cam.target)},
        "inf": inf, "actors": actors, "warnings": warnings,
        "source": {"cuts_img": f"Cuts/Cuts.img/{cut_id}.dat|.inf|.SUB|*.HXD|*.agr",
                   "audio": "audio/PLAYLIST/Cuts.bin" if aud else None,
                   "text": f"Config/Text/{language}.img/{cut_id}_SUB.bin" if table else None},
    }
    with open(os.path.join(dest, "cutscene.json"), "w") as f:
        json.dump(man, f, indent=1)
    return man


def list_cutscenes(install: str) -> list:
    """Every cutscene id in Cuts.img (a .dat with a matching .inf), with what each has."""
    from .img import ImgArchive
    with ImgArchive(os.path.join(install, "Cuts", "Cuts.img")) as img:
        names = {e.name.lower() for e in img.entries}
        ids = sorted(e.stem for e in img.entries if e.ext == "dat" and f"{e.stem.lower()}.inf" in names)
        out = []
        for cid in ids:
            out.append({"id": cid, "sub": f"{cid.lower()}.sub" in names, "hxds": [e.name for e in cut_hxds(img, cid)],
                        "audio": stream_index_of(install, cid) is not None})
    return out
