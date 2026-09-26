"""ue_make_fx_materials.py — the weather's cards (tools/art/fx_gen.py) and their materials into /Game/Art/FX.
Headless, from the repo root (the script path must be absolute):

    <UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "%CD%/unreal/SchizoGame.uproject" \
        -run=pythonscript -script="%CD%/tools/art/ue_make_fx_materials.py"

All three: translucent, unlit, two-sided, instanced; the motion is World Position Offset, the fade MPC_World.
  M_Rain   pale streaks falling 30 m through the camera's box, a little wind slant; opacity 0.55 x Rain
  M_Dust   ochre motes drifting 30 m across the box on the wind; opacity 0.5 x Dust
  M_Smoke  grey puffs rising 6 m and drifting 1.5 m as they fade, a soft round card; opacity 0.8 x
           per-instance custom data 0 (the source is lit: ASimAtmosphere's schedule)
Every instance's phase is PerInstanceRandom, so a field never moves in step.
"""
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
GEN = os.path.join(REPO, "art", "generated", "fx")
DEST = "/Game/Art/FX"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def wipe(path, directory=False):
    if directory and EAL.does_directory_exist(path):
        EAL.delete_directory(path)
    elif not directory and EAL.does_asset_exist(path):
        EAL.delete_asset(path)


def import_card(name, mat):
    final = f"{DEST}/SM_FX_{name}"
    staging = f"{DEST}/_staging/{name}"
    wipe(final)
    wipe(staging, directory=True)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(GEN, f"SM_FX_{name}.glb")
    task.destination_path = staging
    task.destination_name = f"SM_FX_{name}"
    task.automated = True
    task.save = True
    task.replace_existing = True
    if not os.path.exists(task.filename):
        raise RuntimeError(f"{task.filename} missing: run tools/art/fx_gen.py")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    landed = [str(p) for p in EAL.list_assets(staging, recursive=True)]
    mesh = next((p for p in landed if isinstance(unreal.load_asset(p), unreal.StaticMesh)), None)
    if mesh is None or not EAL.rename_asset(mesh.split(".")[0], final):
        raise RuntimeError(f"SM_FX_{name}: import produced {landed}")
    sm = unreal.load_asset(final)
    sm.set_material(0, mat)  # before the staging folder (and its placeholder material) goes
    body = sm.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    return sm


def material(name, mpc, build):
    path = f"{DEST}/{name}"
    wipe(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    def E(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def C(a, out, b, into):  # a pin that does not exist connects nothing silently: fail loudly
        if not MEL.connect_material_expressions(a, out, b, into):
            raise RuntimeError(f"{name}: could not connect {a.get_name()}.{out!r} -> {b.get_name()}.{into!r}")

    def param(pname, x, y):
        return E(unreal.MaterialExpressionCollectionParameter, x, y, collection=mpc, parameter_name=pname)

    def phase(speed, x, y):
        """frac(Time x speed + PerInstanceRandom): each instance's 0..1 cycle."""
        t = E(unreal.MaterialExpressionTime, x, y)
        ts = E(unreal.MaterialExpressionMultiply, x + 120, y, const_b=speed)
        C(t, "", ts, "A")
        rnd = E(unreal.MaterialExpressionPerInstanceRandom, x, y + 80)
        add = E(unreal.MaterialExpressionAdd, x + 240, y)
        C(ts, "", add, "A")
        C(rnd, "", add, "B")
        fr = E(unreal.MaterialExpressionFrac, x + 360, y)
        C(add, "", fr, "")
        return fr

    def vec3(a, b, c, x, y):
        ab = E(unreal.MaterialExpressionAppendVector, x, y)
        C(a, "", ab, "A")
        C(b, "", ab, "B")
        abc = E(unreal.MaterialExpressionAppendVector, x + 120, y)
        C(ab, "", abc, "A")
        C(c, "", abc, "B")
        return abc

    colour, opacity, wpo = build(E, C, param, phase, vec3)
    MEL.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def rain(E, C, param, phase, vec3):
    col = E(unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(0.55, 0.6, 0.66, 1))
    op = E(unreal.MaterialExpressionMultiply, -400, 200, const_b=0.55)
    C(param("Rain", -600, 200), "", op, "A")
    ph = phase(0.9, -1400, 400)
    fall = E(unreal.MaterialExpressionMultiply, -900, 400, const_b=-3000.0)  # down through the 30 m box
    C(ph, "", fall, "A")
    slant = E(unreal.MaterialExpressionMultiply, -900, 520, const_b=-300.0)  # the wind leans it
    C(ph, "", slant, "A")
    zero = E(unreal.MaterialExpressionConstant, -900, 620, r=0.0)
    return col, op, vec3(slant, zero, fall, -700, 450)


def dust(E, C, param, phase, vec3):
    col = E(unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(0.55, 0.4, 0.22, 1))
    op = E(unreal.MaterialExpressionMultiply, -400, 200, const_b=0.5)
    C(param("Dust", -600, 200), "", op, "A")
    ph = phase(0.12, -1400, 400)
    across = E(unreal.MaterialExpressionMultiply, -900, 400, const_b=3000.0)  # across the box on the wind
    C(ph, "", across, "A")
    lift = E(unreal.MaterialExpressionSine, -1000, 520, period=1.0)
    C(ph, "", lift, "")
    bob = E(unreal.MaterialExpressionMultiply, -900, 520, const_b=60.0)
    C(lift, "", bob, "A")
    zero = E(unreal.MaterialExpressionConstant, -900, 620, r=0.0)
    return col, op, vec3(across, zero, bob, -700, 450)


def smoke(E, C, param, phase, vec3):
    col = E(unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(0.45, 0.43, 0.4, 1))
    ph = phase(0.12, -1600, 400)
    # A soft round card: 1 - distance of the UV from the centre x 2.
    uv = E(unreal.MaterialExpressionTextureCoordinate, -1400, 100)
    centre = E(unreal.MaterialExpressionConstant2Vector, -1400, 200, r=0.5, g=0.5)
    dist = E(unreal.MaterialExpressionDistance, -1250, 150)
    C(uv, "", dist, "A")
    C(centre, "", dist, "B")
    x2 = E(unreal.MaterialExpressionMultiply, -1100, 150, const_b=2.0)
    C(dist, "", x2, "A")
    soft = E(unreal.MaterialExpressionOneMinus, -950, 150)
    C(x2, "", soft, "")
    round_ = E(unreal.MaterialExpressionSaturate, -850, 150)
    C(soft, "", round_, "")
    fade = E(unreal.MaterialExpressionOneMinus, -1100, 300)  # thins as it rises
    C(ph, "", fade, "")
    lit = E(unreal.MaterialExpressionPerInstanceCustomData, -1100, 380, data_index=0)
    a = E(unreal.MaterialExpressionMultiply, -700, 200)
    C(round_, "", a, "A")
    C(fade, "", a, "B")
    b = E(unreal.MaterialExpressionMultiply, -550, 250)
    C(a, "", b, "A")
    C(lit, "", b, "B")
    op = E(unreal.MaterialExpressionMultiply, -400, 250, const_b=0.8)
    C(b, "", op, "A")
    rise = E(unreal.MaterialExpressionMultiply, -900, 450, const_b=600.0)
    C(ph, "", rise, "A")
    drift = E(unreal.MaterialExpressionMultiply, -900, 560, const_b=150.0)
    C(ph, "", drift, "A")
    zero = E(unreal.MaterialExpressionConstant, -900, 660, r=0.0)
    return col, op, vec3(drift, zero, rise, -700, 500)


def main():
    mpc = unreal.load_asset("/Game/Art/Scatter/MPC_World")
    if mpc is None:
        unreal.log_error("ue_make_fx_materials: MPC_World missing — run tools/art/ue_import_scatter.py first")
        sys.exit(1)
    params = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("scalar_parameters")}
    if "Rain" not in params:  # the rain's own scalar (V-B5 T2): added without touching the others' ids
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", "Rain")
        p.set_editor_property("default_value", 0.0)
        mpc.set_editor_property("scalar_parameters", list(mpc.get_editor_property("scalar_parameters")) + [p])
        EAL.save_loaded_asset(mpc)
    EAL.make_directory(DEST)
    mats = {"Rain": material("M_Rain", mpc, rain), "Dust": material("M_Dust", mpc, dust), "Smoke": material("M_Smoke", mpc, smoke)}
    for name, mat in mats.items():
        sm = import_card(name, mat)
        EAL.save_loaded_asset(sm)
    wipe(f"{DEST}/_staging", directory=True)  # only the cards' placeholder materials live there
    unreal.log("ue_make_fx_materials: SM_FX_Rain/Dust/Smoke + M_Rain/M_Dust/M_Smoke in /Game/Art/FX")


main()
