"""ue_import_kit.py — imports the mudbrick house kit FBXs into /Game/Art/Kit.

Run inside the UE editor's own Python (PythonScriptPlugin +
EditorScriptingUtilities, enabled in SchizoGame.uproject), headless:

    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
        "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
        -script=tools/art/ue_import_kit.py

(from the repo root, after the Blender kit step + fetch_textures.py — see
tools/art/README.md.)

Reads the kit piece list from art/assets.csv (kind == mesh_kit_piece) —
the same manifest the Blender export writes — so this script never
hardcodes the piece list twice. Imports each FBX to /Game/Art/Kit/Meshes
WITH its embedded CC0 surface maps (import_materials/import_textures on:
the FBX carries the colour maps, UE rebuilds a textured material per named
slot), pins a matte roughness constant on those rebuilt materials (the
FBX export cannot carry Blender's roughness-map wiring, and mudbrick is
not glossy), and additionally authors two tiled materials as real assets
in /Game/Art/Kit/Materials via MaterialEditingLibrary:

  - M_Ground      — Ground109 colour map, TextureCoordinate tiling 30x30,
                    roughness 0.95 — worn by the runtime ground cube
                    (SimStreetBuilder::BuildGroundAndLighting).
  - M_PlasterWall — Ground087 colour map, tiling 8x8, roughness 0.95 —
                    spare, for runtime-coloured fallback surfaces (door
                    leafs & co. stay flat colours this wave).

Everything here is idempotent: existing assets are reused, FBX re-imports
replace in place.
"""
import csv
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # UE's pythonscript runner does not always set __file__
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
sys.path.insert(0, _HERE)
import fetch_textures as ft  # noqa: E402 -- plain python, no bpy, safe in UE

REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
ASSETS_CSV = os.path.join(REPO_ROOT, "art", "assets.csv")
DEST_MESH_PATH = "/Game/Art/Kit/Meshes"
DEST_MAT_PATH = "/Game/Art/Kit/Materials"
DEST_TEX_PATH = "/Game/Art/Kit/Textures"

# The two authored tiled materials (name -> CC0 colour map + tiling +
# roughness). Tiling ~30 on the ground cube: the cube spans the whole
# quarter (~200 m + margins) with engine-cube 0-1 UVs per face, so 30
# repeats land near Ground109's documented 3 m tile at street scale.
AUTHORED_MATS = [
    ("M_Ground", "Ground109", 30.0, 0.95,
     "packed-earth ground under the whole quarter (SimStreetBuilder ground cube)"),
    ("M_PlasterWall", "Ground087", 8.0, 0.95,
     "mud-plaster surface for flat-coloured fallbacks (door leafs etc., later waves)"),
]

# Sun-dried mudbrick/mud plaster is matte; the FBX-imported materials get
# UE's default 0.5 roughness without this.
KIT_MAT_ROUGHNESS = 0.95

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


def import_color_texture(asset_id):
    """Import (or reuse) an ambientCG colour map as a Texture2D asset.

    Idempotent: an existing T_<asset>_Color is loaded, not re-imported.
    """
    src = ft.map_path(asset_id, "Color")
    if not os.path.exists(src):
        unreal.log_error(f"missing source texture: {src} — run tools/art/fetch_textures.py")
        return None
    name = f"T_{asset_id}_Color"
    dest = f"{DEST_TEX_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        return unreal.load_asset(dest)

    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = DEST_TEX_PATH
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not unreal.EditorAssetLibrary.does_asset_exist(dest):
        unreal.log_error(f"texture import failed: {name}")
        return None
    tex = unreal.load_asset(dest)
    unreal.log(f"ue_import_kit: imported {name} from {os.path.relpath(src, REPO_ROOT)}")
    return tex


def make_tiled_material(name, texture, tiling, roughness):
    """Create (or reuse) a lit material: TextureSample(colour map) driven by
    a TextureCoordinate with `tiling` repeats -> BaseColor, scalar Constant
    -> Roughness. Idempotent: an existing asset is returned untouched."""
    asset_path = f"{DEST_MAT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"ue_import_kit: {name} already exists — reusing")
        return unreal.load_asset(asset_path)

    factory = unreal.MaterialFactoryNew()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, DEST_MAT_PATH, unreal.Material, factory)
    if mat is None:
        unreal.log_error(f"could not create material {name}")
        return None

    tex = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSample, -400, 0)
    tex.set_editor_property("texture", texture)

    tcoord = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    # UE5's python exposes UTiling/VTiling; try both spellings so neither
    # the snake_case nor the raw property name build breaks the import.
    for prop in ("u_tiling", "UTiling"):
        try:
            tcoord.set_editor_property(prop, tiling)
            break
        except Exception:
            continue
    for prop in ("v_tiling", "VTiling"):
        try:
            tcoord.set_editor_property(prop, tiling)
            break
        except Exception:
            continue
    if not unreal.MaterialEditingLibrary.connect_material_expressions(tcoord, "", tex, "UVs"):
        unreal.log_warning(f"{name}: TextureCoordinate->TextureSample link failed — "
                           f"tiling falls back to 1 (check this material by hand)")

    rough = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", roughness)

    unreal.MaterialEditingLibrary.connect_material_property(
        tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"ue_import_kit: authored {name} (tiling {tiling:g}x{tiling:g}, "
               f"roughness {roughness}) in {DEST_MAT_PATH}")
    return mat


def matte_imported_materials(meshes, roughness=KIT_MAT_ROUGHNESS):
    """Pin a matte roughness constant on every material the FBX import
    rebuilt for the kit meshes. Blender's FBX exporter embeds the colour
    maps but cannot carry the roughness-map wiring, so UE's rebuilt
    materials would sit at the default 0.5 gloss. Same MaterialEditingLibrary
    call pattern as make_tiled_material; per-material failures are logged
    and skipped, never fatal (the import result stays usable)."""
    seen = set()
    for mesh in meshes:
        if mesh is None:
            continue
        for slot in mesh.get_editor_property("static_materials"):
            mat = slot.material_interface
            if mat is None or not isinstance(mat, unreal.Material):
                continue
            mat_name = mat.get_name()
            if mat_name in seen:
                continue
            seen.add(mat_name)
            try:
                rough = unreal.MaterialEditingLibrary.create_material_expression(
                    mat, unreal.MaterialExpressionConstant, -400, 250)
                rough.set_editor_property("r", roughness)
                unreal.MaterialEditingLibrary.connect_material_property(
                    rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
                unreal.MaterialEditingLibrary.recompile_material(mat)
                unreal.EditorAssetLibrary.save_loaded_asset(mat)
                unreal.log(f"ue_import_kit: pinned matte roughness {roughness} on {mat_name}")
            except Exception as exc:  # noqa: BLE001 — degrade gracefully, keep importing
                unreal.log_warning(f"ue_import_kit: could not set roughness on {mat_name}: {exc}")


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
    # path_mode=COPY + embed_textures): import them as real materials —
    # SimStreetBuilder's runtime colours are only a fallback for null slots.
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
    unreal.log("ue_import_kit: importing mudbrick house kit (FBX materials + embedded "
               "CC0 maps; runtime slot colours stay as SimStreetBuilder's fallback)")
    pieces = kit_pieces()
    if not pieces:
        unreal.log_error("no mesh_kit_piece rows in the manifest — nothing to import")
        sys.exit(1)

    unreal.EditorAssetLibrary.make_directory(DEST_MESH_PATH)
    unreal.EditorAssetLibrary.make_directory(DEST_MAT_PATH)
    unreal.EditorAssetLibrary.make_directory(DEST_TEX_PATH)

    imported_meshes = []
    for row in pieces:
        mesh = import_fbx(row["file"], row["id"])
        if mesh is not None:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            imported_meshes.append(mesh)

    # The FBX-rebuilt kit materials are glossier than mud — pin them matte.
    matte_imported_materials(imported_meshes)

    # The two authored tiled surface materials (M_Ground is LoadObject'd by
    # SimStreetBuilder for the ground cube; M_PlasterWall is a spare for
    # fallback surfaces).
    for name, asset_id, tiling, roughness, use in AUTHORED_MATS:
        texture = import_color_texture(asset_id)
        if texture is None or make_tiled_material(name, texture, tiling, roughness) is None:
            unreal.log_error(f"could not author {name} ({use}) — "
                             f"run tools/art/fetch_textures.py and retry")
            sys.exit(1)

    unreal.log(f"ue_import_kit: {len(imported_meshes)}/{len(pieces)} kit pieces imported to "
               f"{DEST_MESH_PATH}; authored materials in {DEST_MAT_PATH}")
    if len(imported_meshes) < len(pieces):
        sys.exit(1)


main()
