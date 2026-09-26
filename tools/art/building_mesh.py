"""The building mesher (Stage V batch 1): building_grammar.py's specs.json -> one GLB per building,
plus a contact sheet of a sample. Runs inside Blender (5.2 LTS):

  nice -n 19 ~/.local/bin/blender -b --factory-startup -P tools/art/building_mesh.py -- \
      [--specs art/generated/buildings/specs.json] [--out art/generated/buildings] [--only <id>] [--sheet]

Each part of a spec becomes a closed solid (checked manifold here, part by part: the parts overlap,
so a whole-file manifold test means nothing), flat-shaded, and every face carries:
  UV0  planar metres / the material's tile size (the material wraps it: frac(UV0))
  UV1  the trim-atlas cell (col, row), constant over the face — M_Building samples
       T_Trim at (frac(UV0) + UV1) / 8
  COLOR_0  RGB = tint x grime, stored at HALF (M_Building multiplies by 2, so tints up to 2x
       survive the 8-bit vertex colour); A = drought wear weight (earthen surfaces crack, cloth
       does not)
Frames: the spec is UE's local frame (x along the width, y along the depth, z up). UE's glTF
import mirrors Y (right- to left-handed), so the mesh is authored with y negated here and lands in
UE with the door on its data side.
"""
import bmesh
import bpy
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit_common as kc  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
_CELLS_PATH = os.path.join(REPO, "art", "generated", "tex", "trim_cells.json")
# The trim atlas exists only after trim_atlas.py; importing this module for contact_sheet() must not need it.
CELLS = json.load(open(_CELLS_PATH)) if os.path.exists(_CELLS_PATH) else {"grid": 8, "cells": {}}
GRID = CELLS["grid"]
TILE_M = {"plaster": 2.0, "whitewash": 2.0, "packed_earth": 2.0, "stone": 1.5}  # else 1 m
EARTH = {"mudbrick": 1.0, "plaster": 1.0, "whitewash": 1.0, "packed_earth": 1.0, "cone_mosaic": 1.0,
         "ochre_band": 1.0, "fired_brick": 0.5, "stone": 0.3}  # else 0.1; textiles 0
WEAR_GRIME = {"fresh": 0.5, "worn": 1.0, "crumbling": 1.4}
SEG = 8


def budget(s):
    return 6000 if s["wealth"] in ("elite", "sacred") or s["w"] * s["d"] > 200 else 3500


# ---- closed solids in the part's frame (origin bottom centre); each returns (verts, faces) ----
def box(sx, sy, sz, batter=0.0):
    hx, hy = sx / 2, sy / 2
    tx, ty = (hx, hy - batter) if sx >= sy else (hx - batter, hy)
    v = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0),
         (-tx, -ty, sz), (tx, -ty, sz), (tx, ty, sz), (-tx, ty, sz)]
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return v, f


def ring_solid(rings, apex_top=None):
    """Stacked closed rings (bottom to top), capped; apex_top closes the top with one vertex."""
    v, f, n = [], [], len(rings[0])
    for r in rings:
        v += r
    for k in range(len(rings) - 1):
        a, b = k * n, (k + 1) * n
        f += [(a + i, a + (i + 1) % n, b + (i + 1) % n, b + i) for i in range(n)]
    f.append(tuple(reversed(range(n))))
    top = (len(rings) - 1) * n
    if apex_top is None:
        f.append(tuple(range(top, top + n)))
    else:
        v.append(apex_top)
        f += [(top + i, top + (i + 1) % n, len(v) - 1) for i in range(n)]
    return v, f


def circle(rx, ry, z):
    return [(rx * math.cos(2 * math.pi * i / SEG), ry * math.sin(2 * math.pi * i / SEG), z) for i in range(SEG)]


def prism(rb, rt, h):
    if rt < 0.005:
        return ring_solid([circle(rb, rb, 0)], apex_top=(0, 0, h))
    return ring_solid([circle(rb, rb, 0), circle(rt, rt, h)])


def dome(rx, ry, h):
    rings = [circle(rx * math.cos(a), ry * math.cos(a), h * math.sin(a)) for a in (0, 0.4, 0.8, 1.15)]
    return ring_solid(rings, apex_top=(0, 0, h))


def extrude_x(poly, sx):
    """A closed (y, z) polygon extruded along x, capped (caps may be concave: the exporter
    triangulates n-gons with polyfill)."""
    n = len(poly)
    v = [(-sx / 2, y, z) for y, z in poly] + [(sx / 2, y, z) for y, z in poly]
    f = [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    return v, f + [tuple(reversed(range(n))), tuple(range(n, 2 * n))]


def vault(sx, sy, sz, t=0.15):
    """A barrel arch spanning y (a C section, open underneath), extruded along x."""
    hy = sy / 2
    arc = lambda ry, rz: [(ry * math.cos(math.pi * i / SEG), rz * math.sin(math.pi * i / SEG)) for i in range(SEG + 1)]  # noqa: E731
    outer = arc(hy, sz)
    inner = list(reversed(arc(hy - t, sz - t)))
    return extrude_x(outer + inner, sx)


def gable(sx, sy, sz, open_side=1, eave=0.0, t=0.1):
    """A tent roof, ridge along x: the back leg reaches the ground, the open_side leg ends at eave."""
    o, hy = open_side, sy / 2
    back_out, ridge_out, front_out = (-o * hy, 0.0), (0.0, sz), (o * hy, eave)
    front_in = (o * hy, eave - t) if eave > t else (o * (hy - 0.12), 0.0)
    poly = [back_out, ridge_out, front_out, front_in, (0.0, sz - 1.5 * t), (-o * (hy - 0.12), 0.0)]
    if o < 0:
        poly.reverse()
    return extrude_x(poly, sx)


def hull(sx, sy, sz, stations=7):
    """A reed boat: U sections lofted along x, the ends pinched and swept up."""
    secs = []
    for i in range(stations):
        t = -1 + 2 * i / (stations - 1)
        k = sy / 2 * max(0.08, math.sqrt(max(0.0, 1 - t * t)))
        zb, zt = 0.4 * sz * t * t, sz * (1 + 0.5 * t * t)
        x = t * sx / 2
        secs.append([(x, -k, zt), (x, -0.6 * k, zb + 0.15 * (zt - zb)), (x, 0, zb), (x, 0.6 * k, zb + 0.15 * (zt - zb)), (x, k, zt)])
    v, f, n = [p for s in secs for p in s], [], 5
    for k in range(stations - 1):
        a, b = k * n, (k + 1) * n
        f += [(a + i, a + (i + 1) % n, b + (i + 1) % n, b + i) for i in range(n)]
    last = (stations - 1) * n
    return v, f + [tuple(reversed(range(n))), tuple(range(last, last + n))]


def solid(p):
    sx, sy, sz = p["size"]
    shape = p["shape"]
    if shape == "box":
        return box(sx, sy, sz, p.get("batter") or 0.0)
    if shape == "prism":
        return prism(sx, sy, sz)
    if shape == "dome":
        return dome(sx, sy, sz)
    if shape == "vault":
        return vault(sx, sy, sz)
    if shape == "gable":
        return gable(sx, sy, sz, p.get("open", 1), p.get("eave", 0.0))
    if shape == "hull":
        return hull(sx, sy, sz)
    raise ValueError(f"unknown shape {shape!r}")


# ---- grime ----
def grime(s, p, x, y, z):
    """The multiplier (0..1) of a vertex at spec-frame (x, y, z): damp at the base (by wear), soot.
    ponytail: per-vertex on coarse boxes, so the base damp is a gradient up the whole wall and a
    hand band can't show; subdivide the walls if grime needs to read sharper."""
    g = 1.0 - 0.35 * WEAR_GRIME.get(s["wear"], 1.0) * max(0.0, 1 - z / 0.7) * (0.5 if p["tag"] == "marker" else 1.0)
    for sx, sy, sz, r in s.get("soot", []):
        dist = math.dist((x, y, z), (sx, sy, sz))
        g *= 1 - 0.55 * max(0.0, 1 - dist / r)
    return max(0.0, g)


def build(s):
    """One spec -> one mesh object named SM_B_<id>. Returns (obj, tris)."""
    rng = random.Random(s["id"])
    bm = bmesh.new()
    uv0 = bm.loops.layers.uv.new("UV0")
    uv1 = bm.loops.layers.uv.new("UV1")
    col = bm.loops.layers.float_color.new("Col")
    for pi, p in enumerate(s["parts"]):
        verts, faces = solid(p)
        yaw = math.radians(p.get("yaw") or 0.0)
        c, sn = math.cos(yaw), math.sin(yaw)
        ax, ay, az = p["at"]
        j = p.get("jitter") or 0.0
        world = []
        for vx, vy, vz in verts:
            x, y, z = ax + c * vx - sn * vy, ay + sn * vx + c * vy, az + vz
            if j:
                x += rng.uniform(-j, j)
                y += rng.uniform(-j, j)
                if z > az + 0.01:
                    z += rng.uniform(-j, j)
            world.append((x, y, z))
        # Each part in its own bmesh first: prove it closed, orient it outward, then merge it in.
        pb = bmesh.new()
        pv = [pb.verts.new((x, -y, z)) for x, y, z in world]  # UE mirrors Y on import
        for fi in faces:
            pb.faces.new([pv[i] for i in fi])
        bad = [e for e in pb.edges if not e.is_manifold]
        if bad:
            raise RuntimeError(f"{s['id']} part {pi} ({p['shape']} {p['tag']}): {len(bad)} open edges")
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
        nv = [bm.verts.new(v.co) for v in pb.verts]
        pb.verts.index_update()
        mat, wear = p["mat"].split("/")
        cc, cr = CELLS["cells"][p["mat"]]
        tile = TILE_M.get(mat, 1.0)
        wear_a = 0.0 if mat.startswith("textile") or mat == "goat_hair" else EARTH.get(mat, 0.1)
        tint = p.get("tint") or [1.0, 1.0, 1.0]
        for f in pb.faces:
            nf = bm.faces.new([nv[v.index] for v in f.verts])
            nf.smooth = False
            n = f.normal
            ua, va = ((1, 2), (0, 2), (0, 1))[max(range(3), key=lambda k: abs(n[k]))]
            for loop in nf.loops:
                co = loop.vert.co
                loop[uv0].uv = (co[ua] / tile, co[va] / tile)
                loop[uv1].uv = (cc, 1 - cr)  # glTF export flips V: UE reads (col, row)
                g = grime(s, p, co.x, -co.y, co.z)
                loop[col] = (min(1.0, tint[0] * g * 0.5), min(1.0, tint[1] * g * 0.5), min(1.0, tint[2] * g * 0.5), wear_a)
        pb.free()
    tris = sum(len(f.verts) - 2 for f in bm.faces)
    mesh = bpy.data.meshes.new(f"SM_B_{s['id']}")
    bm.to_mesh(mesh)
    bm.free()
    mesh.color_attributes.active_color = mesh.color_attributes["Col"]
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(building_material())
    return obj, tris


def building_material():
    """The preview twin of UE's M_Building: T_Trim at (frac(UV0) + UV1) / 8 x vertex colour x 2
    (in UE's top-down V; see the offset below)."""
    if "M_Building" in bpy.data.materials:
        return bpy.data.materials["M_Building"]
    m = bpy.data.materials.new("M_Building")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.95
    u0, u1 = nt.nodes.new("ShaderNodeUVMap"), nt.nodes.new("ShaderNodeUVMap")
    u0.uv_map, u1.uv_map = "UV0", "UV1"
    fr = nt.nodes.new("ShaderNodeVectorMath")
    fr.operation = "FRACTION"
    add = nt.nodes.new("ShaderNodeVectorMath")
    add.operation = "ADD"
    # Blender samples images bottom-up and the glTF export flips V: the UE formula, seen from here,
    # is (frac(UV0) + UV1 + (0, GRID - 2)) / GRID.
    off = nt.nodes.new("ShaderNodeVectorMath")
    off.operation = "ADD"
    off.inputs[1].default_value = (0.0, GRID - 2.0, 0.0)
    div = nt.nodes.new("ShaderNodeVectorMath")
    div.operation = "SCALE"
    div.inputs["Scale"].default_value = 1.0 / GRID
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.interpolation = "Closest"
    tex.image = bpy.data.images.load(os.path.join(REPO, "art", "generated", "tex", "T_Trim.png"))
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type, mul.blend_type = "RGBA", "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    x2 = nt.nodes.new("ShaderNodeMix")
    x2.data_type, x2.blend_type = "RGBA", "MULTIPLY"
    x2.inputs["Factor"].default_value = 1.0
    x2.inputs[7].default_value = (2.0, 2.0, 2.0, 1.0)
    L = nt.links.new
    L(u0.outputs["UV"], fr.inputs[0])
    L(fr.outputs["Vector"], add.inputs[0])
    L(u1.outputs["UV"], add.inputs[1])
    L(add.outputs["Vector"], off.inputs[0])
    L(off.outputs["Vector"], div.inputs[0])
    L(div.outputs["Vector"], tex.inputs["Vector"])
    L(tex.outputs["Color"], mul.inputs[6])
    L(vc.outputs["Color"], mul.inputs[7])
    L(mul.outputs[2], x2.inputs[6])
    L(x2.outputs[2], bsdf.inputs["Base Color"])
    return m


def export(obj, out_dir):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(out_dir, f"{obj.name}.glb")
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_normals=True,
                              export_vertex_color="ACTIVE", export_all_vertex_colors=False,
                              export_materials="PLACEHOLDER", export_yup=True)
    return path


# ---- the contact sheet ----
def sample(specs, n=24):
    """One of each typology first (a spread of wealth), then the rest in id order."""
    seen, out = set(), []
    for s in sorted(specs, key=lambda s: (s["wealth"], s["id"])):
        if s["typology"] not in seen:
            seen.add(s["typology"])
            out.append(s)
    return sorted(out, key=lambda s: s["id"])[:n]


def contact_sheet(objs, path, cols=6, cell=384):
    import numpy as np  # noqa: PLC0415 -- Blender bundles numpy
    import thumbs  # noqa: PLC0415 -- its fixed 3/4 camera framing
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.render.resolution_x = scene.render.resolution_y = cell
    scene.render.image_settings.file_format = "PNG"
    scene.world = scene.world or bpy.data.worlds.new("World")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.62, 0.72, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(200))
    bpy.context.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.type = "ORTHO"
    bpy.context.collection.objects.link(cam)
    scene.camera = cam
    rows = math.ceil(len(objs) / cols)
    sheet = np.zeros((rows * cell, cols * cell, 4), dtype=np.float32)
    tmp = os.path.join(os.path.dirname(path), "_cell.png")
    coll = bpy.context.collection
    for o in list(coll.objects):
        if o.type == "MESH":
            coll.objects.unlink(o)  # every building stands at the origin: render them one at a time
    for i, o in enumerate(objs):
        coll.objects.link(o)
        thumbs.frame_camera(cam, o)
        scene.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        coll.objects.unlink(o)
        img = bpy.data.images.load(tmp)
        px = np.empty(cell * cell * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        bpy.data.images.remove(img)
        r, c = i // cols, i % cols
        sheet[(rows - 1 - r) * cell:(rows - r) * cell, c * cell:(c + 1) * cell] = px.reshape(cell, cell, 4)
    os.remove(tmp)
    out = bpy.data.images.new("contact", cols * cell, rows * cell, alpha=False)
    out.pixels.foreach_set(sheet.ravel())
    out.filepath_raw = path
    out.file_format = "PNG"
    out.save()


def main():
    args = kc.parse_args(sys.argv)
    specs = json.load(open(args.get("specs", os.path.join(REPO, "art", "generated", "buildings", "specs.json"))))
    out = args.get("out", os.path.join(REPO, "art", "generated", "buildings"))
    if "only" in args:
        specs = [s for s in specs if s["id"] == args["only"]]
    os.makedirs(out, exist_ok=True)
    kc.clear_scene()
    objs, over = {}, []
    for s in specs:
        obj, tris = build(s)
        export(obj, out)
        objs[s["id"]] = obj
        if tris > budget(s):
            over.append(f"{s['id']} {tris}>{budget(s)}")
    print(f"building_mesh: {len(specs)} GLBs -> {out}; {sum(len(o.data.polygons) for o in objs.values())} faces")
    if over:
        print("OVER BUDGET: " + ", ".join(over))
    if "sheet" in args:
        path = os.path.join(REPO, "art", "review", "buildings_contact.png")
        contact_sheet([objs[s["id"]] for s in sample(specs)], path)
        print(f"contact sheet -> {path}")
    sys.exit(1 if over else 0)


if __name__ == "__main__":
    main()
