"""The birds and the carp (Stage V batch 4): code-built low-poly meshes, no skeleton. Runs in Blender:

  blender -b --factory-startup -P tools/art/birds_gen.py -- [--sheet]

Each `code` row of art/fauna_models.csv -> art/generated/fauna/SM_Bird_<id>.glb (the carp SM_Fish_carp):
a body (a squashed icosphere), a head, a beak, legs for the waders and wings. Flying species (dove,
crow, sparrow, vulture) carry spread wings; the waders and the water fowl fold theirs along the body
(M_Bird flaps them only when an instance's FlapHz > 0). Colour is palette vertex colour (linear, like
the scatter; M_Bird decodes the mesh build's sRGB bytes); vertex alpha is the wing weight (0 body ..
1 wing tip) that M_Bird's flap bends. Longest side scaled to `size_m`; origin at the feet (the belly
for swimmers and flyers). <= 300 triangles each.
"""
import bmesh
import bpy
import csv
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flora_gen  # noqa: E402
import kit_common as kc  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(REPO, "art", "generated", "fauna")
PAL = flora_gen.palette()
LIN = {n: tuple(flora_gen.srgb_to_linear(c) for c in rgb) for n, rgb in PAL.items()}

# species: body, wing, head, beak colours; neck length and leg length (x body length); wings spread?
SPECIES = {
    "goose": dict(body="cream_0", wing="stone_2", head="cream_0", beak="saffron_0", neck=0.45, legs=0.0, spread=False),
    "duck": dict(body="mud_3", wing="mud_2", head="leaf_0", beak="saffron_1", neck=0.2, legs=0.0, spread=False),
    "dove": dict(body="stone_2", wing="stone_1", head="stone_2", beak="madder_2", neck=0.1, legs=0.0, spread=True),
    "heron": dict(body="stone_1", wing="stone_0", head="stone_3", beak="saffron_0", neck=0.8, legs=1.0, spread=False),
    "flamingo": dict(body="madder_2", wing="madder_1", head="madder_2", beak="goat_0", neck=1.0, legs=1.3, spread=False),
    "vulture": dict(body="goat_1", wing="goat_0", head="cream_0", beak="stone_0", neck=0.25, legs=0.0, spread=True),
    "crow": dict(body="goat_0", wing="goat_0", head="goat_0", beak="goat_1", neck=0.1, legs=0.0, spread=True),
    "sparrow": dict(body="mud_4", wing="ochre_1", head="mud_2", beak="goat_1", neck=0.05, legs=0.0, spread=True),
}


class Mesh:
    def __init__(self):
        self.bm = bmesh.new()
        self.col = self.bm.loops.layers.float_color.new("Col")

    def face(self, pts, colour, alphas):
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        for loop, a in zip(f.loops, alphas):
            loop[self.col] = (*LIN[colour], a)
        return f

    def blob(self, c, r, colour, alpha=0.0, sub=1):
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=sub, radius=1.0)
        for f in tmp.faces:
            self.face([(c[0] + v.co.x * r[0], c[1] + v.co.y * r[1], c[2] + v.co.z * r[2]) for v in f.verts],
                      colour, [alpha] * len(f.verts))
        tmp.free()

    def cone(self, base, tip, r, colour, sides=4):
        bx, by, bz = base
        ring = [(bx, by + r * math.cos(2 * math.pi * k / sides), bz + r * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
        for k in range(sides):
            self.face([ring[k], ring[(k + 1) % sides], tip], colour, [0, 0, 0])

    def stick(self, a, b, r, colour):
        """A thin 4-sided tube from a to b (necks, legs)."""
        (ax, ay, az), (bx, by, bz) = a, b
        offs = [(r, 0), (0, r), (-r, 0), (0, -r)]
        lo = [(ax + ox, ay + oy, az) for ox, oy in offs]
        hi = [(bx + ox, by + oy, bz) for ox, oy in offs]
        for k in range(4):
            j = (k + 1) % 4
            self.face([lo[k], lo[j], hi[j], hi[k]], colour, [0] * 4)

    def finish(self, name):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        self.bm.free()
        me.color_attributes.active_color = me.color_attributes["Col"]
        for p in me.polygons:
            p.use_smooth = False
        obj = bpy.data.objects.new(name, me)
        bpy.context.collection.objects.link(obj)
        return obj


def bird(sp, s):
    """A bird 1 unit long along +X (head forward), standing on z = 0 or resting on its belly."""
    m = Mesh()
    leg = s["legs"]
    z = leg * 0.5 + 0.18
    m.blob((0, 0, z), (0.5, 0.2, 0.18), s["body"])
    nx, nz = 0.4 + 0.1 * s["neck"], z + 0.15 + 0.35 * s["neck"]
    if s["neck"] > 0.3:
        m.stick((0.35, 0, z + 0.1), (nx, 0, nz), 0.04, s["body"])
    m.blob((nx, 0, nz), (0.1, 0.08, 0.08), s["head"])
    m.cone((nx + 0.08, 0, nz), (nx + 0.08 + 0.12 + 0.2 * s["neck"], 0, nz - 0.02), 0.03, s["beak"])
    m.face([(-0.5, 0, z + 0.05), (-0.75, 0.1, z + 0.08), (-0.75, -0.1, z + 0.08)], s["wing"], [0, 0, 0])  # tail
    if leg > 0:
        for side in (-1, 1):
            m.stick((0, 0.06 * side, z - 0.12), (0.02, 0.06 * side, 0.0), 0.015, "stone_0")
    for side in (-1, 1):
        if s["spread"]:  # a wing out to the side, the tip lifts with the flap
            w = [(0.2, 0.15 * side, z + 0.05), (-0.25, 0.15 * side, z + 0.05), (-0.35, 0.85 * side, z + 0.08), (0.05, 0.9 * side, z + 0.08)]
            m.face(w if side > 0 else list(reversed(w)), s["wing"], [0.0, 0.0, 1.0, 1.0])
        else:  # folded along the flank
            w = [(0.25, 0.19 * side, z + 0.08), (-0.45, 0.19 * side, z + 0.06), (-0.55, 0.16 * side, z + 0.14), (0.15, 0.17 * side, z + 0.16)]
            m.face(w if side > 0 else list(reversed(w)), s["wing"], [0.0, 0.2, 0.3, 0.1])
    return m


def carp():
    m = Mesh()
    m.blob((0, 0, 0.15), (0.5, 0.12, 0.18), "saffron_0")
    m.face([(-0.45, 0, 0.15), (-0.75, 0, 0.35), (-0.75, 0, -0.05)], "ochre_1", [0, 1, 1])  # the tail (A: it beats; M_Bird is two-sided)
    m.face([(0.1, 0, 0.32), (-0.15, 0, 0.42), (-0.25, 0, 0.3)], "ochre_1", [0, 0, 0])  # the dorsal fin
    return m


def normalise(obj, size_m):
    me = obj.data
    xs, ys, zs = zip(*[v.co for v in me.vertices])
    k = float(size_m) / max(max(xs) - min(xs), max(ys) - min(ys))
    cx, cy, z0 = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, min(zs)
    for v in me.vertices:
        v.co = ((v.co.x - cx) * k, (v.co.y - cy) * k, (v.co.z - z0) * k)
    me.update()


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_normals=True,
                              export_vertex_color="ACTIVE", export_all_vertex_colors=False,
                              export_materials="PLACEHOLDER", export_yup=True)


def main():
    args = kc.parse_args(sys.argv)
    rows = [r for r in csv.DictReader(open(os.path.join(REPO, "art", "fauna_models.csv"), newline="", encoding="utf-8"))
            if r["source"] == "code"]
    os.makedirs(OUT, exist_ok=True)
    kc.clear_scene()
    objs, over = [], []
    for r in rows:
        name = f"SM_Fish_{r['file']}" if r["file"] == "carp" else f"SM_Bird_{r['file']}"
        m = carp() if r["file"] == "carp" else bird(r["file"], SPECIES[r["file"]])
        obj = m.finish(name)
        normalise(obj, r["size_m"])
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        if tris > int(r["tris"]):
            over.append(f"{r['id']} {tris}>{r['tris']}")
        export(obj, os.path.join(OUT, f"{name}.glb"))
        objs.append(obj)
        print(f"birds_gen: {r['id']} {tris} tris -> {name}.glb")
    if over:
        print("OVER BUDGET: " + ", ".join(over))
    if "sheet" in args:
        from building_mesh import contact_sheet  # noqa: PLC0415
        from scatter_mesh import scatter_material  # noqa: PLC0415 -- vertex colour preview
        mat = scatter_material()
        for o in objs:
            o.data.materials.append(mat)
            bpy.context.collection.objects.unlink(o)
        path = os.path.join(REPO, "art", "review", "birds_contact.png")
        contact_sheet(objs, path, cols=5)
        print(f"contact sheet -> {path}")
    sys.exit(1 if over else 0)


if __name__ == "__main__":
    main()
