# OWNER: builder (phase2-doors)
"""ExitLocs.dat / areabbox.dat parsing, pairing, link building and the geometry direction check, on synthetic text."""
import importlib.util
import json
import math
import os

import numpy as np

from tools.importers import cli
from tools.importers import exits as X
from tools.importers import exits_build as B
from tools.importers.exits_geom import Local

EXIT_TEXT = (
    "# header comment\r\n"
    "# AreaID, ExitID, x, y, z\r\n\r\n"
    "MainMap,\t\t\t\t0,\t100.0,\t\t-50.0,\t\t5.0\t\t\t#->Downtown\r\n"
    "MainMap,\t\t\t\t1,\t200.0,\t\t-60.0,\t\t6.0\t\t\t#->Library\r\n"
    "MainMap,\t\t\t\t2,\t300.0,\t\t-70.0,\t\t7.0\t\t\t#->Boy'sDorm?\r\n"
    "MainMap,\t\t\t\t3,\t310.0,\t\t-80.0,\t\t7.0\t\t\t#->Rich_Barb\r\n"
    "MainMap,\t\t\t\t4,\t320.0,\t\t-90.0,\t\t7.0\t\t\t#->Janitor'sRoom\r\n"
    "MainMap,\t\t\t\t5,\t330.0,\t\t-95.0,\t\t7.0\t\t\t#->FireWorks\r\n"
    "MainMap,\t\t\t\t6,\t207.0,\t\t-73.0,\t\t8.0\t\t\t#->SchoolHallways\r\n"
    "Everywhere,\t\t0,\t0.0,\t0.0,\t0.0\t#\r\n"
    "School_Hallways,\t0,\t-628.0,\t-330.0,\t-1.5\t#->MainMap\r\n"
    "School_Hallways,\t1,\t-640.0,\t-295.0,\t0.0\t#->Chemlab\r\n"
    "School_Hallways,\t2,\t-650.0,\t-300.0,\t0.0\t#->Cafeteria\r\n"
    "Chem_Lab,\t0,\t-595.0,\t320.0,\t34.2\t#->SchoolHallways\r\n"
    "#Gym,\t0,\t49.8,\t39.8,\t59.6\t#->MainMap\r\n"
    "Library,\t0,\t-789.0,\t203.0,\t90.1\t#->MainMap\r\n"
    "Boys_Dorm,\t0,\t-500.0,\t310.0,\t31.5\t#->MainMap\r\n"
    "Hair_Salon, \t0,\t-766.0, 15.9, 2.9,\t#->MainMap\r\n"          # trailing comma in the numbers
    "Janitors_Room,\t0,\t-776.0,\t-59.0,\t12.0\t#->SchoolHallways,\r\n"  # trailing comma in the comment
    "Fire_Works,\t0,\t330.0,\t-95.0,\t7.0\t#->Island2\r\n"            # copy-paste of the outdoor point
    "Staff_Room,\t0,\t-38.0,\t3.0,\t-0.4\t#->SchoolHallways\r\n"     # stale first listing
    "Staff_Room,\t0,\t-650.0,\t225.0,\t-0.4\t#->SchoolHallways\r\n"
    "School_Hallways,\t3,\t-655.0,\t-290.0,\t0.0\t#->Staffroom\r\n"
)

BBOX_TEXT = (
    "# AreaID Left Right Top Bottom Base Height RadarMap\r\n"
    "MainMap,\t\t-227.7\t761.4\t-678.1\t780.0\t-26.2\t68.2\tOutdoor\r\n"
    "Everywhere,\t0.0\t0.0\t0.0\t0.0\t0.0\t0.0\tIndoor_FirstFloor\r\n"
    "School_Hallways,\t-678.5\t-579.7\t-343.1\t-261.3\t-1.4\t15.2\tIndoor_MultiFloor\r\n"
    "Chem_Lab,\t-601.1\t-589.0\t314.3\t328.0\t34.1\t38.1\tIndoor_FirstFloor\r\n"
    "Library,\t-790.1\t-756.9\t179.3\t226.7\t89.7\t101.9\tIndoor_MultiFloor\r\n"
    "Boys_Dorm,\t-527.4\t-478.0\t305.0\t330.9\t31.2\t44.1\tIndoor_MultiFloor\r\n"
    "Hair_Salon,\t-773.2\t-760.3\t6.2\t33.5\t-1.7\t8.0\tIndoor_FirstFloor\r\n"
    "Janitors_Room,\t-797.6\t-728.8\t-163.4\t-40.9\t2.9\t21.3\tIndoor_FirstFloor\r\n"
    "Fire_Works,\t-613.7\t-604.8\t379.4\t392.9\t18.2\t22.8\tIndoor_FirstFloor\r\n"
    "Staff_Room,\t-657.1\t-641.8\t220.8\t231.1\t-0.4\t4.0\tIndoor_FirstFloor\r\n"
)


def test_parse_exit_rows():
    ex, warn = X.parse_exit_locs(EXIT_TEXT)
    assert warn == []
    live = [e for e in ex if not e.disabled]
    assert len(live) == 20 and sum(e.disabled for e in ex) == 1
    lib = next(e for e in ex if e.area == "MainMap" and e.dest == "Library")
    assert (lib.exit_id, lib.x, lib.y, lib.z) == (1, 200.0, -60.0, 6.0)
    assert next(e for e in ex if e.dest == "Boy'sDorm?").dkey == "boysdorm"
    assert next(e for e in ex if e.area == "Hair_Salon").z == 2.9                 # trailing comma ok
    assert next(e for e in ex if e.area == "Janitors_Room").dest == "SchoolHallways"  # trailing comma stripped
    assert next(e for e in ex if e.area == "Everywhere").dest is None
    gym = next(e for e in ex if e.area == "Gym")
    assert gym.disabled and gym.dest == "MainMap"
    assert [e.instance for e in ex if e.area == "Staff_Room"] == [0, 1]


def test_parse_reports_garbage():
    ex, warn = X.parse_exit_locs("Foo, bar\nBaz, 1, a, b, c\n# just a comment\n")
    assert ex == [] and len(warn) == 2


def test_parse_bbox():
    b = X.parse_area_bbox(BBOX_TEXT)
    lib = b["library"]
    assert (lib.x0, lib.x1, lib.y0, lib.y1, lib.z0, lib.z1) == (-790.1, -756.9, 179.3, 226.7, 89.7, 101.9)
    assert not b["everywhere"].enabled and b["mainmap"].enabled
    assert lib.contains(-789.0, 203.0, 90.1) and not lib.contains(0, 0)
    assert b["schoolhallways"].centre[0] < -600


def test_keys_and_aliases():
    assert X.key("Boy'sDorm?") == "boysdorm" == X.key("Boys_Dorm")
    assert X.canon("Rich_Barb") == "hairsalon" == X.canon("Hair_Salon")
    assert X.canon("Funhouse") == "houseofmirrors" and X.canon("Tenements") == X.canon("Tenement")
    assert X.slug("School_Hallways") == "school_hallways"


def _pairs():
    ex, _ = X.parse_exit_locs(EXIT_TEXT)
    boxes = X.parse_area_bbox(BBOX_TEXT)
    pairs, unpaired = X.pair_exits(ex, boxes)
    return pairs, unpaired


def test_pairing():
    pairs, unpaired = _pairs()
    names = {(p.a.ref, p.b.ref, p.kind) for p in pairs}
    assert ("MainMap#1", "Library#0", "outdoor-interior") in names
    assert ("MainMap#2", "Boys_Dorm#0", "outdoor-interior") in names
    assert ("MainMap#3", "Hair_Salon#0", "outdoor-interior") in names          # Rich_Barb alias
    assert ("MainMap#5", "Fire_Works#0", "outdoor-interior") in names          # Island2 = outdoors
    assert ("School_Hallways#0", "School_Hallways#0", "x") not in names
    ii = [p for p in pairs if p.kind == "interior-interior"]
    assert {frozenset((p.a.area, p.b.area)) for p in ii} == {frozenset(("School_Hallways", "Chem_Lab")),
                                                           frozenset(("School_Hallways", "Staff_Room"))}
    staff = next(p for p in ii if "Staff_Room" in (p.a.area, p.b.area))
    assert next(e for e in (staff.a, staff.b) if e.area == "Staff_Room").x == -650.0   # the entry inside the area box
    # the hallway's exit to the outdoors has no MainMap exit to a "SchoolHallways" in this file: it is unpaired
    un = {e.ref: r for e, r in unpaired}
    assert "MainMap#0" in un and "no interior area" in un["MainMap#0"]
    assert "MainMap#4" in un and "no exit back to MainMap" in un["MainMap#4"]   # Janitor's room leads elsewhere
    assert "School_Hallways#2" in un and "no area entry" in un["School_Hallways#2"]   # Cafeteria
    assert "Gym#0" in un and "commented out" in un["Gym#0"]
    assert "Everywhere#0" in un and "placeholder" in un["Everywhere#0"]
    assert any("duplicate listing" in r for r in un.values())                   # stale Staff_Room entry
    assert ("MainMap#6", "School_Hallways#0", "outdoor-interior") in names
    # every live exit is either paired or unpaired, once
    ex, _ = X.parse_exit_locs(EXIT_TEXT)
    assert len(pairs) * 2 + len(unpaired) == len(ex)


def test_check_exit_box_and_pockets():
    ex, _ = X.parse_exit_locs(EXIT_TEXT)
    boxes = X.parse_area_bbox(BBOX_TEXT)
    pockets = [{"name": "isc_lib", "kind": "interior", "lo": [-799, 181, 90], "hi": [-757, 224, 101]},
               {"name": "tschool", "kind": "exterior", "lo": [-80, -260, 0], "hi": [150, 90, 60]}]
    lib = next(e for e in ex if e.area == "Library")
    c = X.check_exit(lib, boxes, pockets)
    assert c["in_box"] and c["in_pocket"] and c["scenes"] == ["isc_lib"] and c["problems"] == []
    fw = next(e for e in ex if e.area == "Fire_Works")                          # outdoor point in an interior area
    c = X.check_exit(fw, boxes, pockets)
    assert not c["in_box"] and any("outside the area box" in p for p in c["problems"])
    assert any("no imported interior pocket" in p for p in c["problems"])
    out = next(e for e in ex if e.area == "MainMap" and e.dest == "Library")    # (200,-60) outside tschool
    c = X.check_exit(out, boxes, pockets)
    assert c["in_pocket"] is False and c["scenes"] == ["tschool"]


def test_direction_math():
    assert X.snap(5, 1) == (1.0, 0.0) and X.snap(-1, -4) == (0.0, -1.0) and X.snap(0, 0) == (1.0, 0.0)
    assert X.ue_yaw(1, 0) == 0.0 and X.ue_yaw(0, 1) == -90.0 and abs(abs(X.ue_yaw(-1, 0)) - 180.0) < 1e-9
    assert X.ue_cm(1.5, -2.0, 3.0) == [150.0, 200.0, 300.0]
    box = X.Box("r", 0, 10, 0, 4, 0, 3)
    assert X.prior_interior(X.Exit("r", 0, 1, 2, 0, None, 1), box) == (1.0, 0.0)
    assert X.prior_interior(X.Exit("r", 0, 9, 2, 0, None, 1), box) == (-1.0, 0.0)
    e = X.Exit("MainMap", 0, 0, 0, 0, None, 1)
    assert X.prior_outdoor(e, [(-3.0, 0.0, 200.0)]) == (1.0, 0.0)               # building to the west -> leave east
    assert X.prior_outdoor(e, [(0.0, 3.0, 200.0), (50.0, 50.0, 1e6)]) == (0.0, -1.0)  # far one ignored
    assert X.prior_outdoor(e, []) is None
    assert X.open_side(e, [(5.0, 0.0), (8.0, 1.0), (0.0, 5.0)]) in {(-1.0, 0.0), (0.0, -1.0)}


def test_build_links_format_and_geometry():
    handmade = [{"id": "hm_a", "partner": "hm_b",
                 "trigger": {"center_cm": [-62860, 32950, 100], "half_extent_cm": [150, 50, 250]},
                 "exit": {"location_cm": [0, 0, 0], "yaw": 0}},
                {"id": "hm_b", "partner": "hm_a",
                 "trigger": {"center_cm": [20760, 7330, 900], "half_extent_cm": [50, 150, 250]},
                 "exit": {"location_cm": [0, 0, 0], "yaw": 0}}]
    ex_text = EXIT_TEXT + "MainMap,\t10,\t200.0,\t-60.5,\t6.0\t#->Library\r\n"   # unused extra row, unpaired
    manifest = {"scenes": [
        {"name": "isc_lib", "kind": "interior", "bounds_game_positions": [[-799, 181, 90], [-757, 224, 101]]},
        {"name": "ischool", "kind": "interior", "bounds_game_positions": [[-676, -335, -9], [-580, -262, 17]]},
        {"name": "isc_chem", "kind": "interior", "bounds_game_positions": [[-600, 320, 34], [-595, 331, 37]]},
        {"name": "isc_staf", "kind": "interior", "bounds_game_positions": [[-656, 220, -0.4], [-642, 233, 4]]},
        {"name": "isc_dorm", "kind": "interior", "bounds_game_positions": [[-525, 296, 30], [-480, 330, 36]]},
        {"name": "isalon", "kind": "interior", "bounds_game_positions": [[-770, 10, 3], [-763, 26, 7]]},
        {"name": "ext", "kind": "exterior", "bounds_game_positions": [[90, -100, 0], [340, 0, 20]]}]}
    doc, rep, sides = B.build(ex_text, BBOX_TEXT, manifest, handmade)
    assert doc["_format"] == "bully-remake.door-links/1"
    ids = {l["id"] for l in doc["links"]}
    assert len(ids) == len(doc["links"]) and len(doc["links"]) % 2 == 0
    by = {l["id"]: l for l in doc["links"]}
    for l in doc["links"]:
        assert by[l["partner"]]["partner"] == l["id"]                           # both sides, pointing back
        assert l["trigger"]["half_extent_cm"][2] == 250.0
        assert sorted(l["trigger"]["half_extent_cm"][:2]) == [50.0, 150.0]
        tc, ex = l["trigger"]["center_cm"], l["exit"]["location_cm"]
        assert math.isclose(math.hypot(tc[0] - ex[0], tc[1] - ex[1]), 250.0, abs_tol=1.0)   # 2.5 m step
        assert -180.0 <= l["exit"]["yaw"] <= 180.0
    lib_in = by["g_library__mainmap"]
    assert lib_in["trigger"]["center_cm"][:2] == [-78900.0, -20300.0]
    assert lib_in["exit"]["yaw"] == 0.0                                          # box centre is east of the door
    assert lib_in["trigger"]["half_extent_cm"] == [50.0, 150.0, 250.0]
    # hallway door at (-628,-330) is within 5 m of hand-made hm_a: that pair is skipped, not duplicated
    assert [s["exits"] for s in rep["skipped_duplicates_of_handmade"]] == [["MainMap#6", "School_Hallways#0"]]
    assert "g_school_hallways__mainmap" not in by and "g_mainmap__school_hallways" not in by
    assert rep["exit_checks"] and rep["geometry"] is False
    rej = {tuple(r["pair"]) for r in rep["rejected_pairs"]}
    assert ("MainMap#5", "Fire_Works#0") in rej                                  # outdoor point in the interior area
    text = B.format_report(rep)
    assert "unpaired exits" in text and "rejected pairs" in text
    json.dumps(doc), json.dumps(rep)


def test_handmade_duplicate_detection():
    link = {"trigger": {"center_cm": [21070, 7330, 1000]}}
    hm = [{"id": "x", "trigger": {"center_cm": [20760, 7336, 1000]}}]
    assert X.is_duplicate_of(link, hm) == "x"
    assert X.is_duplicate_of({"trigger": {"center_cm": [30000, 7336, 1000]}}, hm) is None


def test_trigger_conflicts():
    a = {"id": "a", "trigger": {"center_cm": [0, 0, 0], "half_extent_cm": [50, 150, 250]}, "exit": {"location_cm": [250, 0, 0]}}
    b = {"id": "b", "trigger": {"center_cm": [30, 100, 0], "half_extent_cm": [50, 150, 250]}, "exit": {"location_cm": [30, 600, 0]}}
    c = {"id": "c", "trigger": {"center_cm": [5000, 0, 0], "half_extent_cm": [50, 150, 250]}, "exit": {"location_cm": [0, 0, 0]}}
    ov, arr = X.trigger_conflicts([a, b, c])
    assert ("a", "b") in ov and ("a", "c") not in ov
    assert ("c", "a") in arr                                                     # c's arrival point sits in a's trigger


def _quad(x0, x1, y0, y1, z):
    return [[(x0, y0, z), (x1, y0, z), (x1, y1, z)], [(x0, y0, z), (x1, y1, z), (x0, y1, z)]]


def _wall_x(x, y0, y1, z0, z1):
    return [[(x, y0, z0), (x, y1, z0), (x, y1, z1)], [(x, y0, z0), (x, y1, z1), (x, y0, z1)]]


def _room_tris():
    """Floor x in [0, 12], y in [-3, 3] at z=0 (a corridor), walls closing the west end at x=0 (a door wall with a
    hole is not needed: the doorway point sits at x=0.2 against the closed end) and the long sides at y=+-3."""
    t = _quad(0, 12, -3, 3, 0.0) + _wall_x(0.0, -3, 3, 0, 3) + []
    for y in (-3.0, 3.0):  # long side walls (normal along y)
        t += [[(0, y, 0), (12, y, 0), (12, y, 3)], [(0, y, 0), (12, y, 3), (0, y, 3)]]
    return np.array(t, dtype=float)


def test_geometry_choose_corridor():
    loc = Local(_room_tris(), 0.0)
    assert loc.floor_z(5, 0, 0.0) == 0.0 and loc.floor_z(20, 0, 0.0) is None
    assert loc.wall_between((1, 0), (-0.5, 0), 0.0)                             # the west end wall
    assert not loc.wall_between((1, 0), (5, 0), 0.0)
    run_e, _ = loc.run((0.6, 0.0), 0.0, (1.0, 0.0))
    run_w, _ = loc.run((0.6, 0.0), 0.0, (-1.0, 0.0))
    assert run_e == 6.0 and run_w == 0.5                                          # stops at the end wall
    res = loc.choose((0.6, 0.0), 0.0, prior=(-1.0, 0.0), want_open=False)       # prior points the wrong way
    assert res["name"] == "E" and res["ok"] and res["back_blocked"]             # geometry beats the prior
    assert abs(res["floor_z"]) < 1e-9


def test_geometry_thin_post_does_not_block():
    t = _room_tris().tolist()
    t += _wall_x(3.0, -0.05, 0.05, 0.0, 3.0)                                    # a 10 cm post across the centre line
    loc = Local(np.array(t, dtype=float), 0.0)
    assert not loc.wall_between((1, 0), (5, 0), 0.0)


def test_geometry_no_floor_means_no_run():
    t = np.array(_quad(0, 2, -3, 3, 0.0), dtype=float)
    loc = Local(t, 0.0)
    assert loc.run((0.5, 0.0), 0.0, (1.0, 0.0))[0] == 1.5                       # floor ends at x=2


def test_cli_registers_exits():
    import argparse
    assert callable(cli.cmd_exits)
    src = open(cli.__file__).read()
    assert 'sub.add_parser("exits"' in src


def _load_lib():
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ue", "door_links_lib.py")
    spec = importlib.util.spec_from_file_location("door_links_lib", here)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_door_links_lib(tmp_path):
    lib = _load_lib()
    assert lib.resolve_files("/h", {}) == ["/h/door_links.json"]
    assert lib.resolve_files("/h", {"BR_DOOR_LINKS": "/a.json" + os.pathsep + "/b.json"}) == ["/a.json", "/b.json"]

    def link(i, p):
        return {"id": i, "partner": p, "trigger": {"center_cm": [0, 0, 0], "half_extent_cm": [1, 1, 1]},
                "exit": {"location_cm": [0, 0, 0], "yaw": 0}}
    a, b = tmp_path / "a.json", tmp_path / "b.json"
    a.write_text(json.dumps({"_format": lib.FORMAT, "links": [link("x", "y"), link("y", "x")]}))
    b.write_text(json.dumps({"_format": lib.FORMAT, "links": [link("x", "z"), link("p", "q"), link("q", "p")]}))
    links, notes = lib.load_links([str(a), str(b)])
    assert [d["id"] for d in links] == ["x", "y", "p", "q"]                      # first x wins
    assert links[0]["partner"] == "y" and any("duplicate id x" in n for n in notes)
    assert lib.problems(links) == []
    links[2]["partner"] = "missing"
    assert any("not in the list" in p for p in lib.problems(links))
