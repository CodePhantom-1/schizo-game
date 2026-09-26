"""The scatter gate: every row of art/scatter_meshes.csv has its GLB, with faces, within its triangle
budget, standing on z = 0 (+-2 cm). Runs inside Blender; exits 1 on any problem.

  blender -b --factory-startup -P tools/art/check_scatter.py -- \
      [--meshes art/scatter_meshes.csv] [--dir art/generated/scatter]
"""
import bpy
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit_common as kc  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def check(row, path):
    if not os.path.exists(path):
        return ["missing"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    zs = [(o.matrix_world @ v.co).z for o in objs for v in o.data.vertices]
    tris = sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)
    out = []
    if tris == 0:
        return ["zero faces"]
    if tris > int(row["max_tris"]):
        out.append(f"{tris} tris > budget {row['max_tris']}")
    if abs(min(zs)) > 0.02:
        out.append(f"bottom at z={min(zs):.3f}, not 0")
    return out


def main():
    args = kc.parse_args(sys.argv)
    rows = list(csv.DictReader(open(args.get("meshes", os.path.join(REPO, "art", "scatter_meshes.csv")), newline="")))
    d = args.get("dir", os.path.join(REPO, "art", "generated", "scatter"))
    bad = 0
    for r in rows:
        problems = check(r, os.path.join(d, f"SM_F_{r['id']}.glb"))
        if problems:
            bad += 1
            print(f"FAIL {r['id']}: " + "; ".join(problems))
    print(f"check_scatter: {len(rows) - bad}/{len(rows)} pass")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
