"""ue_import_fauna.py — the animals (tools/art/fauna_variants.py) and birds (tools/art/birds_gen.py) into
/Game/Art/Fauna, and M_Bird. Headless, from the repo root (the script path must be absolute):

    <UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "%CD%/unreal/SchizoGame.uproject" \
        -run=pythonscript -script="%CD%/tools/art/ue_import_fauna.py"

Skeletal animals, the layout USimFaunaSubsystem loads (like ue_import_characters.py's):
    /Game/Art/Fauna/SK_<id>                         the SkeletalMesh
    /Game/Art/Fauna/<id>/Anims/{Idle,Walk,Run,Eat,Sleep}   one clip per role
    /Game/Art/Fauna/<id>/...                        the import's skeleton, materials and source clips (kept)
A role the base has no clip for takes the nearest one (Walk <- Idle for the farm sheep and pig, Run <-
Gallop, Eat <- Eating, Sleep <- a head-low idle, else Idle).
Birds and the carp: static meshes /Game/Art/Fauna/SM_Bird_<id> / SM_Fish_carp with M_Bird: BaseColor =
VertexColor.rgb ^ 2.2 (the mesh build stores it sRGB-encoded), two-sided, instanced; World Position
Offset = up x sin(Time x FlapHz x 2pi + PerInstanceRandom x 2pi) x 25 cm x VertexColor.a, FlapHz from
per-instance custom data 0 (0: wings still — the waders and the swimmers).
"""
import csv
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
GEN = os.path.join(REPO, "art", "generated", "fauna")
DEST = "/Game/Art/Fauna"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
ROLES = {
    "Idle": ["idle"],
    "Walk": ["walk", "idle"],
    "Run": ["gallop", "walk", "idle"],
    "Eat": ["eating", "idle_headlow", "idle_2_headlow", "idle"],
    "Sleep": ["idle_headlow", "idle_2_headlow", "idle"],
}


def rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def import_to(path, dest, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return [str(p) for p in EAL.list_assets(dest, recursive=True)]


def wipe(path, directory=False):
    if directory and EAL.does_directory_exist(path):
        EAL.delete_directory(path)
    elif not directory and EAL.does_asset_exist(path):
        EAL.delete_asset(path)


def import_animal(mid):
    glb = os.path.join(GEN, f"SK_{mid}.glb")
    if not os.path.exists(glb):
        return f"no GLB (run fauna_variants.py)"
    final = f"{DEST}/SK_{mid}"
    # The import lands in the animal's own folder and stays there: the skeleton, physics asset, materials
    # and source clips the mesh references live in it (deleting it breaks the mesh).
    home = f"{DEST}/{mid}"
    wipe(final)
    wipe(home, directory=True)
    landed = import_to(glb, home, f"SK_{mid}")
    skel, anims = [], []
    for p in landed:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            skel.append(p)
        elif isinstance(a, unreal.AnimSequence):
            anims.append(p)
    if not skel:
        return f"no skeletal mesh (got {len(landed)} assets)"
    if not EAL.rename_asset(skel[0].split(".")[0], final):
        return f"could not move the mesh to {final}"

    def stem(p):
        n = os.path.basename(p).split(".")[0].lower()
        for pre in (f"sk_{mid}", mid):  # Interchange names clips "<mesh><Clip>" (SK_catIdle), maybe with "_"
            if n.startswith(pre):
                n = n[len(pre):].lstrip("_")
                break
        return n
    by_stem = {stem(p): p.split(".")[0] for p in anims}
    missing = []
    for role, wants in ROLES.items():
        src = next((by_stem[w] for w in wants if w in by_stem), None)
        if src is None:
            missing.append(role)
            continue
        dst = f"{DEST}/{mid}/Anims/{role}"
        if not EAL.duplicate_asset(src, dst) or not EAL.save_asset(dst, only_if_is_dirty=False):
            missing.append(role)  # a duplicate lives in memory until saved (the commandlet saves nothing itself)
    if missing:
        return f"no clip for {missing} (has {sorted(by_stem)})"
    return None


def bird_material():
    path = f"{DEST}/M_Bird"
    wipe(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Bird", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    def E(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def C(a, out, b, into):  # a pin that does not exist connects nothing silently: fail loudly
        if not MEL.connect_material_expressions(a, out, b, into):
            raise RuntimeError(f"M_Bird: could not connect {a.get_name()}.{out!r} -> {b.get_name()}.{into!r}")

    vc = E(unreal.MaterialExpressionVertexColor, -900, 0)
    decode = E(unreal.MaterialExpressionPower, -700, 0, const_exponent=2.2)
    C(vc, "", decode, "Base")
    hz = E(unreal.MaterialExpressionPerInstanceCustomData, -1300, 400, data_index=0)
    t = E(unreal.MaterialExpressionTime, -1300, 300)
    th = E(unreal.MaterialExpressionMultiply, -1150, 350)
    C(t, "", th, "A")
    C(hz, "", th, "B")
    rnd = E(unreal.MaterialExpressionPerInstanceRandom, -1300, 500)
    arg = E(unreal.MaterialExpressionAdd, -1000, 400)
    C(th, "", arg, "A")
    C(rnd, "", arg, "B")
    sine = E(unreal.MaterialExpressionSine, -850, 400, period=1.0)  # sin(2pi x): cycles per second
    C(arg, "", sine, "")
    amp = E(unreal.MaterialExpressionMultiply, -700, 450, const_b=25.0)
    C(sine, "", amp, "A")
    wing = E(unreal.MaterialExpressionMultiply, -550, 450)
    C(amp, "", wing, "A")
    C(vc, "A", wing, "B")
    up = E(unreal.MaterialExpressionConstant3Vector, -550, 600, constant=unreal.LinearColor(0, 0, 1, 1))
    wpo = E(unreal.MaterialExpressionMultiply, -400, 500)
    C(wing, "", wpo, "A")
    C(up, "", wpo, "B")
    rough = E(unreal.MaterialExpressionConstant, -500, 200, r=0.85)
    MEL.connect_material_property(decode, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def import_static(name, mat):
    glb = os.path.join(GEN, f"{name}.glb")
    if not os.path.exists(glb):
        return "no GLB (run birds_gen.py)"
    final = f"{DEST}/{name}"
    staging = f"{DEST}/_staging/{name}"
    wipe(final)
    wipe(staging, directory=True)
    landed = import_to(glb, staging, name)
    mesh = next((p for p in landed if isinstance(unreal.load_asset(p), unreal.StaticMesh)), None)
    if mesh is None or not EAL.rename_asset(mesh.split(".")[0], final):
        return f"import produced {landed}"
    sm = unreal.load_asset(final)
    sm.set_material(0, mat)
    body = sm.get_editor_property("body_setup")
    if body is not None:  # birds are never in the way
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    EAL.save_loaded_asset(sm)
    return None


def main():
    used = {r["model"] for r in rows(os.path.join(REPO, "db", "canon", "fauna.csv")) if r["model"]}
    models = [m for m in rows(os.path.join(REPO, "art", "fauna_models.csv")) if m["id"] in used]
    EAL.make_directory(DEST)
    mat = bird_material()
    ok, bad = 0, []
    for m in models:
        if m["source"] == "code":
            name = f"SM_Fish_{m['file']}" if m["file"] == "carp" else f"SM_Bird_{m['file']}"
            problem = import_static(name, mat)
        else:
            problem = import_animal(m["id"])
        if problem:
            bad.append(f"{m['id']}: {problem}")
        else:
            ok += 1
    wipe(f"{DEST}/_staging", directory=True)  # the birds' placeholder materials only
    for b in bad:
        unreal.log_error(f"ue_import_fauna: {b}")
    unreal.log(f"ue_import_fauna: {ok}/{len(models)} animals and birds imported to {DEST}")
    if bad:
        sys.exit(1)


main()
