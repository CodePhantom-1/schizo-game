"""ue_import_props.py — imports the street props into /Game/Art/Props and
authors the sky dome's gradient material at /Game/Art/Sky/M_SkyDome.

Run inside the UE editor's own Python, headless (UNTESTED as written — this
wave has no compiled editor to drive; commands in tools/art/README.md):

    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
        "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
        -script=tools/art/ue_import_props.py

(from the repo root, after the Blender props step.)

Part 1: reads art/assets.csv (kind == mesh_street_prop — the manifest
tools/art/props_gen.py writes, so the piece list is never hardcoded twice)
and imports each FBX to /Game/Art/Props/Meshes, AssetImportTask pattern from
ue_import_kit.py (materials + textures import from the FBX's embedded CC0
maps). SM_SkyDome imports collision-free: its convex hull would be a solid
120 m sphere blocking the whole level.

Part 2: M_SkyDome via MaterialEditingLibrary — an UNLIT, two-sided gradient:
Lerp(horizon, zenith, clamp(AbsWorldPosition.Z / DomeRadius)) into Emissive.
DomeRadius is a ScalarParameter (default 12000 cm) so the same material
serves the kit dome and the engine-sphere fallback (SimSkyDome sets it).
Dusk-warm horizon, pale zenith (values in docs/proposals/invented-ledger-dressing.md).
"""
import csv
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # UE's pythonscript runner does not always set __file__
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
ASSETS_CSV = os.path.join(REPO_ROOT, "art", "assets.csv")
DEST_MESH_PATH = "/Game/Art/Props/Meshes"
DEST_SKY_PATH = "/Game/Art/Sky"

# No auto collision on these (see module docstring, part 1).
NO_COLLISION_PIECES = {"SM_SkyDome"}

# Sky gradient colours — see docs/proposals/invented-ledger-dressing.md.
SKY_HORIZON = unreal.LinearColor(r=0.86, g=0.58, b=0.36, a=1.0)  # dusk-warm dust
SKY_ZENITH = unreal.LinearColor(r=0.38, g=0.52, b=0.68, a=1.0)   # pale dusty blue
SKY_DOME_RADIUS_CM = 12000.0  # matches ASimSkyDome's kDomeRadiusCm


def street_props():
    rows = []
    if not os.path.exists(ASSETS_CSV):
        unreal.log_error(f"missing manifest: {ASSETS_CSV} — run tools/art/props_gen.py first")
        return rows
    with open(ASSETS_CSV, newline="") as f:
        for row in csv.DictReader(f):
            if row["kind"] == "mesh_street_prop":
                rows.append(row)
    return rows


def import_fbx(fbx_rel_path, mesh_id):
    fbx_path = os.path.join(REPO_ROOT, fbx_rel_path)
    if not os.path.exists(fbx_path):
        unreal.log_error(f"missing FBX: {fbx_path} — run tools/art/props_gen.py first")
        return None

    dest_asset = f"{DEST_MESH_PATH}/{mesh_id}"
    if unreal.EditorAssetLibrary.does_asset_exist(dest_asset):
        unreal.EditorAssetLibrary.delete_asset(dest_asset)

    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = DEST_MESH_PATH
    task.destination_name = mesh_id
    task.automated = True
    task.save = True
    task.replace_existing = True

    fbx_options = unreal.FbxImportUI()
    fbx_options.import_mesh = True
    fbx_options.import_as_skeletal = False
    # The FBX carries the CC0 surface maps embedded (kit_common exports with
    # path_mode=COPY + embed_textures): import them as real materials.
    fbx_options.import_materials = True
    fbx_options.import_textures = True
    fbx_options.static_mesh_import_data.combine_meshes = True
    if mesh_id in NO_COLLISION_PIECES:
        fbx_options.static_mesh_import_data.auto_generate_collision = False
    task.options = fbx_options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not unreal.EditorAssetLibrary.does_asset_exist(dest_asset):
        unreal.log_error(f"import failed: {mesh_id}")
        return None
    return unreal.load_asset(dest_asset)


def build_sky_material():
    """Author /Game/Art/Sky/M_SkyDome headless (MaterialEditingLibrary).

    Emissive = Lerp(A=horizon, B=zenith, Alpha=clamp( |WP.Z| / DomeRadius ))
    with Shading Model Unlit and TwoSided (the dome is seen from inside).
    """
    MEL = unreal.MaterialEditingLibrary
    unreal.EditorAssetLibrary.make_directory(DEST_SKY_PATH)
    asset_path = f"{DEST_SKY_PATH}/M_SkyDome"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_SkyDome", DEST_SKY_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        unreal.log_error("could not create M_SkyDome")
        return None
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    horizon = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 100)
    horizon.set_editor_property("constant_value", SKY_HORIZON)
    zenith = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 340)
    zenith.set_editor_property("constant_value", SKY_ZENITH)
    lerp = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -330, 220)
    MEL.connect_material_properties(horizon, "", lerp, "A")
    MEL.connect_material_properties(zenith, "", lerp, "B")

    world_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -950, 480)
    divide = MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, -730, 480)
    MEL.connect_material_properties(world_pos, "", divide, "A")
    radius = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -950, 640)
    radius.set_editor_property("parameter_name", "DomeRadius")
    radius.set_editor_property("default_value", SKY_DOME_RADIUS_CM)
    MEL.connect_material_properties(radius, "", divide, "B")
    clamp = MEL.create_material_expression(mat, unreal.MaterialExpressionClamp, -510, 480)
    MEL.connect_material_properties(divide, "", clamp, "Input")  # Min/Max default 0..1
    MEL.connect_material_properties(clamp, "", lerp, "Alpha")

    MEL.connect_material_property(mat, unreal.MaterialProperty.MP_EMISSIVE, lerp, "")
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"ue_import_props: M_SkyDome authored (unlit two-sided gradient, "
               f"DomeRadius default {SKY_DOME_RADIUS_CM} cm)")
    return mat


def main():
    unreal.log("ue_import_props: importing street props (SimPropsBuilder colours the "
               "FBX slots at runtime, see SimPropsBuilder::SpawnProp)")
    props = street_props()
    if not props:
        unreal.log_error("no mesh_street_prop rows in the manifest — nothing to import")
        sys.exit(1)

    unreal.EditorAssetLibrary.make_directory(DEST_MESH_PATH)

    imported = 0
    for row in props:
        mesh = import_fbx(row["file"], row["id"])
        if mesh is not None:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            imported += 1

    sky_ok = build_sky_material() is not None
    unreal.log(f"ue_import_props: {imported}/{len(props)} props imported to {DEST_MESH_PATH}; "
               f"sky material {'ok' if sky_ok else 'FAILED'}")
    if imported < len(props) or not sky_ok:
        sys.exit(1)


main()
