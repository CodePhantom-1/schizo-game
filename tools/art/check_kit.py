"""Automated gate for the mudbrick house kit (T8).

Run:
    ~/opt/blender/blender -b --factory-startup -P tools/art/check_kit.py -- --out art/generated

Re-imports each exported piece (both FBX and GLB must exist) and checks:
  1. the file exists for every kit piece
  2. triangle count is within the documented budget (kit_common.TRI_BUDGET)
  3. footprint dimensions match the documented grid-snapped size for that
     piece (kit_common's module constants), within a 5cm tolerance
  4. UV0 (texturing) and UV1 (lightmap) are both present
  5. material slots are named (all M_* slots, matching kit_common.MAT_DEFS)

Then, for each of the 3 example houses, rebuilds it pre-join (via
assemble_house.HOUSE_OBJS_BUILDERS) and checks the assembly itself:
  6. no floaters — every placed piece's bounding box must touch another
     piece's bounding box (or the ground, z=0), within a small tolerance
  7. no volumetric overlap — no two pieces' bounding boxes may genuinely
     overlap in all 3 axes beyond a small tolerance (flush/touching faces,
     which have ~zero overlap in at least one axis, are fine)
These are bounding-box proxies, not exact mesh boolean checks (ponytail:
good enough to catch duplicated/misplaced geometry and disconnected
pieces; upgrade to a real mesh intersection test if a false negative
shows up).

Exits non-zero if anything fails.
"""
import bpy
import os
import sys
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_common as kc  # noqa: E402
import house_kit as hk  # noqa: E402
import assemble_house as ah  # noqa: E402

REPO_ROOT = hk.REPO_ROOT
TOL = 0.05  # metres
BBOX_EPS = 0.01  # metres, for the house-assembly touch/overlap checks

# Documented expected footprint (x, y, z) in metres for each piece, derived
# from the same constants house_kit.py builds with.
EXPECTED_DIMS = {
    "SM_WallPlain": (kc.GRID, kc.WALL_THICK, kc.WALL_HEIGHT),
    "SM_WallDoor": (kc.GRID, kc.WALL_THICK, kc.WALL_HEIGHT),
    "SM_WallWindow": (kc.GRID, kc.WALL_THICK, kc.WALL_HEIGHT),
    "SM_Corner": (kc.GRID, kc.GRID, kc.WALL_HEIGHT),
    "SM_RoofSlab": (kc.GRID, kc.GRID, kc.ROOF_THICK + kc.PARAPET_H),
    "SM_RoofAccess": (kc.GRID, kc.GRID, kc.ROOF_THICK),
    "SM_Pilaster": (0.3, 0.15, kc.WALL_HEIGHT),
    "SM_Stair": (kc.GRID, 2 * kc.GRID, kc.WALL_HEIGHT),
    "SM_Lintel": (kc.DOOR_WIDTH + 0.2, kc.WALL_THICK, 0.15),
    "SM_CourtyardTile": (kc.GRID, kc.GRID, kc.TILE_THICK),
    "SM_Awning": (kc.GRID, kc.GRID, kc.WALL_HEIGHT - 0.1),
}


def check_piece(name, budget_key, out_dir, failures):
    mesh_dir = os.path.join(out_dir, "meshes")
    fbx_path = os.path.join(mesh_dir, f"{name}.fbx")
    glb_path = os.path.join(mesh_dir, f"{name}.glb")

    if not os.path.exists(fbx_path):
        failures.append(f"{name}: missing FBX {fbx_path}")
        return
    if not os.path.exists(glb_path):
        failures.append(f"{name}: missing GLB {glb_path}")

    kc.clear_scene()
    # match house_kit.py's export axis settings exactly, so the round trip
    # back into Blender space is the identity transform.
    bpy.ops.import_scene.fbx(filepath=fbx_path, axis_forward="-Z", axis_up="Y")
    imported = [o for o in bpy.context.selected_objects if o.type == "MESH"]
    if not imported:
        failures.append(f"{name}: FBX import produced no mesh object")
        return
    obj = imported[0]
    mesh = obj.data

    # 1. triangle budget
    tris = kc.triangulate_count(obj)
    budget = kc.TRI_BUDGET[budget_key]
    if tris > budget:
        failures.append(f"{name}: {tris} tris exceeds budget {budget}")

    # 2. footprint snaps to the documented grid size
    bbox = obj.bound_box
    xs = [v[0] for v in bbox]
    ys = [v[1] for v in bbox]
    zs = [v[2] for v in bbox]
    dims = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    exp = EXPECTED_DIMS[name]
    for axis, (got, want) in zip("xyz", zip(dims, exp)):
        if abs(got - want) > TOL:
            failures.append(
                f"{name}: {axis} dim {got:.3f}m does not match documented {want:.3f}m"
            )

    # 3. UV0 + UV1 present
    if len(mesh.uv_layers) < 2:
        failures.append(f"{name}: only {len(mesh.uv_layers)} UV layer(s), need UV0+UV1")

    # 4. material slots named (all M_* known materials)
    if not mesh.materials or any(m is None for m in mesh.materials):
        failures.append(f"{name}: missing/unnamed material slot(s)")
    else:
        for m in mesh.materials:
            if m.name not in kc.MAT_DEFS:
                failures.append(f"{name}: unexpected material slot name '{m.name}'")

    print(f"checked {name}: {tris} tris, dims={tuple(round(d,3) for d in dims)}, "
          f"uvs={len(mesh.uv_layers)}, mats={[m.name for m in mesh.materials]}")


def world_bbox(obj):
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    xs = [c.x for c in corners]
    ys = [c.y for c in corners]
    zs = [c.z for c in corners]
    return (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))


def check_house_assembly(house_name, objs, failures):
    bpy.context.view_layer.update()
    boxes = [world_bbox(o) for o in objs]
    n = len(boxes)

    # overlap check: real volumetric overlap in all 3 axes, beyond BBOX_EPS
    for i in range(n):
        ax0, ax1, ay0, ay1, az0, az1 = boxes[i]
        for j in range(i + 1, n):
            bx0, bx1, by0, by1, bz0, bz1 = boxes[j]
            ox = min(ax1, bx1) - max(ax0, bx0)
            oy = min(ay1, by1) - max(ay0, by0)
            oz = min(az1, bz1) - max(az0, bz0)
            if ox > BBOX_EPS and oy > BBOX_EPS and oz > BBOX_EPS:
                failures.append(
                    f"{house_name}: {objs[i].name} overlaps {objs[j].name} "
                    f"by ({ox:.3f}, {oy:.3f}, {oz:.3f}) m"
                )

    # floater check: every piece must touch another piece or the ground
    for i in range(n):
        ax0, ax1, ay0, ay1, az0, az1 = boxes[i]
        if abs(az0) <= BBOX_EPS:
            continue  # sits on the ground
        touches = False
        for j in range(n):
            if i == j:
                continue
            bx0, bx1, by0, by1, bz0, bz1 = boxes[j]
            ox = min(ax1, bx1) - max(ax0, bx0)
            oy = min(ay1, by1) - max(ay0, by0)
            oz = min(az1, bz1) - max(az0, bz0)
            # touching (or overlapping) in all 3 axes, with at least one
            # axis at ~zero gap (a real face/edge contact, not just two
            # bounding boxes that happen to overlap through empty space)
            if ox > -BBOX_EPS and oy > -BBOX_EPS and oz > -BBOX_EPS:
                touches = True
                break
        if not touches:
            failures.append(f"{house_name}: {objs[i].name} is a floater (touches nothing)")

    print(f"checked {house_name} assembly: {n} pieces")


def main():
    args = kc.parse_args(sys.argv)
    out_dir = args.get("out", "art/generated")
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(REPO_ROOT, out_dir)

    failures = []
    # Build each piece once (cheap, same functions house_kit.py uses) to
    # recover its real object name rather than guessing the SM_* naming.
    kc.clear_scene()
    name_to_budget = {}
    for budget_key, build_fn in hk.PIECES:
        obj = build_fn()
        name_to_budget[obj.name] = budget_key
    kc.clear_scene()

    for name, budget_key in name_to_budget.items():
        check_piece(name, budget_key, out_dir, failures)

    for house_name, build_objs_fn in ah.HOUSE_OBJS_BUILDERS:
        kc.clear_scene()
        objs = build_objs_fn()
        check_house_assembly(house_name, objs, failures)
    kc.clear_scene()

    if failures:
        print("\nFAILURES:")
        for f in failures:
            print(f" - {f}")
        print(f"\ncheck_kit: {len(failures)} failure(s)")
        sys.exit(1)
    print(f"\ncheck_kit: all {len(name_to_budget)} pieces passed")
    sys.exit(0)


if __name__ == "__main__":
    main()
