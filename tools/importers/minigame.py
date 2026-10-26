# OWNER: sm-minigames
"""Minigame HUD assets from your own install -> `<out>/minigames/` (git-ignored, read at runtime by
UBullyMinigameSubsystem; the game falls back to drawn keys and our own wording when this folder is absent).

  glyphs/<Name>.png   the original button glyphs from `TXD/HUDElems.nft` (Xbox 360 face buttons, keyboard keys,
                      mouse wheel, right stick, grade tick/cross)
  text.json           the original HUD strings the class / lockpick screens show, looked up in
                      `Config/Text/<Language>.img` by key (BKDR hash of the upper-cased key) or, where the key name
                      is not known, by the table hash itself. `~code~` glyph markers are stripped.

Nothing here is committed: the repo holds only this list of names and hashes.
"""

from __future__ import annotations

import json
import os
import re

from .binreader import FormatError
from .cutscene import clean_text, parse_text_table, string_hash
from .img import ImgArchive
from .nft import TextureDictionary, safe_name

# HUDElems.nft texture stems the minigame overlay draws (missing ones are reported, not fatal)
GLYPHS = (
    # Xbox 360 face buttons (the class's ClassChem actions 0..3 = cross/circle/square/triangle = A/B/X/Y)
    "HUD_buttonCross", "HUD_buttonCircle", "HUD_buttonSquare", "HUD_buttonTriangle",
    # keyboard keys the PC build of our overlay maps those actions to, and the lock dial's keys
    "Button_Up_Arrow", "Button_Down_Arrow", "Button_Left_Arrow", "Button_Right_Arrow",
    "Button_A", "Button_D", "Button_E", "Button_Enter", "Button_Backspace",
    "Button_Scroll_Up", "Button_Scroll_Down", "Analog_R",
    # lock dial direction arrows, grade marks, a ring
    "arrow_left", "arrow_right", "Grade_check", "Grade_cross", "HUD_circleoutline",
)

# (table, key or hash, alias). Keys are what the scripts pass to TextPrint/TutorialShowMessage; a hash entry is a
# string whose key name the scripts don't spell out (the alias is ours).
TEXTS = (
    ("CLASSCHEM.bin", "C4_GETREADY", None),     # attempt 1 banner
    ("CLASSCHEM.bin", "C4_GETREADY2", None),    # after the first failed experiment
    ("CLASSCHEM.bin", "C4_GETREADY3", None),    # after the second
    ("CLASSCHEM.bin", "C4_BEGIN", None),
    ("CLASSCHEM.bin", "C4_INST01", None),       # the instruction card
    ("GENERAL.bin", "M_PASS", None),
    ("GENERAL.bin", "M_FAIL", None),
    ("GENERAL.bin", "C4_UNLOCK01", None),       # class-1 unlock card (F_EndPresentation: C4_unlock01)
    ("GENERAL.bin", "TUT_CHEM1C1", None),       # class-1 tutorial card
    ("GENERAL.bin", "TUT_PICK1", None),         # lockpick tutorial (1_02B's RUSLOCKX tutorial text)
    ("GENERAL.bin", "BUT_CLASS_CHE", None),
    ("GENERAL.bin", 0x79B1BC99, "CLASS_CHEMISTRY"),
    ("GENERAL.bin", 0x079CE753, "CLASS_CHEMISTRY_1"),
    ("GENERAL.bin", 0x2D094C69, "LOCKPICK_TITLE"),
    ("GENERAL.bin", 0x3434554B, "LOCK_PICKED"),
    ("GENERAL.bin", 0x61E42C9D, "BUT_PICK_LOCK"),
    ("GENERAL.bin", 0x2EC5186E, "BUT_ROTATE_PAD"),
    ("GENERAL.bin", 0x79E2F9FC, "BUT_ROTATE_PC"),
)


_GLYPH_CODE = re.compile(r"~((?:rstick|lstick|action|class_exit|class_select|scroll_weapl|scroll_weapr|attack_l|attack_r))~", re.I)


def build(install: str, out_dir: str, language: str = "American", log=print) -> dict:
    dest = os.path.join(out_dir, "minigames")
    gdir = os.path.join(dest, "glyphs")
    os.makedirs(gdir, exist_ok=True)
    report = {"glyphs": [], "missing_glyphs": [], "texts": 0, "missing_texts": []}

    with open(os.path.join(install, "TXD", "HUDElems.nft"), "rb") as f:
        td = TextureDictionary("HUDElems", f.read())
    for stem in GLYPHS:
        t = td.textures.get(stem.lower())
        if t is None:
            report["missing_glyphs"].append(stem)
            continue
        try:
            td.export(t, os.path.join(gdir, safe_name(stem) + ".png"))
            report["glyphs"].append(stem)
        except FormatError as ex:
            log(f"glyph {stem}: {ex}")
            report["missing_glyphs"].append(stem)

    texts = {}
    tables = {}
    with ImgArchive(os.path.join(install, "Config", "Text", f"{language}.img")) as img:
        names = {n.lower(): n for n in img.names()}
        for table, key, alias in TEXTS:
            if table not in tables:
                tables[table] = parse_text_table(img.read(names[table.lower()])) if table.lower() in names else {}
            h = key if isinstance(key, int) else string_hash(key.upper())
            name = alias or key
            raw = tables[table].get(h)
            if raw is None:
                report["missing_texts"].append(name)
                continue
            # keep inline glyph markers as {code} tokens (the overlay draws/words them); clean_text drops the rest
            texts[name] = clean_text(_GLYPH_CODE.sub(lambda m: "{" + m.group(1).lower() + "}", raw))
    with open(os.path.join(dest, "text.json"), "w", encoding="utf-8") as f:
        json.dump({"_format": "bully-remake.minigame-text/1", "language": language, "text": texts}, f, indent=1,
                  ensure_ascii=False)
    report["texts"] = len(texts)
    log(f"minigames: {len(report['glyphs'])} glyphs, {len(texts)} strings -> {dest}")
    return report
