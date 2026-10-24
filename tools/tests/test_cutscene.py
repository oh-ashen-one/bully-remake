# OWNER: sm-cutscenes
"""Synthetic fixtures for tools/importers/cutscene.py (no game bytes). Opt-in install checks at the end."""
import math
import os
import struct

import pytest

from tools.importers import cutscene as cs
from tools.importers.agr import Clip, Key
from tools.importers.binreader import FormatError


# ---- string tables -------------------------------------------------------------------------------------------

def test_string_hash_definition():
    assert cs.string_hash("") == 0
    assert cs.string_hash("A") == 65
    assert cs.string_hash("AB") == 65 * 131 + 66
    # wraps at 32 bits
    long = "Z" * 40
    x = 0
    for c in long:
        x = (x * 131 + ord(c)) % (1 << 32)
    assert cs.string_hash(long) == x


def make_table(entries: dict) -> bytes:
    """Encode {key: text} the way the install's tables are laid out (see parse_text_table)."""
    block = bytearray()
    rows = []
    for key, text in entries.items():
        while len(block) % 4:
            block.append(0)
        off = len(block) + 1            # points at the second byte of the FF FF marker
        block += b"\xff\xff" + text.encode("utf-8") + b"\0"
        rows.append((cs.string_hash(key) if isinstance(key, str) else key, off))
    while len(block) % 4:
        block.append(0)
    rows.sort()
    table = b"".join(struct.pack("<II", h, o) for h, o in rows)
    head = struct.pack("<8I", cs.TEXT_MAGIC, 1, 0, len(block), len(table), len(rows), 2, 0) + b"XY\0\0\0\0\0\0"
    return head + bytes(block) + table


def test_text_table_round_trip():
    data = make_table({"CUT.SUB0001": "First line.", "CUT.SUB0002": "Second~n~line", "OTHER": "café"})
    t = cs.parse_text_table(data)
    assert cs.lookup(t, "CUT.SUB0001") == "First line."
    assert cs.clean_text(cs.lookup(t, "CUT.SUB0002")) == "Second\nline"
    assert cs.lookup(t, "OTHER") == "café"
    assert cs.lookup(t, "MISSING") is None


def test_text_table_rejects_bad_magic_and_sizes():
    data = bytearray(make_table({"K": "v"}))
    bad = bytes([0]) + bytes(data[1:])
    with pytest.raises(FormatError):
        cs.parse_text_table(bad)
    struct.pack_into("<I", data, 16, 999)   # table length != 8 * count
    with pytest.raises(FormatError):
        cs.parse_text_table(bytes(data))


def test_clean_text_codes():
    assert cs.clean_text("Press ~ACTION~ now~n~ok") == "Press  now\nok"
    assert cs.clean_text("  plain  ") == "plain"


# ---- subtitle timing -----------------------------------------------------------------------------------------

SUB = "{1}{1}30.000\n{30}{60}CUT.SUB0001\n{60}{90}CUT.SUB0002\n"


def test_parse_sub():
    fps, rows = cs.parse_sub(SUB)
    assert fps == 30.0
    assert rows == [(30, 60, "CUT.SUB0001"), (60, 90, "CUT.SUB0002")]
    with pytest.raises(FormatError):
        cs.parse_sub("{1}{1}30\n{90}{60}BACKWARDS\n")
    with pytest.raises(FormatError):
        cs.parse_sub("{1}{1}30\nnot a line\n")


def test_subtitles_by_hash():
    t = cs.parse_text_table(make_table({"CUT.SUB0001": "One", "CUT.SUB0002": "Two"}))
    fps, subs = cs.subtitles(SUB, t)
    assert [(s["start"], s["end"], s["text"], s["match"]) for s in subs] == [(1.0, 2.0, "One", "hash"), (2.0, 3.0, "Two", "hash")]


def test_subtitles_sequence_fallback():
    # table keyed by an unknown prefix: same digit suffixes, so the sorted hashes keep the decimal spacing
    keys = [f"SOMETHINGELSE{n:04d}" for n in range(1, 13)]
    t = cs.parse_text_table(make_table({k: f"line {i + 1}" for i, k in enumerate(keys)}))
    sub = "{1}{1}30\n" + "".join(f"{{{i * 10}}}{{{i * 10 + 9}}}CUT.SUB{i:04d}\n" for i in range(1, 13))
    _fps, subs = cs.subtitles(sub, t)
    assert [s["text"] for s in subs] == [f"line {i}" for i in range(1, 13)]
    assert {s["match"] for s in subs} == {"sequence"}


def test_sequence_fallback_refuses_mismatch():
    keys = [f"P{n:04d}" for n in (1, 2, 5)]          # a gap: spacing differs from 0001..0003
    t = cs.parse_text_table(make_table({k: "x" for k in keys}))
    assert cs.sequence_match(t, ["CUT.SUB0001", "CUT.SUB0002", "CUT.SUB0003"]) is None
    assert cs.sequence_match(t, ["CUT.SUB0001", "CUT.SUB0002"]) is None   # count differs


# ---- camera track --------------------------------------------------------------------------------------------

def dat(fov_rows, roll_rows, pos_rows, tgt_rows):
    def blk(rows):
        return f"{len(rows)},\r\n" + "".join(",".join(str(v) for v in r) + ",\r\n" for r in rows)
    return ";\r\n".join([blk(fov_rows), blk(roll_rows), blk(pos_rows), blk(tgt_rows), "", ""])


def test_camera_linear_move_and_cut():
    d = (2.0, 4.0, 0.0)  # straight move over 0..2 s, tangents = segment delta
    pos = [
        [0.0, 10, 20, 1, 0, 0, 0, *d, 0, 0],
        [2.0, 12, 24, 1, *d, 0, 0, 0, 0, 0],
        [2.0333333, 12, 24, 1, 0, 0, 0, 0, 0, 0, 0, 0],
        [2.0666667, 50, 50, 5, 0, 0, 0, 0, 0, 0, 0, 0],      # one-frame jump: a cut
        [3.0, 50, 50, 5, 0, 0, 0, 0, 0, 0, 0, 0],
    ]
    tgt = [[0.0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0], [3.0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0]]
    fov = [[0.0, 70.0, 0, 0, 0, 0], [3.0, 50.0, -20, -20, 0, 0]]
    roll = [[0.0, 5.0, 0, 0, 0, 0]]
    cam = cs.parse_camera(dat(fov, roll, pos, tgt))
    assert len(cam.pos) == 5 and len(cam.target) == 2 and len(cam.fov) == 2 and len(cam.roll) == 1
    mid = cs.sample_track(cam.pos, 1.0)
    assert mid == pytest.approx((11.0, 22.0, 1.0))
    q = cs.sample_track(cam.pos, 0.5)
    assert q == pytest.approx((10.5, 21.0, 1.0))           # equal tangents = constant velocity
    baked = cs.bake_camera(cam, 3.0)
    assert baked["frames"] == 91
    assert baked["cuts"] == [61]                            # frame 61 -> 62 jumps (2.0333 -> 2.0667)
    assert baked["pos_cm"][:3] == [1000.0, -2000.0, 100.0]  # game (x, y, z) m -> UE (100x, -100y, 100z) cm
    assert baked["roll_deg"][0] == -5.0
    assert baked["fov_deg"][0] == 70.0 and baked["fov_deg"][-1] == 50.0


def test_camera_rejects_bad_counts():
    bad = "3,\n0,70,0,0,0,0,\n;\n1,\n0,0,0,0,0,0,\n;\n1,\n0,1,2,3,0,0,0,0,0,0,0,0,\n;\n1,\n0,1,2,3,0,0,0,0,0,0,0,0,\n;"
    with pytest.raises(FormatError):
        cs.parse_camera(bad)
    with pytest.raises(FormatError):
        cs.parse_camera("1,\n0,70,0,0,0,0,\n")


def test_hermite_endpoints():
    k0 = cs.TrackKey(0.0, (1.0,), (0.0,), (5.0,))
    k1 = cs.TrackKey(1.0, (3.0,), (-5.0,), (0.0,))
    assert cs.hermite(k0, k1, 0.0) == pytest.approx((1.0,))
    assert cs.hermite(k0, k1, 1.0) == pytest.approx((3.0,))


# ---- actors ---------------------------------------------------------------------------------------------------

def test_parse_inf():
    assert cs.parse_inf("3\r\nCS_A\r\nCS_B\r\nF_CS_A\r\n\0\0") == ["CS_A", "CS_B", "F_CS_A"]
    with pytest.raises(FormatError):
        cs.parse_inf("4\nCS_A\n")


def test_anim_label():
    assert cs.anim_label("1-02B") == "CUT_1_02B"
    assert cs.anim_label("1-1-1") == "CUT_1_1_1"


def _clip(n_bones, animated):
    tracks = []
    for b in range(n_bones):
        keys = [Key(0.0, (1.0, 0.0, 0.0, 0.0)), Key(1.0, (1.0, 0.0, 0.0, 0.0))]
        if b in animated:
            keys.insert(1, Key(0.5, (0.0, 1.0, 0.0, 0.0)))
        tracks.append(keys)
    c = Clip(0, 1001, 0, 1.0)
    c.tracks = tracks
    c.positions = [[] for _ in range(n_bones)]
    return c


def test_merge_face_takes_facial_tracks_only():
    body = _clip(64, animated={2, 40})
    face = _clip(64, animated={9, 20, 31})
    m = cs.merge_face(body, face)
    assert len(m.tracks[9]) == 3 and len(m.tracks[31]) == 3     # facial bones from the face clip
    assert len(m.tracks[40]) == 3 and len(m.tracks[2]) == 3     # body bones kept
    assert len(m.tracks[8]) == 2 and len(m.tracks[32]) == 2     # outside the facial range untouched
    assert len(body.tracks[9]) == 2                             # input not modified


def test_offset_relative():
    c = _clip(64, animated=set())
    c.positions[1] = [(0.0, (10.0, 20.0, 3.0)), (1.0, (11.0, 20.0, 3.0))]
    r = cs.offset_relative(c, (10.0, 20.0, 3.0))
    assert r.positions[1] == [(0.0, (0.0, 0.0, 0.0)), (1.0, (1.0, 0.0, 0.0))]
    assert c.positions[1][0][1] == (10.0, 20.0, 3.0)           # input not modified
    plain = _clip(64, animated=set())
    assert cs.offset_relative(plain, (5.0, 5.0, 5.0)) is plain   # no Offset_Node keys: unchanged


def test_agr_long_clip_with_known_sizes():
    """A clip with more keys than the scan heuristic allows still splits when the HXD sizes are given."""
    from tools.importers import agr
    import struct as st
    hdr = st.pack("<IIII", agr.HEADER_MAGIC, next(iter(agr.KEY_SIZE)), 30000, 0) + st.pack("<f", 90.0)
    assert not agr.is_header(hdr, 0, len(hdr))
    assert agr.is_header(hdr, 0, len(hdr), max_keys=1 << 20)


# ---- opt-in: the real install ---------------------------------------------------------------------------------

INSTALL = os.environ.get("BULLY_INSTALL")


@pytest.mark.skipif(not INSTALL, reason="BULLY_INSTALL not set")
def test_install_chapter1_cutscenes(tmp_path):
    from tools.importers.img import ImgArchive
    with ImgArchive(os.path.join(INSTALL, "Stream", "World.img")) as w:
        for cid in ("1-1-1", "1-02B", "1-03"):
            m = cs.build(INSTALL, cid, str(tmp_path), world_img=w, with_audio=False)
            assert m["subtitles"] and all(s["text"] for s in m["subtitles"])
            assert m["camera"]["frames"] > 30 * 30
            assert any(a["name"] == "CS_PLAY" for a in m["actors"])
            assert all(not a["name"].startswith("F_") for a in m["actors"])
            assert not m["warnings"]
