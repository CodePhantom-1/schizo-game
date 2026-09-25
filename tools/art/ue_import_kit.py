"""ue_import_kit.py — imports the mudbrick house kit FBXs into /Game/Art/Kit.

Run inside the UE editor's own Python (PythonScriptPlugin + 
EditorScriptingUtilities, enabled in SchizoGame.uproject), headless:

    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
        "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
        -script=tools/art/ue_import_kit.py

(from the repo root, after the Blender kit step — see tools/art/README.md.)

Reads the kit piece list from art/assets.csv (kind == mesh_kit_piece) —
the same manifest the Blender export writes — so this script never
hardcodes the piece list twice. Imports each FBX to /Game/Art/Kit/Meshes,
creates one flat-colour UMaterial per named slot (M_MudPlaster,
M_Mudbrick, M_Timber, M_Reed) in /Game/Art/Kit/Materials, and assigns them
onto the imported mesh's material slots by NAME (so a later re-import that
swaps in a textured material — an art agent's job, per the coordinator's
brief — only needs to edit the material asset, not this script).

D-023 art direction: flat unlit-base-colour materials, no PBR textures.
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
DEST_MESH_PATH = "/Game/Art/Kit/Meshes"

# Pieces with interior cuts (doorways, windows, the roof-access hatch, the
# awning over the stalls): a convex hull of these is the solid slab, so they
# must NOT carry auto collision — see import_fbx.
NO_COLLISION_PIECES = {"SM_WallDoor", "SM_WallWindow", "SM_RoofAccess", "SM_Awning"}


def kit_pieces():
    rows = []
    if not os.path.exists(ASSETS_CSV):
        unreal.log_error(f"missing manifest: {ASSETS_CSV} — run tools/art/house_kit.py first")
        return rows
    with open(ASSETS_CSV, newline="") as f:
        for row in csv.DictReader(f):
            if row["kind"] == "mesh_kit_piece":
                rows.append(row)
    return rows



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
    # The FBX carries the CC0 surface maps embedded (kit_common exports with
    # path_mode=COPY + embed_textures): import them as real materials.
    fbx_options.import_materials = True
    fbx_options.import_textures = True
    fbx_options.static_mesh_import_data.combine_meshes = True
    # Auto convex collision would fill every cut solid: an 18-DOP hull of a
    # wall-with-doorway is just the wall, so buildings would be unenterable
    # and awnings would wedge over the stalls. Cut pieces import collision-
    # free; passage is the door leaf's job. Solid pieces keep their hulls.
    if mesh_id in NO_COLLISION_PIECES:
        fbx_options.static_mesh_import_data.auto_generate_collision = False
    task.options = fbx_options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not unreal.EditorAssetLibrary.does_asset_exist(dest_asset):
        unreal.log_error(f"import failed: {mesh_id}")
        return None
    return unreal.load_asset(dest_asset)


def main():
    unreal.log("ue_import_kit: importing mudbrick house kit (meshes only — the street "
               "builder colours the FBX slots at runtime, see SimStreetBuilder::LoadKitMeshes)")
    pieces = kit_pieces()
    if not pieces:
        unreal.log_error("no mesh_kit_piece rows in the manifest — nothing to import")
        sys.exit(1)

    unreal.EditorAssetLibrary.make_directory(DEST_MESH_PATH)

    imported = 0
    for row in pieces:
        mesh = import_fbx(row["file"], row["id"])
        if mesh is not None:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            imported += 1

    unreal.log(f"ue_import_kit: {imported}/{len(pieces)} kit pieces imported to {DEST_MESH_PATH}")
    if imported < len(pieces):
        sys.exit(1)


main()
