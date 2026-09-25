"""Shared geometry/material/export helpers for the mudbrick house kit.

Run only inside Blender's Python (`blender -b -P ...`). Not a standalone module.

Scale: Blender scene stays in real-world METERS. Export uses global_scale=1.0 with
apply_unit_scale=True, so FBX/GLB carry the unit metadata and UE imports at the
correct 1-unit-=-1-cm scale automatically (this is Blender's and UE's shared
default pipeline — no manual x100 scale math needed).

Grid: every piece is modelled with its local origin at the bottom-back-left
corner of its bounding box, so pieces tile by placing them at
(i*GRID, j*GRID, k*WALL_HEIGHT) with no rotation math beyond 90-degree steps.
"""
import bpy
import bmesh
import math
import os

# ---- Grid + real-world dimensions (source: real Mesopotamian mudbrick
# residential architecture, tag [A] — general practice attested at Ur III /
# Old Babylonian house sites, e.g. Woolley's excavations at Ur, and standard
# treatments of Mesopotamian vernacular building: thick mudbrick walls on a
# stone/baked-brick footing course, flat roofs of mud over reed/timber,
# courtyard houses, clerestory-style high small windows for privacy and heat
# control, roof access by internal stair/ladder for sleeping on the roof.) ----
GRID = 1.0                 # m — one module = 100 UE units (1 unit = 1 cm)
WALL_HEIGHT = 2.6           # m — low flat-roofed room height [A]
WALL_THICK = 0.4            # m — mudbrick wall thickness [A]
PLINTH_HEIGHT = 0.15        # m — stone/baked-brick footing course under mudbrick [A]
DOOR_WIDTH = 0.9            # m
DOOR_HEIGHT = 2.0           # m
WINDOW_W = 0.35             # m
WINDOW_H = 0.3              # m
WINDOW_SILL = 1.9           # m — high clerestory-style opening [A]
PARAPET_H = 0.3             # m
PARAPET_T = 0.1             # m
ROOF_THICK = 0.15           # m
ACCESS_HOLE = 0.8           # m — square roof-access hole
BEVEL_W = 0.015             # m — cheap geometry-only "worn mudbrick" edge bevel
TILE_THICK = 0.08           # m — courtyard floor tile

# Triangle budgets per piece (documented; check_kit.py enforces these).
TRI_BUDGET = {
    "wall_plain": 250,
    "wall_door": 500,
    "wall_window": 500,
    "corner": 350,
    "roof_slab": 450,
    "roof_access": 500,
    "pilaster": 220,
    "stair": 700,
    "lintel": 220,
    "courtyard_tile": 150,
    "awning": 450,
}

MAT_DEFS = {
    "M_MudPlaster": ((0.76, 0.62, 0.42), 0.95),
    "M_Mudbrick": ((0.55, 0.40, 0.27), 0.92),
    "M_Timber": ((0.30, 0.19, 0.10), 0.55),
    "M_Reed": ((0.68, 0.58, 0.30), 0.85),
}


def get_material(name):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    color, rough = MAT_DEFS[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    # Workbench solid/material-preview shading reads viewport-display
    # diffuse_color, not the node tree, so set it explicitly too.
    mat.diffuse_color = (*color, 1.0)
    mat.roughness = rough
    return mat


def new_bmesh():
    return bmesh.new()


def add_box(bm, mn, mx, mat_index=0):
    """Add a box's 6 faces to bm. mn/mx are (x,y,z) tuples. Normals fixed later."""
    x0, y0, z0 = mn
    x1, y1, z1 = mx
    v = [
        bm.verts.new((x0, y0, z0)),
        bm.verts.new((x1, y0, z0)),
        bm.verts.new((x1, y1, z0)),
        bm.verts.new((x0, y1, z0)),
        bm.verts.new((x0, y0, z1)),
        bm.verts.new((x1, y0, z1)),
        bm.verts.new((x1, y1, z1)),
        bm.verts.new((x0, y1, z1)),
    ]
    face_idx = [
        (0, 1, 2, 3), (7, 6, 5, 4),
        (0, 4, 5, 1), (1, 5, 6, 2),
        (2, 6, 7, 3), (3, 7, 4, 0),
    ]
    for f in face_idx:
        try:
            face = bm.faces.new([v[i] for i in f])
            face.material_index = mat_index
        except ValueError:
            pass  # duplicate face from welded shared box faces; ignore
    return v


def extrude_profile_x(bm, points, x0, x1, mat_index=0):
    """Extrude a closed 2D (y,z) profile along X between x0 and x1: two end
    caps plus side quads. Used for the stair and the awning's mat panel."""
    n = len(points)
    v0 = [bm.verts.new((x0, y, z)) for y, z in points]
    v1 = [bm.verts.new((x1, y, z)) for y, z in points]
    try:
        f0 = bm.faces.new(list(reversed(v0)))
        f0.material_index = mat_index
    except ValueError:
        pass
    try:
        f1 = bm.faces.new(v1)
        f1.material_index = mat_index
    except ValueError:
        pass
    for i in range(n):
        j = (i + 1) % n
        try:
            face = bm.faces.new([v0[i], v0[j], v1[j], v1[i]])
            face.material_index = mat_index
        except ValueError:
            pass
    return v0 + v1


def finalize_object(bm, name, slot_names, weld=True):
    """Build a mesh object from bm, weld verts, recalc normals, set material slots."""
    if weld:
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for slot in slot_names:
        mesh.materials.append(get_material(slot))
    return obj


def boolean_cut(obj, cutter_obj):
    """Apply a DIFFERENCE boolean of cutter_obj on obj, then delete the cutter."""
    mod = obj.modifiers.new("cut", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = cutter_obj
    mod.solver = "EXACT"
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter_obj, do_unlink=True)


def make_cutter_box(name, mn, mx):
    bm = bmesh.new()
    add_box(bm, mn, mx)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def apply_bevel(obj, width=BEVEL_W, angle_deg=50):
    mod = obj.modifiers.new("bev", "BEVEL")
    mod.width = width
    mod.segments = 1
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(angle_deg)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def unwrap_uv0_uv1(obj, margin0=0.02, margin1=0.04):
    """UV0 = normal unwrap for texturing. UV1 = a second, more-padded
    non-overlapping projection used as the lightmap UV (ponytail: reusing
    smart_project's non-overlapping-island property instead of driving the
    dedicated lightmap_pack operator, which needs a live 3D-view context that
    doesn't exist in background mode; upgrade to lightmap_pack if baked
    lighting seams show up in-engine)."""
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=margin0)
    bpy.ops.object.mode_set(mode="OBJECT")

    uv1 = obj.data.uv_layers.new(name="lightmap")
    obj.data.uv_layers.active = uv1
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=margin1)
    bpy.ops.object.mode_set(mode="OBJECT")
    # keep UV0 active/render layer as the first one
    obj.data.uv_layers.active = obj.data.uv_layers[0]


def triangulate_count(obj):
    return sum(max(len(p.vertices) - 2, 0) for p in obj.data.polygons)


def finish_piece(obj, budget_key):
    apply_bevel(obj)
    unwrap_uv0_uv1(obj)
    tris = triangulate_count(obj)
    budget = TRI_BUDGET[budget_key]
    if tris > budget:
        print(f"WARNING: {obj.name} has {tris} tris, over budget {budget}")
    return tris


def export_piece(obj, out_dir):
    mesh_dir = os.path.join(out_dir, "meshes")
    os.makedirs(mesh_dir, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    fbx_path = os.path.join(mesh_dir, f"{obj.name}.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx_path,
        use_selection=True,
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="-Z",
        axis_up="Y",
        object_types={"MESH"},
        use_mesh_modifiers=True,
    )
    glb_path = os.path.join(mesh_dir, f"{obj.name}.glb")
    bpy.ops.export_scene.gltf(
        filepath=glb_path, use_selection=True, export_format="GLB"
    )
    return fbx_path, glb_path


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block_coll in (bpy.data.meshes, bpy.data.materials):
        for block in list(block_coll):
            if block.users == 0:
                block_coll.remove(block)


def parse_args(argv):
    """Return dict from '--key value' pairs after the '--' in sys.argv."""
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []
    out = {}
    i = 0
    while i < len(argv):
        if argv[i].startswith("--"):
            key = argv[i][2:]
            if i + 1 < len(argv) and not argv[i + 1].startswith("--"):
                out[key] = argv[i + 1]
                i += 2
            else:
                out[key] = True
                i += 1
        else:
            i += 1
    return out
