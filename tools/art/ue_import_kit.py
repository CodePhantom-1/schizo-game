"""ue_import_kit.py — imports the mudbrick house kit FBXs into /Game/Art/Kit.

Run inside the UE editor's own Python (PythonScriptPlugin, enabled in
SchizoGame.uproject), headless:

    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
        "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
        -script=tools/art/ue_import_kit.py

Reads the kit piece list from art/assets.csv (kind == mesh_kit_piece) —
the same manifest the Blender export writes — so this script never
hardcodes the piece list twice. Imports each FBX to /Game/Art/Kit/Meshes,
creates one flat-colour UMaterial per named slot (M_MudPlaster,
M_Mudbrick, M_Timber, M_Reed) in /Game/Art/Kit/Materials, and assigns them
onto the imported mesh's material slots by NAME (so a later re-import that
swaps in a textured material — an art agent's job, per the coordinator's
brief — only needs to edit the material asset, not this script).
"""
import csv
import os
import sys

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ASSETS_CSV = os.path.join(REPO_ROOT, "art", "assets.csv")
DEST_MESH_PATH = "/Game/Art/Kit/Meshes"
DEST_MAT_PATH = "/Game/Art/Kit/Materials"

# Flat placeholder colours (linear RGB) — matches kit_common.py's MAT_DEFS
# base colours so the in-engine look matches the Blender review render
# until an art agent swaps in PBR textures.
MAT_COLORS = {
    "M_MudPlaster": (0.76, 0.62, 0.42),
    "M_Mudbrick": (0.55, 0.40, 0.27),
    "M_Timber": (0.30, 0.19, 0.10),
    "M_Reed": (0.62, 0.55, 0.30),
}


def kit_pieces():
    rows = []
    with open(ASSETS_CSV, newline="") as f:
        for row in csv.DictReader(f):
            if row["kind"] == "mesh_kit_piece":
                rows.append(row)
    return rows


def make_flat_material(name, rgb):
    """Creates (or reuses) a simple unlit-base-colour material asset."""
    asset_path = f"{DEST_MAT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.load_asset(asset_path)

    factory = unreal.MaterialFactoryNew()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, DEST_MAT_PATH, unreal.Material, factory)

    const_expr = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -300, 0
    )
    const_expr.set_editor_property(
        "constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        const_expr, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def import_fbx(fbx_rel_path, mesh_id):
    fbx_path = os.path.join(REPO_ROOT, fbx_rel_path)
    if not os.path.exists(fbx_path):
        unreal.log_error(f"missing FBX: {fbx_path} — run tools/art/house_kit.py first")
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
    fbx_options.import_materials = False  # we assign our own flat materials by slot name
    fbx_options.import_textures = False
    fbx_options.static_mesh_import_data.combine_meshes = True
    task.options = fbx_options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not unreal.EditorAssetLibrary.does_asset_exist(dest_asset):
        unreal.log_error(f"import failed: {mesh_id}")
        return None
    return unreal.load_asset(dest_asset)


def assign_materials(static_mesh, materials):
    if static_mesh is None:
        return
    for i, slot in enumerate(static_mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name)
        mat = materials.get(slot_name)
        if mat is not None:
            unreal.EditorStaticMeshLibrary.set_material(static_mesh, i, mat)


def main():
    unreal.log("ue_import_kit: importing mudbrick house kit")
    unreal.EditorAssetLibrary.make_directory(DEST_MESH_PATH)
    unreal.EditorAssetLibrary.make_directory(DEST_MAT_PATH)

    materials = {name: make_flat_material(name, rgb) for name, rgb in MAT_COLORS.items()}

    imported = 0
    for row in kit_pieces():
        mesh = import_fbx(row["file"], row["id"])
        if mesh is not None:
            assign_materials(mesh, materials)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            imported += 1

    unreal.log(f"ue_import_kit: {imported} kit pieces imported to {DEST_MESH_PATH}")
    if imported == 0:
        sys.exit(1)


main()
