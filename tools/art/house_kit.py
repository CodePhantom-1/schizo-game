"""Generate the modular mudbrick house kit (T8).

Run:
    ~/opt/blender/blender -b --factory-startup -P tools/art/house_kit.py -- --out art/generated

Produces one FBX + one GLB per kit piece in <out>/meshes/, and appends each
piece to art/assets.csv (repo root, tracked).

All architectural specifics not given by the game's canon (db/canon/buildings.csv,
db/canon/cities.csv, docs/world-bible.md) follow real-world Mesopotamian mudbrick
building practice, tag [A] — see kit_common.py header comment for sources.
"""
import bpy
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_common as kc  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _wall_box(name, cut_fn=None):
    bm = kc.new_bmesh()
    kc.add_box(bm, (0, 0, 0), (kc.GRID, kc.WALL_THICK, kc.PLINTH_HEIGHT), mat_index=2)
    kc.add_box(bm, (0, 0, kc.PLINTH_HEIGHT), (kc.GRID, kc.WALL_THICK, kc.WALL_HEIGHT), mat_index=0)
    obj = kc.finalize_object(bm, name, ["M_MudPlaster", "M_Mudbrick", "M_Plinth"])
    if cut_fn:
        cut_fn(obj)
    return obj


def build_wall_plain():
    return _wall_box("SM_WallPlain")


def build_wall_door():
    def cut(obj):
        cx = kc.GRID / 2
        cutter = kc.make_cutter_box(
            "cutter_door",
            (cx - kc.DOOR_WIDTH / 2, -0.05, -0.05),
            (cx + kc.DOOR_WIDTH / 2, kc.WALL_THICK + 0.05, kc.DOOR_HEIGHT),
        )
        kc.boolean_cut(obj, cutter)

    return _wall_box("SM_WallDoor", cut)


def build_wall_window():
    def cut(obj):
        cx = kc.GRID / 2
        cutter = kc.make_cutter_box(
            "cutter_window",
            (cx - kc.WINDOW_W / 2, -0.05, kc.WINDOW_SILL),
            (cx + kc.WINDOW_W / 2, kc.WALL_THICK + 0.05, kc.WINDOW_SILL + kc.WINDOW_H),
        )
        kc.boolean_cut(obj, cutter)

    return _wall_box("SM_WallWindow", cut)


def build_corner():
    bm = kc.new_bmesh()
    t = kc.WALL_THICK
    # leg A: runs along +X at the near (y=0) edge
    kc.add_box(bm, (0, 0, 0), (kc.GRID, t, kc.PLINTH_HEIGHT), mat_index=2)
    kc.add_box(bm, (0, 0, kc.PLINTH_HEIGHT), (kc.GRID, t, kc.WALL_HEIGHT), mat_index=0)
    # leg B: runs along +Y at the near (x=0) edge, starting past leg A's thickness
    # to avoid overlapping geometry with leg A.
    kc.add_box(bm, (0, t, 0), (t, kc.GRID, kc.PLINTH_HEIGHT), mat_index=2)
    kc.add_box(bm, (0, t, kc.PLINTH_HEIGHT), (t, kc.GRID, kc.WALL_HEIGHT), mat_index=0)
    return kc.finalize_object(bm, "SM_Corner", ["M_MudPlaster", "M_Mudbrick", "M_Plinth"])


def build_roof_slab(with_parapet=True, name="SM_RoofSlab"):
    bm = kc.new_bmesh()
    kc.add_box(bm, (0, 0, 0), (kc.GRID, kc.GRID, kc.ROOF_THICK), mat_index=0)
    if with_parapet:
        # parapet along the +Y (outer) edge; assemble_house.py rotates as
        # needed and omits the parapet on interior (non-perimeter) roof
        # tiles, since a real flat roof's parapet only runs around the
        # building's outer edge [A].
        kc.add_box(
            bm,
            (0, kc.GRID - kc.PARAPET_T, kc.ROOF_THICK),
            (kc.GRID, kc.GRID, kc.ROOF_THICK + kc.PARAPET_H),
            mat_index=1,
        )
    return kc.finalize_object(bm, name, ["M_MudPlaster", "M_Mudbrick"])


def build_roof_access():
    bm = kc.new_bmesh()
    kc.add_box(bm, (0, 0, 0), (kc.GRID, kc.GRID, kc.ROOF_THICK), mat_index=0)
    obj = kc.finalize_object(bm, "SM_RoofAccess", ["M_MudPlaster"])
    h = kc.ACCESS_HOLE
    off = (kc.GRID - h) / 2
    cutter = kc.make_cutter_box(
        "cutter_access",
        (off, off, -0.05),
        (off + h, off + h, kc.ROOF_THICK + 0.05),
    )
    kc.boolean_cut(obj, cutter)
    return obj


def build_pilaster():
    bm = kc.new_bmesh()
    w, d = 0.3, 0.15
    kc.add_box(bm, (0, 0, 0), (w, d, kc.PLINTH_HEIGHT), mat_index=2)
    kc.add_box(bm, (0, 0, kc.PLINTH_HEIGHT), (w, d, kc.WALL_HEIGHT), mat_index=0)
    return kc.finalize_object(bm, "SM_Pilaster", ["M_MudPlaster", "M_Mudbrick", "M_Plinth"])


def build_stair():
    bm = kc.new_bmesh()
    n_steps = 10
    run_depth = 2 * kc.GRID
    step_depth = run_depth / n_steps
    step_rise = kc.WALL_HEIGHT / n_steps
    pts = [(0.0, 0.0)]
    y = 0.0
    z = 0.0
    for i in range(n_steps):
        z = (i + 1) * step_rise
        pts.append((y, z))
        y = (i + 1) * step_depth
        pts.append((y, z))
    pts.append((run_depth, 0.0))
    kc.extrude_profile_x(bm, pts, 0.0, kc.GRID, mat_index=1)
    return kc.finalize_object(bm, "SM_Stair", ["M_MudPlaster", "M_Mudbrick"])


def build_lintel():
    bm = kc.new_bmesh()
    w = kc.DOOR_WIDTH + 0.2
    x0 = (kc.GRID - w) / 2
    kc.add_box(
        bm,
        (x0, 0, kc.DOOR_HEIGHT),
        (x0 + w, kc.WALL_THICK, kc.DOOR_HEIGHT + 0.15),
        mat_index=0,
    )
    return kc.finalize_object(bm, "SM_Lintel", ["M_Timber"])


def build_courtyard_tile():
    bm = kc.new_bmesh()
    kc.add_box(bm, (0, 0, 0), (kc.GRID, kc.GRID, kc.TILE_THICK), mat_index=0)
    return kc.finalize_object(bm, "SM_CourtyardTile", ["M_Ground"])


def build_awning():
    bm = kc.new_bmesh()
    pole_w = 0.08
    pole_h = 2.0
    for px, py in ((0.05, kc.GRID - 0.13), (kc.GRID - 0.13, kc.GRID - 0.13)):
        kc.add_box(bm, (px, py, 0), (px + pole_w, py + pole_w, pole_h), mat_index=0)
    # sloped reed-mat panel, thin slab from the wall (high) to the pole line (low)
    mat_pts = [
        (0.0, kc.WALL_HEIGHT - 0.1),
        (0.0, kc.WALL_HEIGHT - 0.13),
        (kc.GRID, pole_h - 0.03),
        (kc.GRID, pole_h),
    ]
    kc.extrude_profile_x(bm, mat_pts, 0.0, kc.GRID, mat_index=1)
    return kc.finalize_object(bm, "SM_Awning", ["M_Timber", "M_Reed"])


PIECES = [
    ("wall_plain", build_wall_plain),
    ("wall_door", build_wall_door),
    ("wall_window", build_wall_window),
    ("corner", build_corner),
    ("roof_slab", build_roof_slab),
    ("roof_access", build_roof_access),
    ("pilaster", build_pilaster),
    ("stair", build_stair),
    ("lintel", build_lintel),
    ("courtyard_tile", build_courtyard_tile),
    ("awning", build_awning),
]


def write_manifest_rows(rows):
    path = os.path.join(REPO_ROOT, "art", "assets.csv")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    is_new = not os.path.exists(path)
    with open(path, "a", newline="") as f:
        w = csv.writer(f)
        if is_new:
            w.writerow(["id", "file", "kind", "license", "source_ref", "tag"])
        for r in rows:
            w.writerow(r)


def main():
    args = kc.parse_args(sys.argv)
    out_dir = args.get("out", "art/generated")
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(REPO_ROOT, out_dir)
    os.makedirs(out_dir, exist_ok=True)

    kc.clear_scene()
    manifest_rows = []
    for budget_key, build_fn in PIECES:
        obj = build_fn()
        tris = kc.finish_piece(obj, budget_key)
        fbx_path, glb_path = kc.export_piece(obj, out_dir)
        rel = os.path.relpath(fbx_path, REPO_ROOT)
        print(f"{obj.name}: {tris} tris -> {rel}")
        manifest_rows.append(
            [
                obj.name,
                rel,
                "mesh_kit_piece",
                "original (procedural, generated code)",
                "docs/art-pipeline-research.md; real-world Mesopotamian mudbrick architecture [A]",
                "A",
            ]
        )
    write_manifest_rows(manifest_rows)
    kc.copy_textures_for_export(out_dir)
    print(f"House kit: {len(PIECES)} pieces exported to {out_dir}")


if __name__ == "__main__":
    main()
