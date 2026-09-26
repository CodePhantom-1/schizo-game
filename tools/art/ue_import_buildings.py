"""ue_import_buildings.py — imports the generated buildings (tools/art/building_mesh.py GLBs) to
/Game/Art/Buildings, the trim atlas to T_Trim, and authors M_Building. Headless, from the repo root:

    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" \
        -run=pythonscript -script="$PWD/tools/art/ue_import_buildings.py"   # the script path must be absolute

M_Building: BaseColor = T_Trim at (frac(UV0) + UV1) / 8  x  VertexColor.rgb x 2, lerped toward the
same material's "cracked" cell (column (UV1.x // 4) * 4 + 3, same row) by DroughtWear (scalar
parameter, default 0) x VertexColor.a. Roughness 0.95. See building_mesh.py for the UV/colour
contract. Each mesh: its one slot set to M_Building, collision complex-as-simple (the door
openings are walkable), then its bounds are checked against its spec (scale and the Y mirror).
"""
import json
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # UE's pythonscript runner does not always set __file__
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
GEN = os.path.join(REPO, "art", "generated")
DEST = "/Game/Art/Buildings"
STAGING = f"{DEST}/_staging"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def import_file(path, dest_path, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest_path
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return [str(p) for p in EAL.list_assets(dest_path, recursive=True)]


def trim_texture():
    path = f"{DEST}/T_Trim"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    import_file(os.path.join(GEN, "tex", "T_Trim.png"), DEST, "T_Trim")
    tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError("T_Trim did not import")
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(tex)
    return tex


def building_material(tex):
    path = f"{DEST}/M_Building"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Building", DEST, unreal.Material, unreal.MaterialFactoryNew())
    E = lambda cls, x, y: MEL.create_material_expression(m, cls, x, y)  # noqa: E731
    C = MEL.connect_material_expressions
    uv0, uv1 = E(unreal.MaterialExpressionTextureCoordinate, -1600, 0), E(unreal.MaterialExpressionTextureCoordinate, -1600, 200)
    uv1.set_editor_property("coordinate_index", 1)
    frac = E(unreal.MaterialExpressionFrac, -1400, 0)
    C(uv0, "", frac, "")

    def sample(cell_uv, y):
        add = E(unreal.MaterialExpressionAdd, -1000, y)
        C(frac, "", add, "A")
        C(cell_uv, "", add, "B")
        div = E(unreal.MaterialExpressionDivide, -850, y)
        div.set_editor_property("const_b", 8.0)
        C(add, "", div, "A")
        ts = E(unreal.MaterialExpressionTextureSample, -650, y)
        ts.set_editor_property("texture", tex)
        C(div, "", ts, "UVs")
        return ts

    fresh = sample(uv1, 0)
    # The cracked cell of the same material: column (x // 4) * 4 + 3, same row.
    cx = E(unreal.MaterialExpressionComponentMask, -1400, 300)
    cy = E(unreal.MaterialExpressionComponentMask, -1400, 400)
    for mask, keep in ((cx, "r"), (cy, "g")):  # every channel set: the defaults are not one channel
        for ch in "rgba":
            mask.set_editor_property(ch, ch == keep)
    C(uv1, "", cx, "")
    C(uv1, "", cy, "")
    d4 = E(unreal.MaterialExpressionDivide, -1300, 300)
    d4.set_editor_property("const_b", 4.0)
    C(cx, "", d4, "A")
    fl = E(unreal.MaterialExpressionFloor, -1200, 300)
    C(d4, "", fl, "")
    m4 = E(unreal.MaterialExpressionMultiply, -1100, 300)
    m4.set_editor_property("const_b", 4.0)
    C(fl, "", m4, "A")
    a3 = E(unreal.MaterialExpressionAdd, -1050, 300)
    a3.set_editor_property("const_b", 3.0)
    C(m4, "", a3, "A")
    app = E(unreal.MaterialExpressionAppendVector, -1000, 350)
    C(a3, "", app, "A")
    C(cy, "", app, "B")
    cracked = sample(app, 400)

    vc = E(unreal.MaterialExpressionVertexColor, -650, 700)
    wear = E(unreal.MaterialExpressionScalarParameter, -650, 900)
    wear.set_editor_property("parameter_name", "DroughtWear")
    wear.set_editor_property("default_value", 0.0)
    alpha = E(unreal.MaterialExpressionMultiply, -450, 850)
    C(vc, "A", alpha, "A")
    C(wear, "", alpha, "B")
    lerp = E(unreal.MaterialExpressionLinearInterpolate, -350, 200)
    C(fresh, "RGB", lerp, "A")
    C(cracked, "RGB", lerp, "B")
    C(alpha, "", lerp, "Alpha")
    tint = E(unreal.MaterialExpressionMultiply, -200, 300)
    C(lerp, "", tint, "A")
    C(vc, "RGB", tint, "B")
    x2 = E(unreal.MaterialExpressionMultiply, -80, 300)
    x2.set_editor_property("const_b", 2.0)
    C(tint, "", x2, "A")
    rough = E(unreal.MaterialExpressionConstant, -200, 500)
    rough.set_editor_property("r", 0.95)
    MEL.connect_material_property(x2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def bounds_ok(mesh, s):
    """Scale: the footprint fits in cm. Mirror: the side the street markers stick out on in the spec
    is the side they stick out on in UE (door side = +y or -y)."""
    box = mesh.get_bounding_box()
    lo, hi = box.min, box.max
    if abs((hi.x - lo.x) - s["w"] * 100) > 400 or abs((hi.y - lo.y) - s["d"] * 100) > 400:
        return f"size {hi.x - lo.x:.0f}x{hi.y - lo.y:.0f} cm vs {s['w']}x{s['d']} m"
    ys = [p["at"][1] + sgn * p["size"][1] / 2 for p in s["parts"] for sgn in (-1, 1)]
    want = max(ys) + min(ys)  # > 0: the spec reaches further toward +y
    got = hi.y + lo.y
    if abs(want) > 0.3 and (want > 0) != (got > 0):
        return f"mirrored (spec leans {want:+.2f} m in y, mesh {got:+.0f} cm)"
    return None


def main():
    specs = json.load(open(os.path.join(GEN, "buildings", "specs.json")))
    EAL.make_directory(DEST)
    mat = building_material(trim_texture())
    ok, bad = 0, []
    for s in specs:
        name = f"SM_B_{s['id']}"
        final = f"{DEST}/{name}"
        glb = os.path.join(GEN, "buildings", f"{name}.glb")
        if not os.path.exists(glb):
            bad.append(f"{name}: no GLB (run building_mesh.py)")
            continue
        if EAL.does_asset_exist(final):
            EAL.delete_asset(final)
        if EAL.does_directory_exist(STAGING):
            EAL.delete_directory(STAGING)
        landed = import_file(glb, STAGING, name)
        meshes = [p for p in landed if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
        if not meshes or not EAL.rename_asset(meshes[0], final):
            bad.append(f"{name}: import produced {landed}")
            continue
        mesh = unreal.load_asset(final)
        mesh.set_material(0, mat)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        EAL.save_loaded_asset(mesh)
        problem = bounds_ok(mesh, s)
        if problem:
            bad.append(f"{name}: {problem}")
            continue
        ok += 1
    if EAL.does_directory_exist(STAGING):
        EAL.delete_directory(STAGING)  # Interchange's placeholder materials: every mesh now uses M_Building
    for b in bad:
        unreal.log_error(f"ue_import_buildings: {b}")
    unreal.log(f"ue_import_buildings: {ok}/{len(specs)} buildings imported to {DEST}")
    if bad:
        sys.exit(1)


main()
