"""The animals (Stage V batch 4): Quaternius bases -> the period's breeds, one skeletal GLB each. Runs in Blender:

  blender -b --factory-startup -P tools/art/fauna_variants.py -- [--only <id>] [--sheet]

For each skeletal row of art/fauna_models.csv that fauna.csv uses (a base used as-is, or a `variant` of
one): import the base (art/source/fauna/, tools/art/fetch_fauna.py), reshape the rest-pose mesh (the
skin weights stay, so the clips still drive it), recolour every material to the breed's palette ramp,
scale it to `size_m` (its body length; the clips' location keys scaled with it) and export
art/generated/fauna/SK_<id>.glb with its skeleton and clips. The farm sheep and pig carry Idle and Jump
only; their clips are renamed from "Armature|Armature|Idle" to "Idle".

Shapes (all bases face -Y, the head at the front; lengths are fractions of the body, rear 0 -> front 1):
  zebu_cattle       a hump over the withers (+10% of the length) — the humped cattle of the south
  fat_tailed_sheep  the rump and tail widened x2.2 and dropped — the fat-tailed sheep of Sumer
  mastiff_dog       girth x1.15;   saluki  girth x0.8 (a lean sighthound)
"""
import bpy
import csv
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flora_gen  # noqa: E402 -- palette + sRGB helpers
import kit_common as kc  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(REPO, "art", "source", "fauna")
OUT = os.path.join(REPO, "art", "generated", "fauna")
PALETTE = flora_gen.palette()

# Palette ramps per breed: [dark (hooves, face, legs), main coat, light (belly, muzzle), horn]
RAMPS = {
    "fat_tailed_sheep": ["goat_1", "cream_0", "cream_0", "reed_2"],
    "goat": ["goat_0", "goat_1", "mud_2", "reed_1"],
    "zebu_cattle": ["stone_0", "stone_2", "stone_3", "reed_3"],
    "donkey": ["goat_1", "stone_1", "stone_2", "cream_0"],
    "kunga": ["goat_1", "mud_1", "mud_3", "cream_0"],
    "mastiff_dog": ["mud_0", "mud_2", "ochre_1", "reed_2"],
    "saluki": ["mud_2", "reed_2", "cream_0", "reed_4"],
    "jackal": ["goat_1", "mud_3", "ochre_2", "reed_3"],
    "cat": ["stone_0", "stone_1", "stone_2", "stone_3"],
    "pig": ["goat_1", "mud_1", "mud_2", "mud_3"],
}
GIRTH = {"mastiff_dog": 1.15, "saluki": 0.8}


def rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def used_models():
    return {r["model"] for r in rows(os.path.join(REPO, "db", "canon", "fauna.csv")) if r["model"]}


def import_base(fname):
    before = set(bpy.data.objects)
    path = os.path.join(SRC, fname)
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    (bpy.ops.import_scene.gltf if fname.endswith(".gltf") else bpy.ops.import_scene.fbx)(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    return arm, meshes, new


def world_verts(meshes):
    return [(o, v, o.matrix_world @ v.co) for o in meshes for v in o.data.vertices]


def reshape(model_id, meshes):
    """Rest-pose edits in world space (the bases face -Y)."""
    vs = world_verts(meshes)
    ys = [p.y for _, _, p in vs]
    zs = [p.z for _, _, p in vs]
    y0, y1, z0, z1 = min(ys), max(ys), min(zs), max(zs)
    length = y1 - y0

    def frac(p):  # (0 at the rear .. 1 at the front, 0 at the feet .. 1 at the top)
        return (y1 - p.y) / length, (p.z - z0) / (z1 - z0)

    for o, v, p in vs:
        inv = o.matrix_world.inverted()
        fl, fh = frac(p)
        q = p.copy()
        if model_id == "zebu_cattle":
            d = math.hypot((fl - 0.72) * 1.0, (fh - 0.8) * 0.8)
            w = max(0.0, 1 - d / 0.16) ** 2
            q.z += 0.10 * length * w
        elif model_id == "fat_tailed_sheep":
            d = math.hypot(fl - 0.04, (fh - 0.62) * 0.8)
            w = max(0.0, 1 - d / 0.18) ** 2
            q.x *= 1 + 1.2 * w
            q.z -= 0.03 * length * w
        if model_id in GIRTH and 0.15 < fl < 0.8 and fh > 0.35:
            q.x *= GIRTH[model_id]
        v.co = inv @ q


def role(name, lum_rank, n):
    """0 dark, 1 main, 2 light, 3 horn, None = eye (kept)."""
    lo = name.lower()
    if "eye" in lo or "pupil" in lo:
        return None
    if re.match(r"^material(\.\d+)?$", lo):  # unnamed: rank by brightness
        return min(2, int(3 * lum_rank / max(1, n)))
    if "horn" in lo or "antler" in lo:
        return 3
    if any(k in lo for k in ("dark", "black", "hoof", "hooves", "grey", "hair")):
        return 0
    if any(k in lo for k in ("light", "white", "muzzle")):
        return 2
    return 1


def recolour(model_id, meshes):
    ramp = RAMPS[model_id]
    mats = sorted({s.material for o in meshes for s in o.material_slots if s.material}, key=lambda m: m.name)

    def lum(m):
        if m.use_nodes and "Principled BSDF" in m.node_tree.nodes:
            c = m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value
            return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
        return 0.5
    ranked = sorted(mats, key=lum)
    for m in mats:  # opaque: the farm pack's FBX materials carry alpha 0 (the sheep and pig exported invisible)
        if m.use_nodes and "Principled BSDF" in m.node_tree.nodes:
            alpha = m.node_tree.nodes["Principled BSDF"].inputs["Alpha"]
            for link in list(alpha.links):
                m.node_tree.links.remove(link)
            alpha.default_value = 1.0
        if hasattr(m, "surface_render_method"):
            m.surface_render_method = "DITHERED"
        m.blend_method = "OPAQUE"
    for m in mats:
        r = role(m.name, ranked.index(m), len(ranked))
        if r is None or not m.use_nodes or "Principled BSDF" not in m.node_tree.nodes:
            continue
        rgb = PALETTE[ramp[r]]
        base = m.node_tree.nodes["Principled BSDF"].inputs["Base Color"]
        for link in list(base.links):  # whatever fed the colour (vertex colour, a mix): the ramp replaces it
            m.node_tree.links.remove(link)
        base.default_value = (*[flora_gen.srgb_to_linear(c) for c in rgb], 1.0)
    for o in meshes:  # a base's vertex colours would tint the new coat in UE
        for a in list(o.data.color_attributes):
            o.data.color_attributes.remove(a)


def scale_to(arm, meshes, size_m):
    ys = [p.y for _, _, p in world_verts(meshes)]
    k = float(size_m) / (max(ys) - min(ys))
    bpy.ops.object.select_all(action="DESELECT")
    for o in [arm] + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    arm.scale = [s * k for s in arm.scale]
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for act in bpy.data.actions:  # the clips' translations (root and hip bob) shrink with the body
        for fc in act.fcurves:
            if fc.data_path.endswith("location"):
                for kp in fc.keyframe_points:
                    kp.co.y *= k
                    kp.handle_left.y *= k
                    kp.handle_right.y *= k
    return k


def rename_clips():
    for act in bpy.data.actions:
        short = act.name.split("|")[-1]
        if short != act.name:
            act.name = short


def export(arm, meshes, model_id):
    bpy.ops.object.select_all(action="DESELECT")
    for o in [arm] + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    path = os.path.join(OUT, f"SK_{model_id}.glb")
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_skins=True,
                              export_animations=True, export_animation_mode="ACTIONS", export_yup=True,
                              export_materials="EXPORT", export_vertex_color="NONE")
    return path


def main():
    args = kc.parse_args(sys.argv)
    models = {m["id"]: m for m in rows(os.path.join(REPO, "art", "fauna_models.csv"))}
    wanted = [m for m in models.values() if m["id"] in used_models() and m["source"] != "code"]
    if "only" in args:
        wanted = [m for m in wanted if m["id"] == args["only"]]
    os.makedirs(OUT, exist_ok=True)
    bad, sheet = 0, []
    for m in wanted:
        base = models[m["variant_of"]] if m["variant_of"] else m
        bpy.ops.wm.read_factory_settings(use_empty=True)
        try:
            arm, meshes, _ = import_base(base["file"])
        except (FileNotFoundError, StopIteration) as e:
            print(f"FAIL {m['id']}: {e} (run tools/art/fetch_fauna.py)")
            bad += 1
            continue
        rename_clips()
        reshape(m["id"], meshes)
        recolour(m["id"], meshes)
        k = scale_to(arm, meshes, m["size_m"])
        path = export(arm, meshes, m["id"])
        tris = sum(len(p.vertices) - 2 for o in meshes for p in o.data.polygons)
        clips = sorted(a.name for a in bpy.data.actions)
        print(f"fauna_variants: {m['id']} <- {base['id']}: {tris} tris, x{k:.3f}, clips {','.join(clips)} -> {os.path.basename(path)}")
    print(f"fauna_variants: {len(wanted) - bad}/{len(wanted)} animals -> {OUT}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
