# OWNER: sm-voice
"""Synthetic tests for tools/importers/speech.py (no game bytes)."""
from tools.importers import speech


def test_parse_stem_with_take():
    assert speech.parse_stem("Davis_1-03_002_v2") == {"speaker": "Davis", "mission": "1-03", "num": 2, "take": 2,
                                                      "line": "Davis_1-03_002"}


def test_parse_stem_without_take_defaults_to_one():
    p = speech.parse_stem("Danvers_1-02_086")
    assert (p["take"], p["line"]) == (1, "Danvers_1-02_086")


def test_parse_stem_rejects_non_speech_names():
    assert speech.parse_stem("Svenson_CONVG_v1") is None


def test_best_text_match_ignores_case_and_punctuation():
    score, text = speech.best_text_match("hello there new kid", ["Goodbye.", "Hello there, new kid!"])
    assert text == "Hello there, new kid!" and score > 0.95
