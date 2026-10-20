"""Shared door-link file handling for place_doors.py and import_content.py (plain Python, no `unreal` import,
so it is unit-testable: tools/tests/test_door_links_lib.py).

Which files: the BR_DOOR_LINKS environment variable (an os.pathsep-separated list of paths, set by
`run_place_doors.sh --links F [--links G] / --generated`), else the hand-made tools/ue/door_links.json alone.
Lists are merged in order; the first link with a given id wins, so put the authoritative hand-made file first.
"""
import json
import os

FORMAT = "bully-remake.door-links/1"


def resolve_files(here, env=None):
    env = os.environ if env is None else env
    spec = env.get("BR_DOOR_LINKS", "").strip()
    if not spec:
        return [os.path.join(here, "door_links.json")]
    return [p for p in spec.split(os.pathsep) if p]


def load_links(files):
    """-> (links, notes). Missing files are an error (explicit request); duplicate ids are skipped with a note."""
    links, seen, notes = [], set(), []
    for path in files:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("_format") != FORMAT:
            notes.append("%s: unexpected _format %r" % (path, data.get("_format")))
        n = 0
        for d in data["links"]:
            if d["id"] in seen:
                notes.append("%s: duplicate id %s skipped (first definition wins)" % (os.path.basename(path), d["id"]))
                continue
            seen.add(d["id"])
            links.append(d)
            n += 1
        notes.append("%s: %d links" % (os.path.basename(path), n))
    return links, notes


def problems(links):
    """Structural problems: missing partner, partner not pointing back, malformed vectors."""
    out = []
    ids = {d["id"]: d for d in links}
    for d in links:
        p = ids.get(d["partner"])
        if p is None:
            out.append("%s: partner %s not in the list" % (d["id"], d["partner"]))
        elif p["partner"] != d["id"]:
            out.append("%s: partner %s points to %s, not back" % (d["id"], d["partner"], p["partner"]))
        for k, n in (("center_cm", 3), ("half_extent_cm", 3)):
            if len(d["trigger"][k]) != n:
                out.append("%s: trigger.%s needs %d numbers" % (d["id"], k, n))
        if len(d["exit"]["location_cm"]) != 3:
            out.append("%s: exit.location_cm needs 3 numbers" % d["id"])
    return out
