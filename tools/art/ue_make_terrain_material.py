"""ue_make_terrain_material.py — the terrain's detail material (Stage V batch 3). Imports the ground atlas
(tools/art/ground_atlas.py's T_Ground.png) to /Game/Art/Terrain/T_Ground and authors M_Terrain. Headless,
from the repo root (the script path must be absolute):

    <UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe "%CD%/unreal/SchizoGame.uproject" \
        -run=pythonscript -script="%CD%/tools/art/ue_make_terrain_material.py"

M_Terrain is Tommy's M_Flat as it renders (vertex colour x his world noise; roughness 0.92, specular 0.25) times a
tiling detail: BaseColor = lerp(VertexColor.rgb, DryColour, Dry) x Noise x Detail x 2, where
  Cell   = round(VertexColor.A x 15)                 the ground kind (ESimGround, T_Ground's cell order)
  UV     = frac(WorldPosition / 400 cm)              planar XY; on slopes steeper than 30 deg a
                                                     triplanar blend of XZ and YZ by the normal
  Detail = T_Ground((UV + (Cell % 4, floor(Cell / 4))) / 4)   mean 0.5 per cell: a flat cell = M_Flat
  Dry    = MPC_World.Wither x (Cell is Silt or Irrigated)     the drought reaches fields and silt only
           -> the detail swaps to Cracked, or Salt where a low-frequency world noise > 0.6, and the
              colour leans to the cracked field colour (salt: the salt bloom)
  Furrows: on Irrigated cells, a stripe every 90 cm along X darkens the detail by up to 12%.
T_Ground is linear (not sRGB): 0.5 must stay 0.5 so x 2 leaves Tommy's colour as it is.
"""
import os
import sys

import unreal

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # UE's pythonscript runner does not always set __file__
    _HERE = os.path.abspath(os.path.join(os.getcwd(), "tools", "art"))
REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
DEST = "/Game/Art/Terrain"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
MPC_PATH = "/Game/Art/Scatter/MPC_World"
GRID = 4
TILE_CM = 400.0
SILT, IRRIGATED, CRACKED, SALT = 0, 1, 2, 3
# M_Flat reads the raw vertex colour (sRGB-encoded bytes) as its colour: the dry targets are raw too.
CRACKED_RAW = (184 / 255, 152 / 255, 110 / 255)  # Tommy's "cracked, dry" field colour (SimEnvironment)
SALT_RAW = (232 / 255, 228 / 255, 214 / 255)     # the palette's salt_0


def import_texture():
    path = f"{DEST}/T_Ground"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(REPO, "art", "generated", "tex", "T_Ground.png")
    task.destination_path = DEST
    task.destination_name = "T_Ground"
    task.automated = True
    task.save = True
    task.replace_existing = True
    if not os.path.exists(task.filename):
        raise RuntimeError(f"{task.filename} missing: run tools/art/ground_atlas.py")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError("T_Ground did not import")
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(tex)
    return tex


def build(tex, mpc):
    path = f"{DEST}/M_Terrain"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Terrain", DEST, unreal.Material, unreal.MaterialFactoryNew())

    def E(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def C(a, out, b, into):  # a pin name that does not exist connects nothing and says nothing: fail loudly
        if not MEL.connect_material_expressions(a, out, b, into):
            raise RuntimeError(f"M_Terrain: could not connect {a.get_name()}.{out!r} -> {b.get_name()}.{into!r}")

    def op(cls, a, b, x, y, a_out="", b_out=""):
        e = E(cls, x, y)
        C(a, a_out, e, "A")
        C(b, b_out, e, "B")
        return e

    def const(v, x, y):
        return E(unreal.MaterialExpressionConstant, x, y, r=v)

    def mask(src, keep, x, y, out=""):
        e = E(unreal.MaterialExpressionComponentMask, x, y)
        for ch in "rgba":  # every channel set: the defaults are not one channel
            e.set_editor_property(ch, ch in keep)
        C(src, out, e, "")
        return e

    # --- the cell: round(VertexColor.A x 15) -> (Cell % 4, floor(Cell / 4)) ---
    vc = E(unreal.MaterialExpressionVertexColor, -2600, 0)
    a15 = E(unreal.MaterialExpressionMultiply, -2400, 100, const_b=15.0)
    C(vc, "A", a15, "A")
    cell = E(unreal.MaterialExpressionRound, -2250, 100)
    C(a15, "", cell, "")

    def cell_offset(cell_expr, x, y):
        col = op(unreal.MaterialExpressionFmod, cell_expr, const(float(GRID), x - 150, y + 60), x, y)
        d = E(unreal.MaterialExpressionDivide, x, y + 120, const_b=float(GRID))
        C(cell_expr, "", d, "A")
        row = E(unreal.MaterialExpressionFloor, x + 150, y + 120)
        C(d, "", row, "")
        return op(unreal.MaterialExpressionAppendVector, col, row, x + 300, y)

    base_off = cell_offset(cell, -2100, 100)

    # --- world-space planar UVs (cm / 400) ---
    wp = E(unreal.MaterialExpressionWorldPosition, -2600, 500)
    scaled = E(unreal.MaterialExpressionDivide, -2400, 500, const_b=TILE_CM)
    C(wp, "", scaled, "A")
    uv_xy = mask(scaled, "rg", -2250, 450)
    uv_xz = mask(scaled, "rb", -2250, 550)
    uv_yz = mask(scaled, "gb", -2250, 650)

    def sample(uv, off, x, y):
        fr = E(unreal.MaterialExpressionFrac, x, y)
        C(uv, "", fr, "")
        add = op(unreal.MaterialExpressionAdd, fr, off, x + 120, y)
        div = E(unreal.MaterialExpressionDivide, x + 240, y, const_b=float(GRID))
        C(add, "", div, "A")
        ts = E(unreal.MaterialExpressionTextureSample, x + 380, y, texture=tex,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        C(div, "", ts, "UVs")
        return ts

    s_xy = sample(uv_xy, base_off, -1800, 400)
    s_xz = sample(uv_xz, base_off, -1800, 600)
    s_yz = sample(uv_yz, base_off, -1800, 800)

    # Triplanar on slopes: XY on flat ground; past 30 deg lean to XZ/YZ by the normal.
    n = E(unreal.MaterialExpressionVertexNormalWS, -2600, 1000)
    nabs = E(unreal.MaterialExpressionAbs, -2450, 1000)
    C(n, "", nabs, "")
    nx, ny, nz = mask(nabs, "r", -2300, 950), mask(nabs, "g", -2300, 1030), mask(nabs, "b", -2300, 1110)
    nxy = op(unreal.MaterialExpressionAdd, nx, ny, -2150, 980)
    nxy_safe = E(unreal.MaterialExpressionAdd, -2050, 980, const_b=1e-4)
    C(nxy, "", nxy_safe, "A")
    side_w = op(unreal.MaterialExpressionDivide, nx, nxy_safe, -1950, 960)
    side = E(unreal.MaterialExpressionLinearInterpolate, -1300, 700)
    C(s_xz, "RGB", side, "A")  # TextureSample's colour pin is "RGB"
    C(s_yz, "RGB", side, "B")
    C(side_w, "", side, "Alpha")
    steep = E(unreal.MaterialExpressionSubtract, -2150, 1110, const_a=0.866)  # 0.866 - Nz
    C(nz, "", steep, "B")
    steep_d = E(unreal.MaterialExpressionDivide, -2050, 1110, const_b=0.2)
    C(steep, "", steep_d, "A")
    steep_t = E(unreal.MaterialExpressionSaturate, -1950, 1110)
    C(steep_d, "", steep_t, "")
    detail_base = E(unreal.MaterialExpressionLinearInterpolate, -1150, 500)
    C(s_xy, "RGB", detail_base, "A")
    C(side, "", detail_base, "B")
    C(steep_t, "", detail_base, "Alpha")

    # --- furrows on irrigated fields: a stripe every 90 cm along X, up to 12% darker ---
    is_irr = E(unreal.MaterialExpressionIf, -1300, 1300)
    C(cell, "", is_irr, "A")
    C(const(float(IRRIGATED), -1450, 1360), "", is_irr, "B")
    C(const(0.0, -1450, 1420), "", is_irr, "A > B")
    C(const(1.0, -1450, 1480), "", is_irr, "A == B")
    C(const(0.0, -1450, 1540), "", is_irr, "A < B")
    wx = mask(wp, "r", -1800, 1300)
    per = E(unreal.MaterialExpressionDivide, -1650, 1300, const_b=90.0)
    C(wx, "", per, "A")
    furrow_sin = E(unreal.MaterialExpressionSine, -1500, 1300, period=1.0)
    C(per, "", furrow_sin, "")
    furrow = E(unreal.MaterialExpressionMultiply, -1150, 1300, const_b=0.06)  # +-6% around 1
    C(furrow_sin, "", furrow, "A")
    furrow_m = op(unreal.MaterialExpressionMultiply, furrow, is_irr, -1000, 1300)
    furrow_f = E(unreal.MaterialExpressionAdd, -850, 1300, const_b=0.94)  # 0.88 .. 1.0
    C(furrow_m, "", furrow_f, "A")
    detail_furrowed = op(unreal.MaterialExpressionMultiply, detail_base, furrow_f, -700, 700)

    # --- the drought: fields and silt crack (or crust with salt) as MPC_World.Wither rises ---
    wither = E(unreal.MaterialExpressionCollectionParameter, -1800, -300, collection=mpc, parameter_name="Wither")
    field = E(unreal.MaterialExpressionIf, -1800, -150)  # Cell < 1.5: Silt or Irrigated
    C(cell, "", field, "A")
    C(const(1.5, -1950, -100), "", field, "B")
    C(const(0.0, -1950, -40), "", field, "A > B")
    C(const(0.0, -1950, 20), "", field, "A == B")
    C(const(1.0, -1950, 80), "", field, "A < B")
    dry = op(unreal.MaterialExpressionMultiply, wither, field, -1600, -250)
    noise = E(unreal.MaterialExpressionNoise, -1800, 200, scale=0.00025, quality=1, levels=3,
              output_min=0.0, output_max=1.0, turbulence=False)
    C(wp, "", noise, "World Position")  # the input's real name
    salty = E(unreal.MaterialExpressionSubtract, -1650, 200, const_b=0.6)
    C(noise, "", salty, "A")
    salty_x = E(unreal.MaterialExpressionMultiply, -1550, 200, const_b=50.0)
    C(salty, "", salty_x, "A")
    is_salt = E(unreal.MaterialExpressionSaturate, -1450, 200)
    C(salty_x, "", is_salt, "")
    dry_cell = E(unreal.MaterialExpressionAdd, -1350, 200, const_a=float(CRACKED))  # Cracked, or Salt (+1)
    C(is_salt, "", dry_cell, "B")
    dry_round = E(unreal.MaterialExpressionRound, -1250, 200)
    C(dry_cell, "", dry_round, "")
    dry_off = cell_offset(dry_round, -1150, 150)
    s_dry = sample(uv_xy, dry_off, -800, 150)
    detail = E(unreal.MaterialExpressionLinearInterpolate, -400, 500)
    C(detail_furrowed, "", detail, "A")
    C(s_dry, "RGB", detail, "B")
    C(dry, "", detail, "Alpha")

    dry_col = E(unreal.MaterialExpressionLinearInterpolate, -1100, -450)
    C(E(unreal.MaterialExpressionConstant3Vector, -1300, -500, constant=unreal.LinearColor(*CRACKED_RAW, 1.0)), "", dry_col, "A")
    C(E(unreal.MaterialExpressionConstant3Vector, -1300, -400, constant=unreal.LinearColor(*SALT_RAW, 1.0)), "", dry_col, "B")
    C(is_salt, "", dry_col, "Alpha")
    colour = E(unreal.MaterialExpressionLinearInterpolate, -800, -300)
    C(vc, "", colour, "A")  # VertexColor's RGB pin is named ""
    C(dry_col, "", colour, "B")
    C(dry, "", colour, "Alpha")

    # --- Tommy's M_Flat as it renders: vertex colour x his world noise. His style script links the
    # snapped position (floor(WorldPos / 40) * 40) to a pin named "Position", which does not exist (it
    # is "World Position"), so the noise has always read the raw world position: kept, to match. ---
    blocky = E(unreal.MaterialExpressionNoise, -1350, -700, scale=0.0045, quality=1, levels=2,
               output_min=0.86, output_max=1.08, turbulence=False)

    tinted = op(unreal.MaterialExpressionMultiply, colour, blocky, -500, -300)
    detailed = op(unreal.MaterialExpressionMultiply, tinted, detail, -300, 0)
    x2 = E(unreal.MaterialExpressionMultiply, -150, 0, const_b=2.0)
    C(detailed, "", x2, "A")
    rough = const(0.92, -300, 250)
    spec = const(0.25, -300, 350)
    MEL.connect_material_property(x2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def main():
    EAL.make_directory(DEST)
    mpc = unreal.load_asset(MPC_PATH)
    if mpc is None:
        unreal.log_error("ue_make_terrain_material: MPC_World missing — run tools/art/ue_import_scatter.py first")
        sys.exit(1)
    build(import_texture(), mpc)
    unreal.log("ue_make_terrain_material: T_Ground + M_Terrain in /Game/Art/Terrain")


main()
