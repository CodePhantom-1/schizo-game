"""ue_import_characters.py — imports the fetched CC0 characters (.gltf,
Quaternius Ultimate Animated Character Pack) into /Game/Art/Characters so
ASimNpc / ASimCharacter can LoadObject them at runtime (replacing the
cylinder + sphere programmer art, D-023 low-poly direction).

Run inside the UE editor's own Python (PythonScriptPlugin +
EditorScriptingUtilities, enabled in SchizoGame.uproject), headless:

    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
        "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
        -script=tools/art/ue_import_characters.py

(from the repo root, after `python3 tools/art/fetch_characters.py` — see
tools/art/README.md. UNTESTED by the art agent: it writes into the live
project Content, so the coordinator runs it; the runtime code degrades to
cylinders if this never runs.)

Reads the character list from art/assets.csv (kind == character_model) —
the same manifest fetch_characters.py writes — so the cast is defined once.

Per character (asset id Player / Npc0..Npc8): imports the .gltf via
AssetImportTask (glTF goes through Interchange in 5.8; each file carries
mesh + skeleton + 17 embedded animations, flat vertex colours, no
textures). Each import lands in a private staging folder
(/Game/Art/Characters/_staging/<id>) so ten imports can never collide on
auto-generated names, then the script NORMALIZES the layout the C++ side
expects:

    /Game/Art/Characters/<id>.<id>            the SkeletalMesh
    /Game/Art/Characters/<id>/Anims/Walk.Walk the walking loop
    /Game/Art/Characters/<id>/Anims/Idle.Idle the standing loop

(Skeleton / physics asset / materials stay wherever the importer put them
under _staging — references are by object, not path.) Idempotent: deletes
the previous <id> assets before importing.
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
DEST_ROOT = "/Game/Art/Characters"


def character_rows():
    rows = []
    if not os.path.exists(ASSETS_CSV):
        unreal.log_error(f"missing manifest: {ASSETS_CSV} — run tools/art/fetch_characters.py first")
        return rows
    with open(ASSETS_CSV, newline="") as f:
        for row in csv.DictReader(f):
            if row["kind"] == "character_model":
                rows.append(row)
    return rows


def _delete_if_exists(asset_or_dir_path, is_directory=False):
    if is_directory:
        if unreal.EditorAssetLibrary.does_directory_exist(asset_or_dir_path):
            unreal.EditorAssetLibrary.delete_directory(asset_or_dir_path)
    else:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_or_dir_path):
            unreal.EditorAssetLibrary.delete_asset(asset_or_dir_path)


def import_character(asset_id, gltf_path):
    """Import one glTF and normalize the result. Returns True when the
    SkeletalMesh (or at least a StaticMesh) sits at <id>.<id>."""
    final_mesh = f"{DEST_ROOT}/{asset_id}"
    anims_dir = f"{DEST_ROOT}/{asset_id}/Anims"
    staging = f"{DEST_ROOT}/_staging/{asset_id}"

    _delete_if_exists(final_mesh)
    _delete_if_exists(f"{DEST_ROOT}/{asset_id}", is_directory=True)  # Anims/ etc.
    _delete_if_exists(staging, is_directory=True)

    task = unreal.AssetImportTask()
    task.filename = gltf_path
    task.destination_path = staging
    task.destination_name = asset_id
    task.automated = True          # headless: no import dialog
    task.save = True
    task.replace_existing = True
    # No task.options: glTF routes through Interchange (5.8), whose generic
    # pipeline imports the skeletal mesh, skeleton and the embedded
    # animations with editor defaults. The FBX FbxImportUI pattern from
    # ue_import_kit.py does not apply to Interchange formats. (FBX fallback
    # recipe in tools/art/README.md if glTF skeletal import misbehaves.)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    # Discover what actually landed, then normalize names/paths.
    names = unreal.EditorAssetLibrary.list_assets(staging, recursive=True)
    if not names:
        unreal.log_error(f"{asset_id}: import produced no assets under {staging}")
        return False

    skel_meshes, static_meshes, anim_seqs = [], [], []
    for obj_path in names:
        asset = unreal.load_asset(obj_path)
        if asset is None:
            continue
        if isinstance(asset, unreal.SkeletalMesh):
            skel_meshes.append(obj_path)
        elif isinstance(asset, unreal.StaticMesh):
            static_meshes.append(obj_path)
        elif isinstance(asset, unreal.AnimSequence):
            anim_seqs.append(obj_path)

    if not skel_meshes and not static_meshes:
        unreal.log_error(f"{asset_id}: no mesh asset produced (got {len(names)} assets: "
                         f"{[os.path.basename(str(n)) for n in names][:6]}...)")
        return False

    mesh_src = (skel_meshes or static_meshes)[0]
    if str(mesh_src) != final_mesh:
        if not unreal.EditorAssetLibrary.rename_asset(str(mesh_src), final_mesh):
            unreal.log_error(f"{asset_id}: could not rename mesh {mesh_src} -> {final_mesh}")
            return False

    if not skel_meshes:
        unreal.log_warning(f"{asset_id}: imported as STATIC mesh (no skeleton) — "
                           f"C++ falls back to the BodyMesh component path")
        return True

    def anim_stem(obj_path):
        """Comparable name: drop any ".AssetName" suffix list_assets might
        carry and the "<id>_" prefix Interchange puts on imported names —
        "Npc0_Walk_Carry" and "Walk_Carry" both reduce to "walk_carry"."""
        n = os.path.basename(str(obj_path)).lower()
        n = n.rsplit(".", 1)[0]
        if n.startswith(asset_id.lower() + "_"):
            n = n[len(asset_id) + 1:]
        return n

    def _pick(patterns, exclude=()):
        """First anim matching a pattern (exact stem preferred) without any
        excluded word in the stem."""
        lowered = [(p, anim_stem(p)) for p in anim_seqs]
        for pat in patterns:
            exact = [p for p, n in lowered if n == pat]
            if exact:
                return sorted(exact)[0]
        for pat in patterns:
            prefix = sorted(p for p, n in lowered
                            if n.startswith(pat)
                            and not any(x in n for x in exclude))
            if prefix:
                return prefix[0]
        return None

    # Walk_Carry / Run_Carry are loaded-carrier strides — plain Walk/Idle
    # are the loops the street uses (Quaternius ships both).
    walk = _pick(["walk"], exclude=("carry", "run"))
    idle = _pick(["idle"], exclude=("carry",))
    unreal.EditorAssetLibrary.make_directory(anims_dir)
    for src, dst_name in ((walk, "Walk"), (idle, "Idle")):
        if src is None:
            unreal.log_warning(f"{asset_id}: no {dst_name} animation found "
                               f"(has {[os.path.basename(str(a)) for a in anim_seqs]})")
            continue
        unreal.EditorAssetLibrary.rename_asset(str(src), f"{anims_dir}/{dst_name}")

    return True


def main():
    unreal.log("ue_import_characters: importing the CC0 character cast to "
               f"{DEST_ROOT} (SimNpc/SimCharacter LoadObject these paths)")
    rows = character_rows()
    if not rows:
        unreal.log_error("no character_model rows in the manifest — nothing to import")
        sys.exit(1)

    unreal.EditorAssetLibrary.make_directory(DEST_ROOT)
    imported = 0
    for row in rows:
        gltf = os.path.join(REPO_ROOT, row["file"])
        if not os.path.exists(gltf):
            unreal.log_error(f"missing glTF: {gltf} — run tools/art/fetch_characters.py first")
            continue
        if import_character(row["id"], gltf):
            imported += 1

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False)
    # _staging keeps skeletons/physics/materials the meshes reference —
    # do NOT delete it; it is the characters' machinery closet.
    unreal.log(f"ue_import_characters: {imported}/{len(rows)} characters imported to {DEST_ROOT}")
    if imported < len(rows):
        sys.exit(1)


main()
