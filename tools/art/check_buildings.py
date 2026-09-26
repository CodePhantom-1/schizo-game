"""The building gate: every spec in specs.json has its GLB, within budget, with its doorway open where
the street builder puts the door slot. Runs inside Blender; exits 1 on any problem.

  ~/.local/bin/blender -b --factory-startup -P tools/art/check_buildings.py -- \
      [--specs art/generated/buildings/specs.json] [--dir art/generated/buildings]

Manifold is proven part by part in building_mesh.py (the parts overlap, so a whole-file test is
meaningless). The doorway: no triangle may enter the passage (door.x +- width/2 less 0.1 m, from
0.45 m (UE's MaxStepHeight) up to the lintel less 0.1 m, through the wall band); for walled buildings the jambs must stand
within 0.1 m beyond each side of the opening (a ray through the wall there must hit).
"""
import bpy
import json
import os
import sys

from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit_common as kc  # noqa: E402
import building_grammar as bg  # noqa: E402
from building_mesh import budget  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def load(path):
    kc.clear_scene()
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    verts, tris = [], []
    for o in objs:
        m = o.data
        m.calc_loop_triangles()
        base = len(verts)
        verts += [o.matrix_world @ v.co for v in m.vertices]
        tris += [tuple(base + i for i in t.vertices) for t in m.loop_triangles]
    return verts, tris


def box_tris(x0, x1, y0, y1, z0, z1):
    v = [Vector((x, y, z)) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    q = [(0, 1, 3, 2), (4, 5, 7, 6), (0, 1, 5, 4), (2, 3, 7, 6), (0, 2, 6, 4), (1, 3, 7, 5)]
    return v, [t for a, b, c, d in q for t in ((a, b, c), (a, c, d))]


def check(s, path):
    if not os.path.exists(path):
        return ["missing"]
    verts, tris = load(path)
    out = []
    if len(tris) > budget(s):
        out.append(f"{len(tris)} tris > budget {budget(s)}")
    d, hd = s["door"], s["d"] / 2
    sign = 1 if d["side"] == "+y" else -1
    # The mesh is authored with y negated (UE mirrors Y on import): spec y -> Blender -y.
    by = lambda y: -y  # noqa: E731
    x0, x1 = d["x"] - d["width"] / 2 + 0.1, d["x"] + d["width"] / 2 - 0.1
    y0, y1 = sorted((by(sign * (hd + 0.3)), by(sign * (hd - 0.7))))
    bv, bt = box_tris(x0, x1, y0, y1, 0.45, d["height"] - 0.1)
    mesh = BVHTree.FromPolygons(verts, tris, all_triangles=True)
    inside = any(x0 < v.x < x1 and y0 < v.y < y1 and 0.45 < v.z < d["height"] - 0.1 for v in verts)
    if inside or mesh.overlap(BVHTree.FromPolygons(bv, bt, all_triangles=True)):
        out.append("the doorway is blocked")
    if s["typology"] not in bg.REED | bg.TENTS:
        for jx in (d["x"] - d["width"] / 2 - 0.1, d["x"] + d["width"] / 2 + 0.1):
            origin = Vector((jx, by(sign * (hd + 2.0)), 1.0))
            hit, *_ = mesh.ray_cast(origin, Vector((0, by(-sign), 0)), 2.6)
            if hit is None:
                out.append(f"no jamb at x={jx:.2f} (the opening is wider than the door)")
    return out


def main():
    args = kc.parse_args(sys.argv)
    specs = json.load(open(args.get("specs", os.path.join(REPO, "art", "generated", "buildings", "specs.json"))))
    d = args.get("dir", os.path.join(REPO, "art", "generated", "buildings"))
    bad = 0
    for s in specs:
        problems = check(s, os.path.join(d, f"SM_B_{s['id']}.glb"))
        if problems:
            bad += 1
            print(f"FAIL {s['id']}: " + "; ".join(problems))
    print(f"check_buildings: {len(specs) - bad}/{len(specs)} pass")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
