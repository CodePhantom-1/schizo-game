"""ue_import_scatter.py — imports the scatter meshes (tools/art/scatter_mesh.py GLBs) to /Game/Art/Scatter,
and authors MPC_World and M_Scatter. Headless, from the repo root (the script path must be absolute):

    <UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "%CD%/unreal/SchizoGame.uproject" \
        -run=pythonscript -script="%CD%/tools/art/ue_import_scatter.py"

MPC_World: scalars Wither (0..1, the drought) and Bloom (0..1), driven by ASimScatter.
M_Scatter: BaseColor = lerp(VertexColor.rgb ^ 2.2, straw (palette reed_2), MPC_World.Wither x VertexColor.a)
(the mesh build stores vertex colours sRGB-encoded, ToFColor(true): the power decodes them to linear);
Roughness 0.9; two-sided; used with instanced static meshes; World Position Offset wind =
sin(Time x 1.7 + WorldPosition.x x 0.01) x 3 cm x saturate(local z / 300 cm) x VertexColor.a, so it
sways leaves only and never stones or props (their A is 0). Each mesh: slot 0 = M_Scatter; trees, the
tamarisk and props block with one simple box; grass, flowers, lilies, reeds and bushes have no
collision.
"""
import csv
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # UE's pythonscript runner does not always set __file__
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
GEN = os.path.join(REPO, "art", "generated", "scatter")
DEST = "/Game/Art/Scatter"
STAGING = f"{DEST}/_staging"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
BLOCKING_GROUPS = {"tree", "prop"}
BLOCKING_EXTRA = {"tamarisk"}  # a 4 m tree the flora table files as a shrub


def rows(path):
    with open(path, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def palette_linear(name):
    for r in rows(os.path.join(REPO, "art", "palette.csv")):
        if r["name"] == name:
            lin = lambda c: c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4  # noqa: E731
            return tuple(lin(int(r[k]) / 255.0) for k in "rgb")
    raise KeyError(name)


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


def fresh(path):
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)


def world_collection():
    path = f"{DEST}/MPC_World"
    # Kept, not recreated: M_Scatter references it, and ASimScatter finds it by path.
    mpc = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "MPC_World", DEST, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    if mpc is None:
        raise RuntimeError("MPC_World could not be created")
    # Add only what is missing: replacing the list gives every parameter a new id, and a material authored
    # against the old ids (M_Terrain) then fails to compile (V-B3).
    params = list(mpc.get_editor_property("scalar_parameters"))
    have = {str(p.get_editor_property("parameter_name")) for p in params}
    for name in ("Wither", "Bloom", "Wet", "Dust", "Festival", "Shimmer", "Rain"):  # V-B5 added the last five
        if name not in have:
            p = unreal.CollectionScalarParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", 0.0)
            params.append(p)
    if len(params) != len(have):
        mpc.set_editor_property("scalar_parameters", params)
        EAL.save_loaded_asset(mpc)
    return mpc


def mask(m, x, y, keep):
    """A ComponentMask with every channel set explicitly (UE's defaults are not one channel)."""
    e = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    for ch in "rgba":
        e.set_editor_property(ch, ch in keep)
    return e


def scatter_material(mpc):
    path = f"{DEST}/M_Scatter"
    fresh(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Scatter", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    E = lambda cls, x, y: MEL.create_material_expression(m, cls, x, y)  # noqa: E731
    def C(a, out, b, into):  # a pin name that does not exist connects nothing and says nothing: fail loudly
        if not MEL.connect_material_expressions(a, out, b, into):
            raise RuntimeError(f"M_Scatter: could not connect {a.get_name()}.{out!r} -> {b.get_name()}.{into!r}")

    # Colour: the leaves turn to straw as the drought (MPC_World.Wither) rises; A masks leaves only.
    vc = E(unreal.MaterialExpressionVertexColor, -900, 0)
    straw = E(unreal.MaterialExpressionConstant3Vector, -900, 200)
    straw.set_editor_property("constant", unreal.LinearColor(*palette_linear("reed_2"), 1.0))
    wither = E(unreal.MaterialExpressionCollectionParameter, -900, 350)
    wither.set_editor_property("collection", mpc)
    wither.set_editor_property("parameter_name", "Wither")
    amount = E(unreal.MaterialExpressionMultiply, -650, 300)
    C(wither, "", amount, "A")
    C(vc, "A", amount, "B")
    # VertexColor's RGB output pin is named "" (then R, G, B, A): "RGB" silently connects nothing.
    decode = E(unreal.MaterialExpressionPower, -700, 0)
    decode.set_editor_property("const_exponent", 2.2)
    C(vc, "", decode, "Base")
    lerp = E(unreal.MaterialExpressionLinearInterpolate, -450, 100)
    C(decode, "", lerp, "A")
    C(straw, "", lerp, "B")
    C(amount, "", lerp, "Alpha")
    rough = E(unreal.MaterialExpressionConstant, -450, 300)
    rough.set_editor_property("r", 0.9)

    # Wind: sin(Time * 1.7 + WorldPosition.x * 0.01) * 3 cm * saturate(local z / 300 cm) * VertexColor.a.
    t = E(unreal.MaterialExpressionTime, -1300, 600)
    t17 = E(unreal.MaterialExpressionMultiply, -1150, 600)
    t17.set_editor_property("const_b", 1.7)
    C(t, "", t17, "A")
    wp = E(unreal.MaterialExpressionWorldPosition, -1300, 750)
    wpx = mask(m, -1150, 750, "r")
    C(wp, "", wpx, "")
    phase = E(unreal.MaterialExpressionMultiply, -1000, 750)
    phase.set_editor_property("const_b", 0.01)
    C(wpx, "", phase, "A")
    arg = E(unreal.MaterialExpressionAdd, -850, 650)
    C(t17, "", arg, "A")
    C(phase, "", arg, "B")
    sine = E(unreal.MaterialExpressionSine, -700, 650)
    sine.set_editor_property("period", 6.283185)
    C(arg, "", sine, "")
    op = E(unreal.MaterialExpressionObjectPositionWS, -1300, 900)
    local = E(unreal.MaterialExpressionSubtract, -1150, 850)
    C(wp, "", local, "A")
    C(op, "", local, "B")
    lz = mask(m, -1000, 900, "b")
    C(local, "", lz, "")
    h = E(unreal.MaterialExpressionDivide, -850, 900)
    h.set_editor_property("const_b", 300.0)
    C(lz, "", h, "A")
    sat = E(unreal.MaterialExpressionSaturate, -700, 900)
    C(h, "", sat, "")
    amp = E(unreal.MaterialExpressionMultiply, -550, 700)
    C(sine, "", amp, "A")
    C(sat, "", amp, "B")
    leafy = E(unreal.MaterialExpressionMultiply, -400, 700)
    C(amp, "", leafy, "A")
    C(vc, "A", leafy, "B")
    cm3 = E(unreal.MaterialExpressionMultiply, -250, 700)
    cm3.set_editor_property("const_b", 3.0)
    C(leafy, "", cm3, "A")
    dir_ = E(unreal.MaterialExpressionConstant3Vector, -250, 850)
    dir_.set_editor_property("constant", unreal.LinearColor(1.0, 0.6, 0.0, 1.0))  # the prevailing wind
    wpo = E(unreal.MaterialExpressionMultiply, -100, 750)
    C(cm3, "", wpo, "A")
    C(dir_, "", wpo, "B")

    # V-B5 wetness: MPC_World.Wet darkens the albedo to x0.7 and glosses the roughness to 0.4 (0: unchanged).
    wet = E(unreal.MaterialExpressionCollectionParameter, -450, 450)
    wet.set_editor_property("collection", mpc)
    wet.set_editor_property("parameter_name", "Wet")
    dark = E(unreal.MaterialExpressionLinearInterpolate, -300, 400)
    dark.set_editor_property("const_a", 1.0)
    dark.set_editor_property("const_b", 0.7)
    C(wet, "", dark, "Alpha")
    wet_base = E(unreal.MaterialExpressionMultiply, -150, 100)
    C(lerp, "", wet_base, "A")
    C(dark, "", wet_base, "B")
    gloss = E(unreal.MaterialExpressionLinearInterpolate, -300, 300)
    C(rough, "", gloss, "A")
    gloss.set_editor_property("const_b", 0.4)
    C(wet, "", gloss, "Alpha")
    MEL.connect_material_property(wet_base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(gloss, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def blocking_meshes():
    out = set(BLOCKING_EXTRA)
    for f in rows(os.path.join(REPO, "db", "canon", "flora.csv")):
        if f["group"] in BLOCKING_GROUPS:
            out.update(f["meshes"].split(";"))
    return out


def set_collision(mesh, block):
    # The subsystem is missing in the headless commandlet; the older library does the same there.
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    sub.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if block:
        shape = getattr(unreal, "ScriptCollisionShapeType", None) or unreal.ScriptingCollisionShapeType
        sub.add_simple_collisions(mesh, shape.BOX)
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    else:
        # no simple shapes and simple-as-complex: nothing to hit
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)


def main():
    meshes = rows(os.path.join(REPO, "art", "scatter_meshes.csv"))
    EAL.make_directory(DEST)
    mat = scatter_material(world_collection())
    block = blocking_meshes()
    ok, bad = 0, []
    for r in meshes:
        name = f"SM_F_{r['id']}"
        final = f"{DEST}/{name}"
        glb = os.path.join(GEN, f"{name}.glb")
        if not os.path.exists(glb):
            bad.append(f"{name}: no GLB (run scatter_mesh.py)")
            continue
        fresh(final)
        if EAL.does_directory_exist(STAGING):
            EAL.delete_directory(STAGING)
        landed = import_file(glb, STAGING, name)
        found = [p for p in landed if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
        if not found or not EAL.rename_asset(found[0], final):
            bad.append(f"{name}: import produced {landed}")
            continue
        mesh = unreal.load_asset(final)
        mesh.set_material(0, mat)
        set_collision(mesh, r["id"] in block)
        EAL.save_loaded_asset(mesh)
        box = mesh.get_bounding_box()
        if box.min.z < -3.0 or box.max.z <= box.min.z:
            bad.append(f"{name}: bounds z {box.min.z:.1f}..{box.max.z:.1f} cm (not standing on 0)")
            continue
        ok += 1
    if EAL.does_directory_exist(STAGING):
        EAL.delete_directory(STAGING)  # Interchange's placeholder materials: every mesh now uses M_Scatter
    for b in bad:
        unreal.log_error(f"ue_import_scatter: {b}")
    unreal.log(f"ue_import_scatter: {ok}/{len(meshes)} meshes imported to {DEST}")
    if bad:
        sys.exit(1)


main()
