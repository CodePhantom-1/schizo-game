"""ue_make_style_materials.py — the D-023 style materials, authored by code.

D-023: low-poly, low-fi, Valheim-like; the look is carried by light, fog and
colour, not by textures. Every surface in the game draws from four master
materials under /Game/Art/Style:

  M_Flat      Colour param x vertex colour x per-instance tint (ISM custom
              data 0..2, white when absent), broken up by a coarse, blocky
              world-space noise — the "pixel texture" shimmer of low-fi
              games without any texture assets.
  M_FlatWind  M_Flat plus a wind sway in world-position offset, weighted by
              vertex-colour alpha (0 = rooted, 1 = frond tip).
  M_Water     Sea and canals: deep colour, glossy, slow swells in WPO.
  M_Glow      Unlit-looking emissive for fire, lamps and the lighthouse flame.

Run headless (no Python install needed — UE ships its own):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended
Re-running replaces the assets (idempotent).
"""
import unreal

PATH = "/Game/Art/Style"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty


def new_material(name):
    full = f"{PATH}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    return mat


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    mel.connect_material_expressions(a, a_out, b, b_in)


def mul(mat, a, a_out, b, b_out, x, y):
    m = expr(mat, unreal.MaterialExpressionMultiply, x, y)
    link(a, a_out, m, "A")
    link(b, b_out, m, "B")
    return m


def blocky_noise(mat, x, y, cell=40.0, lo=0.86, hi=1.08, scale=0.0045):
    """floor(WorldPos / cell) * cell -> noise: flat colour broken into coarse
    blocks (a low-res texture look with no texture)."""
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, x, y)
    div = expr(mat, unreal.MaterialExpressionDivide, x + 200, y, const_b=cell)
    link(wp, "", div, "A")
    fl = expr(mat, unreal.MaterialExpressionFloor, x + 350, y)
    link(div, "", fl, "")
    back = expr(mat, unreal.MaterialExpressionMultiply, x + 500, y, const_b=cell)
    link(fl, "", back, "A")
    noise = expr(mat, unreal.MaterialExpressionNoise, x + 650, y,
                 scale=scale, quality=1, levels=2, output_min=lo, output_max=hi,
                 turbulence=False)
    link(back, "", noise, "Position")
    return noise


def flat_base(mat, default_color, roughness=0.92):
    """Colour x VertexColor.rgb x PerInstanceCustomData(0..2) x blocky noise."""
    col = expr(mat, unreal.MaterialExpressionVectorParameter, -1400, -300,
               parameter_name="Color", default_value=default_color)
    vc = expr(mat, unreal.MaterialExpressionVertexColor, -1400, -100)
    tint = expr(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -1400, 100,
                data_index=0, const_default_value=unreal.LinearColor(1, 1, 1, 1))
    m1 = mul(mat, col, "", vc, "", -1100, -200)
    m2 = mul(mat, m1, "", tint, "", -900, -100)
    noise = blocky_noise(mat, -1800, 350)
    m3 = mul(mat, m2, "", noise, "", -700, 0)
    mel.connect_material_property(m3, "", MP.MP_BASE_COLOR)
    r = expr(mat, unreal.MaterialExpressionScalarParameter, -700, 250,
             parameter_name="Roughness", default_value=roughness)
    mel.connect_material_property(r, "", MP.MP_ROUGHNESS)
    s = expr(mat, unreal.MaterialExpressionScalarParameter, -700, 350,
             parameter_name="Specular", default_value=0.25)
    mel.connect_material_property(s, "", MP.MP_SPECULAR)
    return vc


def finish(mat):
    mel.recompile_material(mat)
    eal.save_asset(mat.get_path_name().split(".")[0])
    unreal.log(f"style material saved: {mat.get_path_name()}")


# --- M_Flat -------------------------------------------------------------------
m = new_material("M_Flat")
flat_base(m, unreal.LinearColor(0.6, 0.5, 0.38, 1))
finish(m)

# --- M_FlatWind ---------------------------------------------------------------
m = new_material("M_FlatWind")
m.set_editor_property("two_sided", True)
vc = flat_base(m, unreal.LinearColor(0.2, 0.32, 0.1, 1), roughness=0.8)
# offset = (sin(t*speed + phase), cos(t*speed*0.8 + phase)*0.6, 0) * amp * alpha
t = expr(m, unreal.MaterialExpressionTime, -1800, 800)
spd = expr(m, unreal.MaterialExpressionMultiply, -1650, 800, const_b=1.6)
link(t, "", spd, "A")
wp = expr(m, unreal.MaterialExpressionWorldPosition, -1800, 950)
wpx = expr(m, unreal.MaterialExpressionComponentMask, -1650, 950, r=True, g=True, b=False, a=False)
link(wp, "", wpx, "")
dot = expr(m, unreal.MaterialExpressionDotProduct, -1500, 950)
link(wpx, "", dot, "A")
phase_dir = expr(m, unreal.MaterialExpressionConstant2Vector, -1650, 1050, r=0.0021, g=0.0017)
link(phase_dir, "", dot, "B")
add = expr(m, unreal.MaterialExpressionAdd, -1350, 850)
link(spd, "", add, "A")
link(dot, "", add, "B")
sin = expr(m, unreal.MaterialExpressionSine, -1200, 800, period=6.2831853)
link(add, "", sin, "")
cos = expr(m, unreal.MaterialExpressionCosine, -1200, 950, period=6.2831853)
link(add, "", cos, "")
cos_s = expr(m, unreal.MaterialExpressionMultiply, -1050, 950, const_b=0.6)
link(cos, "", cos_s, "A")
app = expr(m, unreal.MaterialExpressionAppendVector, -900, 850)
link(sin, "", app, "A")
link(cos_s, "", app, "B")
app3 = expr(m, unreal.MaterialExpressionAppendVector, -750, 850)
link(app, "", app3, "A")
zero = expr(m, unreal.MaterialExpressionConstant, -900, 1000, r=0.0)
link(zero, "", app3, "B")
amp = expr(m, unreal.MaterialExpressionScalarParameter, -750, 1000,
           parameter_name="WindAmplitude", default_value=18.0)
sway = mul(m, app3, "", amp, "", -600, 900)
weighted = mul(m, sway, "", vc, "A", -450, 900)
mel.connect_material_property(weighted, "", MP.MP_WORLD_POSITION_OFFSET)
finish(m)

# --- M_Water ------------------------------------------------------------------
m = new_material("M_Water")
deep = expr(m, unreal.MaterialExpressionVectorParameter, -1200, -300,
            parameter_name="Color", default_value=unreal.LinearColor(0.02, 0.09, 0.11, 1))
noise = blocky_noise(m, -1800, 0, cell=150.0, lo=0.8, hi=1.2, scale=0.002)
wc = mul(m, deep, "", noise, "", -900, -200)
mel.connect_material_property(wc, "", MP.MP_BASE_COLOR)
mel.connect_material_property(
    expr(m, unreal.MaterialExpressionScalarParameter, -700, 100,
         parameter_name="Roughness", default_value=0.12), "", MP.MP_ROUGHNESS)
mel.connect_material_property(
    expr(m, unreal.MaterialExpressionConstant, -700, 200, r=0.7), "", MP.MP_SPECULAR)
# swells: z = (sin(t*0.7 + x*0.0035) + sin(t*1.1 + y*0.005)) * amp
t = expr(m, unreal.MaterialExpressionTime, -1800, 500)
wp = expr(m, unreal.MaterialExpressionWorldPosition, -1800, 650)
wx = expr(m, unreal.MaterialExpressionComponentMask, -1650, 650, r=True, g=False, b=False, a=False)
link(wp, "", wx, "")
wy = expr(m, unreal.MaterialExpressionComponentMask, -1650, 750, r=False, g=True, b=False, a=False)
link(wp, "", wy, "")
waves = []
for i, (tm, src, k) in enumerate(((0.7, wx, 0.0035), (1.1, wy, 0.005))):
    ts = expr(m, unreal.MaterialExpressionMultiply, -1500, 500 + i * 200, const_b=tm)
    link(t, "", ts, "A")
    ks = expr(m, unreal.MaterialExpressionMultiply, -1500, 600 + i * 200, const_b=k)
    link(src, "", ks, "A")
    a = expr(m, unreal.MaterialExpressionAdd, -1350, 550 + i * 200)
    link(ts, "", a, "A")
    link(ks, "", a, "B")
    s = expr(m, unreal.MaterialExpressionSine, -1200, 550 + i * 200, period=6.2831853)
    link(a, "", s, "")
    waves.append(s)
sum_w = expr(m, unreal.MaterialExpressionAdd, -1050, 650)
link(waves[0], "", sum_w, "A")
link(waves[1], "", sum_w, "B")
amp = expr(m, unreal.MaterialExpressionScalarParameter, -1050, 800,
           parameter_name="WaveAmplitude", default_value=12.0)
h = mul(m, sum_w, "", amp, "", -900, 700)
z = expr(m, unreal.MaterialExpressionAppendVector, -600, 700)
xy0 = expr(m, unreal.MaterialExpressionConstant2Vector, -750, 650, r=0.0, g=0.0)
link(xy0, "", z, "A")
link(h, "", z, "B")
mel.connect_material_property(z, "", MP.MP_WORLD_POSITION_OFFSET)
finish(m)

# --- M_Glow -------------------------------------------------------------------
m = new_material("M_Glow")
gc = expr(m, unreal.MaterialExpressionVectorParameter, -900, -200,
          parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.45, 0.12, 1))
gi = expr(m, unreal.MaterialExpressionScalarParameter, -900, 0,
          parameter_name="Intensity", default_value=30.0)
# a slow flicker so flames breathe: 0.8 + 0.2*sin(t*9 + worldpos.x)
t = expr(m, unreal.MaterialExpressionTime, -1500, 200)
ts = expr(m, unreal.MaterialExpressionMultiply, -1350, 200, const_b=9.0)
link(t, "", ts, "A")
fs = expr(m, unreal.MaterialExpressionSine, -1200, 200, period=6.2831853)
link(ts, "", fs, "")
fa = expr(m, unreal.MaterialExpressionMultiply, -1050, 200, const_b=0.2)
link(fs, "", fa, "A")
fb = expr(m, unreal.MaterialExpressionAdd, -900, 200, const_b=0.8)
link(fa, "", fb, "A")
e1 = mul(m, gc, "", gi, "", -700, -100)
e2 = mul(m, e1, "", fb, "", -500, 0)
mel.connect_material_property(e2, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(gc, "", MP.MP_BASE_COLOR)
finish(m)

# --- flat-colour instances for the grey-box interactables ----------------------
def srgb(r, g, b):
    f = lambda c: (c / 255.0) / 12.92 if c / 255.0 <= 0.04045 else ((c / 255.0 + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(f(r), f(g), f(b), 1.0)


flat = unreal.load_asset(f"{PATH}/M_Flat")
for name, rgb in (("MI_Door", (96, 64, 40)), ("MI_Tablet", (176, 128, 86)), ("MI_Well", (150, 116, 84)),
                  ("MI_Bed", (190, 160, 96)), ("MI_Pickup", (184, 106, 62))):
    full = f"{PATH}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", flat)
    mel.set_material_instance_vector_parameter_value(mi, "Color", srgb(*rgb))
    eal.save_asset(full)
    unreal.log(f"style instance saved: {full}")

unreal.log("ue_make_style_materials: done")