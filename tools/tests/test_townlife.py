# OWNER: builder-A (importers)
"""townlife geometry helpers, on synthetic fixtures only (no game data)."""
import math

import pytest

from tools.importers import townlife as T

# ---------------------------------------------------------------------------
# GRIDS.DAT
# ---------------------------------------------------------------------------

GRIDS = (
    "NCARGRIDNODES 2\r\nNPEDGRIDNODES 3\r\nNCARGRIDLINKS 1\r\nNPEDGRIDLINKS 2\r\n"
    "PEDGRID\r\n\tBEGIN\r\n\t\tNSIDEWALKNODES 3\r\n"
    "\t\tNODEID 0\r\n\t\tNODE\r\n\t\t\tBEGIN\r\n\t\t\t\tPosition -10.5, 20.25, 3.0\r\n\t\t\t\tNLINKS 1\r\n"
    "\t\t\t\tLINKID 0\r\n\t\t\t\tWIDTH 12\r\n\t\t\t\tONDEADEND FALSE\r\n\t\t\t\tSWITCHEDOFF FALSE\r\n"
    "\t\t\t\tBETWEENLEVELS FALSE\r\n\t\t\t\tWATERNODE FALSE\r\n\t\t\t\tUNDERBRIDGE FALSE\r\n\t\t\t\tSPEED 0\r\n"
    "\t\t\t\tDENSITY 2\r\n\t\t\tEND\r\n"
    "\t\tNODEID 1\r\n\t\tNODE\r\n\t\t\tBEGIN\r\n\t\t\t\tPosition -0.5, 20.25, 3.0\r\n\t\t\t\tNLINKS 2\r\n"
    "\t\t\t\tLINKID 0\r\n\t\t\t\tLINKID 1\r\n\t\t\t\tWIDTH 0\r\n\t\t\t\tONDEADEND TRUE\r\n\t\t\t\tSWITCHEDOFF FALSE\r\n"
    "\t\t\t\tBETWEENLEVELS FALSE\r\n\t\t\t\tWATERNODE TRUE\r\n\t\t\t\tUNDERBRIDGE FALSE\r\n\t\t\t\tSPEED 0\r\n"
    "\t\t\t\tDENSITY 1\r\n\t\t\tEND\r\n"
    "\t\tNODEID 2\r\n\t\tNODE\r\n\t\t\tBEGIN\r\n\t\t\t\tPosition 9.5, 20.25, 3.5\r\n\t\t\t\tNLINKS 1\r\n"
    "\t\t\t\tLINKID 1\r\n\t\t\t\tWIDTH 5\r\n\t\t\t\tONDEADEND FALSE\r\n\t\t\t\tSWITCHEDOFF FALSE\r\n"
    "\t\t\t\tBETWEENLEVELS FALSE\r\n\t\t\t\tWATERNODE FALSE\r\n\t\t\t\tUNDERBRIDGE FALSE\r\n\t\t\t\tSPEED 0\r\n"
    "\t\t\t\tDENSITY 0\r\n\t\t\tEND\r\n"
    "\t\tNSIDEWALKLINKS 2\r\n"
    "\t\tLINKID 0\r\n\t\tLINK\r\n\t\t\tBEGIN\r\n\t\t\t\tDist 10.0\r\n\t\t\t\tDEST 1\r\n\t\t\t\tSRC 0\r\n\t\t\tEND\r\n"
    "\t\tLINKID 1\r\n\t\tLINK\r\n\t\t\tBEGIN\r\n\t\t\t\tDist 10.0\r\n\t\t\t\tDEST 2\r\n\t\t\t\tSRC 1\r\n\t\t\tEND\r\n"
    "\tEND\r\n"
    "CARGRID\r\n\tBEGIN\r\n\t\tNROADNODES 2\r\n"
    "\t\tNODEID 0\r\n\t\tNODE\r\n\t\t\tBEGIN\r\n\t\t\t\tPosition 0.0, 0.0, 1.0\r\n\t\t\t\tNLINKS 1\r\n"
    "\t\t\t\tLINKID 0\r\n\t\t\t\tWIDTH 0\r\n\t\t\t\tONDEADEND FALSE\r\n\t\t\t\tSWITCHEDOFF FALSE\r\n"
    "\t\t\t\tBETWEENLEVELS FALSE\r\n\t\t\t\tWATERNODE FALSE\r\n\t\t\t\tUNDERBRIDGE FALSE\r\n\t\t\t\tSPEED 1\r\n"
    "\t\t\t\tDENSITY 1\r\n\t\t\tEND\r\n"
    "\t\tNODEID 1\r\n\t\tNODE\r\n\t\t\tBEGIN\r\n\t\t\t\tPosition 40.0, 0.0, 1.0\r\n\t\t\t\tNLINKS 1\r\n"
    "\t\t\t\tLINKID 0\r\n\t\t\t\tWIDTH 0\r\n\t\t\t\tONDEADEND FALSE\r\n\t\t\t\tSWITCHEDOFF FALSE\r\n"
    "\t\t\t\tBETWEENLEVELS FALSE\r\n\t\t\t\tWATERNODE FALSE\r\n\t\t\t\tUNDERBRIDGE FALSE\r\n\t\t\t\tSPEED 1\r\n"
    "\t\t\t\tDENSITY 1\r\n\t\t\tEND\r\n"
    "\t\tNCARLINKS 1\r\n"
    "\t\tLINKID 0\r\n\t\tLINK\r\n\t\t\tBEGIN\r\n\t\t\t\tDist 40.0\r\n\t\t\t\tDEST 1\r\n\t\t\t\tSRC 0\r\n"
    "\t\t\t\tLANESTO 1\r\n\t\t\t\tLANESFRO 1\r\n\t\t\t\tWIDTH 0\r\n\t\t\tEND\r\n"
    "\tEND\r\n"
)


def test_parse_grids_nodes_links_and_flags():
    g = T.parse_grids(GRIDS)
    assert sorted(g.ped.nodes) == [0, 1, 2] and sorted(g.car.nodes) == [0, 1]
    assert g.ped.nodes[0]["pos"] == (-10.5, 20.25, 3.0)
    assert g.ped.nodes[1]["flags"]["WATERNODE"] and g.ped.nodes[1]["flags"]["ONDEADEND"]
    assert not g.ped.nodes[0]["flags"]["WATERNODE"]
    assert g.ped.nodes[0]["width"] == 12
    assert [(l["src"], l["dest"]) for l in g.ped.links] == [(0, 1), (1, 2)]
    assert g.car.links[0]["lanes_to"] == 1 and g.car.links[0]["lanes_fro"] == 1
    adj = g.ped.adjacency()
    assert adj[0] == {1: 10.0} and adj[1][0] == 10.0 and adj[1][2] == pytest.approx(math.hypot(10.0, 0.5))   # 3D lengths


def test_parse_grids_without_car_section():
    g = T.parse_grids(GRIDS[:GRIDS.index("\r\nCARGRID")])
    assert len(g.ped.nodes) == 3 and not g.car.nodes


# ---------------------------------------------------------------------------
# 2D geometry
# ---------------------------------------------------------------------------

def test_seg_point_dist_endpoints_and_interior():
    a, b = (0, 0), (10, 0)
    assert T.seg_point_dist((5, 3), a, b) == pytest.approx(3.0)
    assert T.seg_point_dist((-4, 3), a, b) == pytest.approx(5.0)   # beyond a: distance to a
    assert T.seg_point_dist((13, 4), a, b) == pytest.approx(5.0)
    assert T.seg_point_dist((1, 1), a, a) == pytest.approx(math.sqrt(2))   # degenerate segment


def test_segments_intersect_cases():
    assert T.segments_intersect((0, 0), (4, 4), (0, 4), (4, 0))
    assert not T.segments_intersect((0, 0), (1, 0), (2, 0), (3, 0))      # collinear, apart
    assert T.segments_intersect((0, 0), (2, 0), (2, 0), (3, 5))          # touching end point
    assert T.segments_intersect((0, 0), (4, 0), (1, 0), (2, 0))          # collinear overlap
    assert not T.segments_intersect((0, 0), (4, 0), (0, 1), (4, 1))      # parallel


def test_seg_seg_dist():
    assert T.seg_seg_dist((0, 0), (4, 0), (0, 3), (4, 3)) == pytest.approx(3.0)
    assert T.seg_seg_dist((0, 0), (4, 4), (0, 4), (4, 0)) == 0.0
    assert T.seg_seg_dist((0, 0), (1, 0), (4, 0), (4, 5)) == pytest.approx(3.0)


def test_point_in_polygon_concave():
    poly = [(0, 0), (4, 0), (4, 4), (2, 1), (0, 4)]   # a notch from the top
    assert T.point_in_polygon((1, 1), poly)
    assert not T.point_in_polygon((2, 3), poly)
    assert not T.point_in_polygon((5, 1), poly)


def test_rect_corners_orientation_and_size():
    c = T.rect_corners(10, 5, 0.0, 5.0, 2.4)
    xs, ys = [p[0] for p in c], [p[1] for p in c]
    assert (min(xs), max(xs)) == pytest.approx((7.5, 12.5)) and (min(ys), max(ys)) == pytest.approx((3.8, 6.2))
    c90 = T.rect_corners(0, 0, math.pi / 2, 5.0, 2.4)      # heading +y: long axis along y
    assert max(p[1] for p in c90) == pytest.approx(2.5) and max(p[0] for p in c90) == pytest.approx(1.2)
    # CCW winding (positive shoelace area)
    area = sum(c[i][0] * c[(i + 1) % 4][1] - c[(i + 1) % 4][0] * c[i][1] for i in range(4)) / 2
    assert area == pytest.approx(5.0 * 2.4)


def test_segment_rect_distance():
    rect = T.rect_corners(0, 0, 0.0, 4.0, 2.0)            # x -2..2, y -1..1
    assert T.segment_rect_distance((-5, 3), (5, 3), rect) == pytest.approx(2.0)     # passes above
    assert T.segment_rect_distance((-5, 0), (5, 0), rect) == 0.0                    # through it
    assert T.segment_rect_distance((0.5, 0), (0.7, 0.2), rect) == 0.0               # inside it
    assert T.segment_rect_distance((4, -3), (4, 3), rect) == pytest.approx(2.0)     # beside it
    assert T.segment_rect_distance((3, 2), (5, 2), rect) == pytest.approx(math.sqrt(2))   # off a corner


def test_sample_segment_spacing_and_endpoints():
    pts = T.sample_segment((0, 0), (10, 0), 0.5)
    assert pts[0] == (0, 0) and pts[-1] == (10, 0) and len(pts) == 21
    assert T.sample_segment((1, 1), (1, 1))[0] == (1, 1)    # zero-length segment does not divide by zero


def test_polyline_length_and_self_intersection():
    sq = [(0, 0), (10, 0), (10, 10), (0, 10)]
    assert T.polyline_length(sq, closed=True) == pytest.approx(40.0)
    assert T.polyline_length(sq) == pytest.approx(30.0)
    assert not T.polyline_self_intersects(sq)
    bow = [(0, 0), (10, 10), (10, 0), (0, 10)]
    assert T.polyline_self_intersects(bow)


def test_heading_yaw_deg_ue_matches_coords_conversion():
    from tools.importers import coords
    for dx, dy in [(1, 0), (0, 1), (-1, 0), (0, -1), (1, 1), (-3, 2)]:
        ue = coords.game_to_ue_position((dx, dy, 0))
        want = math.degrees(math.atan2(ue[1], ue[0]))
        got = T.heading_yaw_deg_ue(dx, dy)
        assert math.isclose(math.remainder(got - want, 360.0), 0.0, abs_tol=1e-9)
    assert T.heading_yaw_deg_ue(1, 0) == 0.0 and T.heading_yaw_deg_ue(0, 1) == -90.0
    assert T.heading_yaw_deg_ue(-1, 0) == 180.0   # (-180, 180], never -180


# ---------------------------------------------------------------------------
# Spatial index, keep-out, walkable network
# ---------------------------------------------------------------------------

def test_seg_index_queries():
    idx = T.SegIndex([((0, 0), (20, 0)), ((100, 100), (120, 100))])
    assert idx.point_within((10, 1.5), 2.0) and not idx.point_within((10, 3.0), 2.0)
    assert idx.point_within((110, 99), 1.5) and not idx.point_within((50, 50), 5.0)
    assert idx.segment_crosses((10, -3), (10, 3)) and not idx.segment_crosses((10, 1), (10, 3))
    assert idx.rect_within(T.rect_corners(10, 3, 0.0, 5.0, 2.4), 2.0)       # rect edge at y=1.8: 1.8 < 2.0
    assert not idx.rect_within(T.rect_corners(10, 5, 0.0, 5.0, 2.4), 2.0)   # rect edge at y=3.8
    # a segment longer than one grid cell is found from every cell it crosses
    idx2 = T.SegIndex([((0, 0), (200, 0))], cell=8.0)
    assert idx2.point_within((155, 1), 2.0)


def test_nogo_polygons_and_vertical_walls():
    flat = [((0, 0, 0), (10, 0, 0), (0, 10, 0))]
    wall = [((20, 0, -5), (30, 0, -5), (30, 0, 5)), ((20, 0, -5), (30, 0, 5), (20, 0, 5))]   # vertical, y = 0
    ng = T.Nogo.from_triangles(flat + wall)
    assert len(ng.polys) == 1 and len(ng.walls) == 2 and ng
    assert not T.Nogo()
    assert ng.hits_point((2, 2)) and not ng.hits_point((8, 8))
    assert ng.hits_point((25, 0.2), margin=0.5) and not ng.hits_point((25, 2.0), margin=0.5)
    assert ng.hits_segment((-5, 2), (5, 2))                  # enters the triangle
    assert ng.hits_segment((25, -3), (25, 3))                # crosses the wall
    assert not ng.hits_segment((25, 1), (25, 3))
    assert ng.hits_rect(T.rect_corners(25, 0, 0.0, 5.0, 2.4))
    assert not ng.hits_rect(T.rect_corners(25, 8, 0.0, 5.0, 2.4))
    # a polygon wholly inside the rectangle still counts
    tiny = T.Nogo.from_triangles([((0, 0, 0), (1, 0, 0), (0, 1, 0))])
    assert tiny.hits_rect(T.rect_corners(0.3, 0.3, 0.0, 6.0, 6.0))


def test_walkable_net_segment_and_rect_checks():
    net = T.WalkableNet([((0, 0), (40, 0))], radius=1.75)
    assert net.point_ok((20, 1.7)) and not net.point_ok((20, 1.8))
    assert net.segment_ok((2, 0), (38, 1.0))
    assert not net.segment_ok((2, 0), (38, 5.0))             # drifts off the corridor
    assert net.rect_touches(T.rect_corners(20, 2.9, 0.0, 5.0, 2.4))       # rect edge at y=1.7 < 1.75
    assert not net.rect_touches(T.rect_corners(20, 3.4, 0.0, 5.0, 2.4))   # edge at y=2.2, beyond radius
    ng = T.Nogo.from_triangles([((10, -1, 0), (12, -1, 0), (11, 1, 0))])
    blocked = T.WalkableNet([((0, 0), (40, 0))], radius=1.75, nogo=ng)
    assert not blocked.segment_ok((2, 0), (38, 0)) and blocked.segment_ok((14, 0), (38, 0))
    assert not blocked.point_ok((11, 0))


# ---------------------------------------------------------------------------
# Loops
# ---------------------------------------------------------------------------

def square_graph(side=20.0, step=10.0):
    """A closed street block: nodes every `step` m round a side x side square, plus a dead-end spur."""
    pos, ring = {}, []
    n = int(side / step)
    pts = ([(i * step, 0.0) for i in range(n)] + [(side, i * step) for i in range(n)]
           + [(side - i * step, side) for i in range(n)] + [(0.0, side - i * step) for i in range(n)])
    for i, (x, y) in enumerate(pts):
        pos[i] = (x, y, 5.0)
        ring.append(i)
    adj = {i: {} for i in ring}
    for i in ring:
        j = ring[(i + 1) % len(ring)]
        d = math.dist(pos[i][:2], pos[j][:2])
        adj[i][j] = d
        adj[j][i] = d
    return adj, pos


def test_find_cycles_square_block_and_length_filter():
    adj, pos = square_graph(20.0, 10.0)             # 80 m perimeter
    cyc = T.find_cycles(adj, pos)
    assert len(cyc) == 1 and sorted(cyc[0]) == sorted(adj)
    assert T.find_cycles(adj, pos, min_len=90.0) == []                  # too short
    assert T.find_cycles(adj, pos, max_len=70.0) == []                  # too long
    tree = {0: {1: 30.0}, 1: {0: 30.0, 2: 30.0}, 2: {1: 30.0}}
    assert T.find_cycles(tree, {}) == []                                 # a path has no cycles


def test_find_cycles_two_adjacent_blocks_gives_each_minimal_cycle_once():
    # 2x1 grid of 15 m cells: two 60 m cells; the 90 m outer ring also qualifies
    pos = {i + 3 * j: (15.0 * i, 15.0 * j, 0.0) for i in range(3) for j in range(2)}
    adj = {k: {} for k in pos}
    for a in pos:
        for b in pos:
            if a < b and math.isclose(math.dist(pos[a][:2], pos[b][:2]), 15.0):
                adj[a][b] = adj[b][a] = 15.0
    cyc = T.find_cycles(adj, pos)
    keys = {frozenset(c) for c in cyc}
    assert len(cyc) == len(keys)
    assert frozenset({0, 1, 4, 3}) in keys and frozenset({1, 2, 5, 4}) in keys


def test_simplify_loop_drops_collinear_first_and_respects_ok():
    sq = [(0, 0, 0), (5, 0, 0), (10, 0, 0), (10, 5, 0), (10, 10, 0), (5, 10, 0), (0, 10, 0), (0, 5, 0)]
    out = T.simplify_loop(sq, 4)
    assert sorted(out) == [(0, 0, 0), (0, 10, 0), (10, 0, 0), (10, 10, 0)]
    assert T.simplify_loop(sq, 8) == sq
    stuck = T.simplify_loop(sq, 4, ok=lambda a, b: False)               # every chord vetoed: nothing dropped
    assert len(stuck) == 8


def test_subdivide_loop_pads_to_minimum_with_midpoints():
    tri = [(0, 0, 0), (12, 0, 4), (0, 12, 0)]
    out = T.subdivide_loop(tri, 4)
    assert len(out) == 4
    assert (6.0, 6.0, 2.0) in out                                        # midpoint of the longest chord (12,0,4)-(0,12,0)


def test_simplify_chain_keeps_corner_and_honours_ok():
    chain = [(0, 0, 0), (5, 0, 0), (10, 0, 0), (10, 5, 0), (10, 10, 0)]
    assert T.simplify_chain(chain, 0.1) == [(0, 0, 0), (10, 0, 0), (10, 10, 0)]
    assert T.simplify_chain(chain, 0.1, ok=lambda a, b: False) == chain  # cannot merge across vetoed chords
    assert T.simplify_chain([(0, 0, 0)], 0.1) == [(0, 0, 0)]


def test_out_and_back_loop_is_a_closed_polygon_inside_the_corridor():
    net = T.WalkableNet([((0, 0), (40, 0))], radius=1.75)
    chain = [(0, 0, 1.0), (10, 0, 1.0), (20, 0, 1.0), (30, 0, 1.0)]
    loop = T.out_and_back_loop(chain, 0.9, net)
    assert loop is not None and len(loop) == 4                           # straight chain collapses to 2 + 2 points
    ys = sorted({round(p[1], 3) for p in loop})
    assert ys == [-0.9, 0.9]                                             # out on one side, back on the other
    assert not T.polyline_self_intersects([p[:2] for p in loop])
    assert all(p[2] == 1.0 for p in loop)
    assert T.polyline_length([p[:2] for p in loop], closed=True) == pytest.approx(2 * 30 + 2 * 1.8)
    narrow = T.WalkableNet([((0, 0), (40, 0))], radius=0.5)             # too narrow for +-0.9: falls back to centreline
    loop2 = T.out_and_back_loop(chain, 0.9, narrow)
    assert loop2 is not None and all(abs(p[1]) <= 0.45 + 1e-9 for p in loop2)


def _block_net(adj, pos, radius=1.75):
    segs = [(pos[a][:2], pos[b][:2]) for a in adj for b in adj[a] if a < b]
    return T.WalkableNet(segs, radius)


def test_candidate_and_select_loops_respect_the_constraints():
    adj, pos = square_graph(30.0, 10.0)          # 120 m perimeter: on the limit, still allowed
    adj2, pos2 = square_graph(20.0, 10.0)
    for a, p in ((adj, pos), (adj2, pos2)):
        net = _block_net(a, p)
        cands = T.candidate_loops(a, p, net)
        assert cands
        for c in cands:
            wps = c["waypoints"]
            assert T.LOOP_MIN_WP <= len(wps) <= T.LOOP_MAX_WP
            assert T.LOOP_MIN_M <= c["length"] <= T.LOOP_MAX_M
            assert all(net.segment_ok(wps[i], wps[(i + 1) % len(wps)]) for i in range(len(wps)))
        chosen = T.select_loops(cands)
        assert 1 <= len(chosen) <= T.LOOPS_MAX
        assert any(c["kind"] == "cycle" for c in chosen)                 # a true cycle beats out-and-back


def test_select_loops_spreads_and_caps():
    # 12 candidates on a line, 15 m apart: the strict pass keeps >= 20 m between loops and never exceeds the cap
    cands = []
    for i in range(12):
        x = 15.0 * i
        wps = [(x, 0, 0), (x + 3, 0, 0), (x + 3, 3, 0), (x, 3, 0)]
        cands.append({"kind": "ring", "waypoints": wps, "nodes": frozenset({i}), "length": 60.0})
    chosen = T.select_loops(cands)
    assert T.LOOPS_MIN <= len(chosen) <= T.LOOPS_MAX
    cents = sorted(T._centroid(c["waypoints"])[0] for c in chosen)
    assert min(b - a for a, b in zip(cents, cents[1:])) >= 20.0
    assert T.select_loops([]) == []


def test_select_loops_rejects_heavy_node_overlap():
    base = {"kind": "cycle", "waypoints": [(0, 0, 0), (3, 0, 0), (3, 3, 0), (0, 3, 0)], "length": 60.0}
    a = dict(base, nodes=frozenset(range(10)))
    b = dict(base, nodes=frozenset(range(10)), waypoints=[(100, 0, 0), (103, 0, 0), (103, 3, 0), (100, 3, 0)])
    c = dict(base, nodes=frozenset(range(100, 110)), waypoints=[(200, 0, 0), (203, 0, 0), (203, 3, 0), (200, 3, 0)])
    chosen = T.select_loops([a, b, c], count=3, min_count=3)
    # strict pass takes a and c (b shares all nodes with a); the relaxed passes may then add b
    assert a in chosen and c in chosen


# ---------------------------------------------------------------------------
# Road raster, kerb, parking
# ---------------------------------------------------------------------------

np = pytest.importorskip("numpy")


def road_band(half_width=5.0, x_range=(-40, 120), pad=15.0, cell=0.25):
    """Road along +x with |y| <= half_width, grass beyond: RoadMask over x_range x [-pad.., +pad]."""
    x0, y0 = x_range[0], -half_width - pad
    w = int((x_range[1] - x_range[0]) / cell)
    h = int((2 * (half_width + pad)) / cell)
    data = np.zeros((h, w), bool)
    r0 = int((-half_width - y0) / cell)
    r1 = int((half_width - y0) / cell)
    data[r0:r1, :] = True
    z = np.full((h, w), 2.0, np.float32)
    return T.RoadMask(data, x0, y0, cell, z)


def test_road_mask_lookup_and_height():
    m = road_band()
    assert m.at(0, 0) and m.at(0, 4.9) and not m.at(0, 5.3) and not m.at(500, 0) and not m.at(0, -500)
    assert m.z_at(0, 0) == 2.0 and m.z_at(0, 9.0, default=-1.0) == -1.0


def test_kerb_distance_from_centre_line_and_off_road():
    m = road_band(5.0)
    assert T.kerb_distance(m, (0, 0), 0, 1) == pytest.approx(5.0, abs=0.3)
    assert T.kerb_distance(m, (0, 3.0), 0, 1) == pytest.approx(2.0, abs=0.3)
    assert T.kerb_distance(m, (0, 3.0), 0, -1) == pytest.approx(8.0, abs=0.3)
    assert T.kerb_distance(m, (0, 8.0), 0, 1) is None                   # start point off the road
    wide = road_band(40.0, pad=5.0)
    assert T.kerb_distance(wide, (0, 0), 0, 1, max_d=14.0) is None      # plaza: no kerb within reach


def test_kerb_distance_ignores_a_gap_narrower_than_the_threshold():
    m = road_band(5.0)
    # carve a 0.5 m ditch across the road at y in [2.0, 2.5]: a lane marking, not a kerb
    r0 = int((2.0 - m.y0) / m.cell)
    m.data[r0:r0 + 2, :] = False
    assert T.kerb_distance(m, (0, 0), 0, 1) == pytest.approx(5.0, abs=0.3)


def test_rect_on_mask():
    m = road_band(5.0)
    assert T.rect_on_mask(m, T.rect_corners(10, 0, 0.0, 5.0, 2.4))
    assert not T.rect_on_mask(m, T.rect_corners(10, 4.2, 0.0, 5.0, 2.4))   # sticks out over the kerb


def synthetic_street():
    """A 100 m street: road graph along y=0 (with a side-street junction node at x=70), sidewalks at y=+-7."""
    cn = {0: {"pos": (0.0, 0.0, 1.0), "width": 0, "flags": {}},
          1: {"pos": (70.0, 0.0, 1.0), "width": 0, "flags": {}},
          2: {"pos": (100.0, 0.0, 1.0), "width": 0, "flags": {}},
          3: {"pos": (70.0, 30.0, 1.0), "width": 0, "flags": {}},
          4: {"pos": (70.0, -30.0, 1.0), "width": 0, "flags": {}}}
    links = [{"id": i, "src": a, "dest": b, "dist": 0.0, "lanes_to": 1, "lanes_fro": 1}
             for i, (a, b) in enumerate([(0, 1), (1, 2), (1, 3), (1, 4)])]
    car = T.Grid(cn, links)
    net = T.WalkableNet([((-40, 7), (120, 7)), ((-40, -7), (120, -7))], radius=1.75)
    return car, net


def test_parking_candidates_kerbside_geometry():
    car, net = synthetic_street()
    mask = road_band(5.0)
    cands = T.parking_candidates(car, [0, 1, 2], mask, net)
    assert cands
    for c in cands:
        x, y, z = c["pos"]
        assert abs(abs(y) - (5.0 - T.KERB_INSET)) < 0.35                 # centre ~1.6 m in from the kerb
        hx, hy = c["heading"]
        assert abs(hy) < 1e-9 and abs(abs(hx) - 1.0) < 1e-9              # parallel to the kerb
        # kerb on the car's right: heading +x -> south kerb (y<0); heading -x -> north kerb (y>0)
        assert (hx > 0) == (y < 0)
        assert z == pytest.approx(1.0)
        assert min(math.dist((x, y), (70.0, 0.0)), 1e9) >= T.JUNCTION_CLEARANCE - 1e-9   # clear of the junction
        corners = T.spot_corners(x, y, hx, hy)
        assert T.rect_on_mask(mask, corners) and not net.rect_touches(corners)
    assert {round(c["pos"][1]) > 0 for c in cands} == {True, False}      # both kerbs used


def test_parking_skips_footprints_over_sidewalk_or_nogo():
    car, _ = synthetic_street()
    mask = road_band(5.0)
    on_kerb = T.WalkableNet([((-40, 3.4), (120, 3.4)), ((-40, -3.4), (120, -3.4))], radius=1.75)
    assert T.parking_candidates(car, [0, 1, 2], mask, on_kerb) == []      # corridor reaches into the parking lane
    ng = T.Nogo.from_triangles([((-50, -8, 0), (150, -8, 0), (150, 8, 0)), ((-50, -8, 0), (150, 8, 0), (-50, 8, 0))])
    far = T.WalkableNet([((-40, 7), (120, 7))], radius=1.75, nogo=ng)
    assert T.parking_candidates(car, [0, 1, 2], mask, far) == []


def test_parking_requires_straight_kerb():
    car, net = synthetic_street()
    mask = road_band(5.0)
    # replace the north kerb over x in [20, 40] with a bulge: no spots there on that side
    r0 = int((5.0 - mask.y0) / mask.cell)
    c0, c1 = int((20 - mask.x0) / mask.cell), int((40 - mask.x0) / mask.cell)
    mask.data[r0:r0 + 12, c0:c1] = True                               # road widens by 3 m over 20 m
    cands = T.parking_candidates(car, [0, 1, 2], mask, net)
    assert not [c for c in cands if c["pos"][1] > 0 and 22 < c["pos"][0] < 38 and c["heading"][0] < 0
                and abs(c["pos"][1] - 3.4) < 0.4]


def test_select_parking_spacing_cap_valid_and_existing():
    cands = [{"pos": (float(x), 0.0, 0.0), "heading": (1.0, 0.0), "kerb": 5.0} for x in range(0, 100, 2)]
    chosen = T.select_parking(cands)
    assert len(chosen) == T.PARK_MAX
    for i, a in enumerate(chosen):
        for b in chosen[i + 1:]:
            assert math.dist(a["pos"][:2], b["pos"][:2]) >= T.PARK_SPACING
    only_even_tens = T.select_parking(cands, valid=lambda c: c["pos"][0] % 10 == 0)
    assert only_even_tens and all(c["pos"][0] % 10 == 0 for c in only_even_tens)
    taken = [{"pos": (50.0, 0.0, 0.0)}]
    chosen2 = T.select_parking(cands, existing=taken)
    assert all(math.dist(c["pos"][:2], (50.0, 0.0)) >= T.PARK_SPACING for c in chosen2)
    assert T.select_parking([]) == []


def test_parking_candidates_mask_straight_kerb_and_lane_requirement():
    m = road_band(5.5)                                                  # 11 m road
    cands = T.parking_candidates_mask(m, step=0.5, headings=8)
    assert cands
    along = [c for c in cands if -30 < c["pos"][0] < 100]   # the band simply ends at x=-40 and x=120
    assert along
    for c in along:
        x, y, _ = c["pos"]
        hx, hy = c["heading"]
        assert abs(hy) < 1e-9                                           # only along-road headings give a straight kerb
        assert abs(abs(y) - (5.5 - T.KERB_INSET)) < 1.0
        assert (hx > 0) == (y < 0)
    assert {c["heading"][0] > 0 for c in along} == {True, False}
    narrow = road_band(2.5)                                             # 5 m lane: no travel lane left beside a car
    assert T.parking_candidates_mask(narrow, step=1.0, headings=8) == []


# ---------------------------------------------------------------------------
# Raster walkable ground (fallback), districts
# ---------------------------------------------------------------------------

def open_ground(w=60.0, h=60.0, cell=0.5):
    data = np.ones((int(h / cell), int(w / cell)), bool)
    z = np.full(data.shape, 7.0, np.float32)
    return T.RoadMask(data, 0.0, 0.0, cell, z)


def test_dilate_and_erode_are_inverse_on_a_blob():
    a = np.zeros((20, 20), bool)
    a[8:12, 8:12] = True
    d = T.dilate(a, 2)
    assert d.sum() > a.sum() and d[6:14, 8:12].all()
    assert (T.erode(d, 2) == a).all()
    assert not T.erode(a, 3).any()


def test_raster_walkable_ring_candidates_are_valid_closed_loops():
    net = T.RasterWalkable(open_ground())
    cands = T.ring_candidates(net, (0, 0, 60, 60), lattice=10.0)
    assert cands
    kinds = {c["kind"] for c in cands}
    assert "ring" in kinds and "racetrack" in kinds
    for c in cands:
        wps = c["waypoints"]
        assert T.LOOP_MIN_WP <= len(wps) <= T.LOOP_MAX_WP and T.LOOP_MIN_M <= c["length"] <= T.LOOP_MAX_M
        assert all(w[2] == 7.0 for w in wps)
        assert all(net.segment_ok(wps[i], wps[(i + 1) % len(wps)]) for i in range(len(wps)))
        assert not T.polyline_self_intersects([w[:2] for w in wps])
    # a wall of non-walkable ground splits the field: no candidate straddles it
    g = open_ground()
    g.data[:, 58:62] = False
    net2 = T.RasterWalkable(g)
    for c in T.ring_candidates(net2, (0, 0, 60, 60), lattice=10.0):
        xs = [w[0] for w in c["waypoints"]]
        assert not (min(xs) < 29 < max(xs)) and not (min(xs) < 31 < max(xs)) or max(xs) < 29 or min(xs) > 31


def test_racetrack_shape():
    rt = T.racetrack(10, 20, 0.0, 12.0, 1.4)
    assert len(rt) == 4 and T.polyline_length(rt, closed=True) == pytest.approx(2 * 12 + 2 * 1.4)
    assert (min(p[0] for p in rt), max(p[0] for p in rt)) == pytest.approx((4.0, 16.0))


def test_aabb_distance_and_assign_nodes():
    box = (0, 0, 10, 10)
    assert T.aabb_distance((5, 5), box) == 0.0
    assert T.aabb_distance((13, 5), box) == pytest.approx(3.0)
    assert T.aabb_distance((13, 14), box) == pytest.approx(5.0)
    nodes = {1: {"pos": (5, 5, 0)}, 2: {"pos": (50, 5, 0)}, 3: {"pos": (200, 200, 0)}}
    fp = {"A": [(0, 0, 10, 10)], "B": [(45, 0, 60, 10)], "Global": [(-1000, -1000, 1000, 1000)]}
    own = T.assign_nodes(nodes, fp, max_dist=25.0)
    assert own == {1: "A", 2: "B", 3: "Global"}                         # smaller box wins an overlap
    own2 = T.assign_nodes(nodes, fp, max_dist=25.0, bias={"Global": 50.0})
    assert own2[3] == "Global" and own2[1] == "A"
    far = T.assign_nodes({9: {"pos": (500, 500, 0)}}, {"A": [(0, 0, 10, 10)]}, max_dist=25.0)
    assert far == {}                                                    # nothing within reach


def test_placement_aabb_applies_rotation_scale_and_position():
    pl = {"game": {"rotation_rows": [[0, -1, 0], [1, 0, 0], [0, 0, 1]],     # +90 deg about z
                   "scale": [2.0, 1.0, 1.0], "position": [100.0, 50.0, 0.0]}}
    # model box x -1..1 (scaled to -2..2), y -3..3: after +90 deg: x' = -y -> -3..3, y' = x -> -2..2
    box = T.placement_aabb(pl, [[-1, -3, 0], [1, 3, 5]])
    assert box == pytest.approx((97.0, 48.0, 103.0, 52.0))


# ---------------------------------------------------------------------------
# Terrain rasters (numpy + Pillow)
# ---------------------------------------------------------------------------

def _tri(*pts):
    return np.array(pts, np.float32)


def tiny_terrain():
    """Road strip y in [-5, 5], x in [0, 40] (2 tris), flat grass y in [5, 25], a 6 m-high wall at y=20, roof tris
    (non-walkable texture), and a lake below the waterline."""
    pytest.importorskip("PIL")
    tris, road, nonwalk, strict, water = [], [], [], [], []

    def quad(x0, y0, x1, y1, z, rd=False, nw=False, wt=False):
        for t in (_tri((x0, y0, z), (x1, y0, z), (x1, y1, z)), _tri((x0, y0, z), (x1, y1, z), (x0, y1, z))):
            tris.append(t)
            road.append(rd)
            strict.append(rd)
            nonwalk.append(nw)
            water.append(wt)

    quad(0, -5, 40, 5, 2.0, rd=True)                     # road
    quad(0, 5, 40, 25, 2.2)                              # grass / sidewalk
    quad(0, 25, 40, 45, 0.0, nw=True, wt=True)           # lake, below the waterline
    quad(10, 8, 14, 12, 8.0, nw=True)                    # a roof
    # vertical wall along y = 20, x 20..30, z 2.2..8.2
    tris.append(_tri((20, 20, 2.2), (30, 20, 2.2), (30, 20, 8.2)))
    tris.append(_tri((20, 20, 2.2), (30, 20, 8.2), (20, 20, 8.2)))
    for _ in range(2):
        road.append(False)
        strict.append(False)
        nonwalk.append(False)
        water.append(False)
    t = np.array(tris, np.float32)
    return T.Terrain(t, np.array(road), np.zeros((len(t), 3), np.float32), t[:, :, 2].max(1),
                     np.array(nonwalk), np.array(strict), water_z=0.0)


def test_terrain_road_mask_covers_road_only_and_carries_height():
    tr = tiny_terrain()
    m = tr.road_mask((0, -10, 40, 30), cell=0.25)
    assert m.at(20, 0) and m.at(1, -4.5) and not m.at(20, 10) and not m.at(20, -8)
    assert m.z_at(20, 0) == pytest.approx(2.0)


def test_terrain_walkable_mask_excludes_road_roof_water_and_blockers():
    tr = tiny_terrain()
    m = tr.walkable_mask((0, -10, 40, 50), cell=0.5, clearance=0.5)
    assert m.at(5, 15) and m.z_at(5, 15) == pytest.approx(2.2)           # open grass
    assert not m.at(20, 0)                                               # road
    assert m.at(12, 10) and m.z_at(12, 10) == pytest.approx(2.2)         # roof tris are not painted: ground under it
    assert not m.at(20, 35)                                              # lake (below the waterline)
    assert not m.at(25, 20) and not m.at(25, 20.7)                       # the wall and its clearance
    assert m.at(35, 20)                                                  # grass past the wall's end
    assert not m.at(20, 5.2) and m.at(20, 6.5)                           # eroded by the clearance beside the road


def test_terrain_height_at_picks_the_surface_nearest_the_hint():
    tr = tiny_terrain()
    h = tr.height_at([(12, 10), (12, 10), (100, 100)], [8.1, 2.2, 0.0])
    assert h[0] == pytest.approx(8.0) and h[1] == pytest.approx(2.2) and h[2] is None
