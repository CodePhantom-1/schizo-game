"""The code-built plants (Stage V batch 2): the Mesopotamian species the packs don't have.
Runs inside Blender; scatter_mesh.py imports it for the `code` rows of art/scatter_meshes.csv.

Each function builds one object in real metres, origin at the bottom centre, seeded by its name so
a rebuild is identical. Every face carries a material named `pal_<palette name>` (art/palette.csv),
so scatter_mesh.py's recolour snaps it exactly like a pack mesh's material colour.
"""
import bmesh
import bpy
import csv
import math
import os
import random

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def palette():
    """{name: (r, g, b)} in 0..255 sRGB, from art/palette.csv."""
    with open(os.path.join(REPO, "art", "palette.csv"), newline="") as f:
        return {r["name"]: (int(r["r"]), int(r["g"]), int(r["b"])) for r in csv.DictReader(f)}


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


class Builder:
    """A bmesh plus one material slot per palette name used."""

    def __init__(self, name):
        self.name = name
        self.rng = random.Random(name)
        self.bm = bmesh.new()
        self.slots = []
        self.pal = palette()

    def slot(self, colour):
        if colour not in self.pal:
            raise KeyError(f"{self.name}: no palette colour {colour!r}")
        if colour not in self.slots:
            self.slots.append(colour)
        return self.slots.index(colour)

    def face(self, pts, colour):
        f = self.bm.faces.new([self.bm.verts.new(p) for p in pts])
        f.material_index = self.slot(colour)
        return f

    def blade(self, x, y, h, w, lean, yaw, colour, segs=3):
        """A tapered leaf blade: a strip of quads closing to a tip, bending along yaw as it rises."""
        dx, dy = math.cos(yaw), math.sin(yaw)
        px, py = -dy, dx  # across the blade
        rows = []
        for i in range(segs + 1):
            t = i / segs
            off = lean * h * t * t
            cx, cy, cz = x + dx * off, y + dy * off, h * t * (1 - 0.15 * t * lean)
            half = w / 2 * (1 - t) if i < segs else 0.0
            rows.append(((cx - px * half, cy - py * half, cz), (cx + px * half, cy + py * half, cz)))
        for i in range(segs):
            (a, b), (c, d) = rows[i], rows[i + 1]
            if i == segs - 1:
                self.face([a, b, c], colour)
            else:
                self.face([a, b, d, c], colour)

    def tube(self, base, top, r0, r1, colour, sides=6, cap=True):
        """A tapered closed tube from base to top (points), radii r0 -> r1."""
        bx, by, bz = base
        tx, ty, tz = top
        ring = lambda cx, cy, cz, r: [(cx + r * math.cos(2 * math.pi * k / sides),  # noqa: E731
                                       cy + r * math.sin(2 * math.pi * k / sides), cz) for k in range(sides)]
        lo, hi = ring(bx, by, bz, r0), ring(tx, ty, tz, r1)
        for k in range(sides):
            j = (k + 1) % sides
            self.face([lo[k], lo[j], hi[j], hi[k]], colour)
        if cap:
            self.face(list(reversed(lo)), colour)
            self.face(hi, colour)

    def lump(self, centre, radii, colour, jitter=0.18):
        """A low-poly blob: an icosphere (80 tris), scaled and roughened."""
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=1, radius=1.0)
        cx, cy, cz = centre
        rx, ry, rz = radii
        pos = {}
        for v in tmp.verts:  # one jitter per vertex, so the blob stays closed
            k = 1 + self.rng.uniform(-jitter, jitter)
            pos[v.index] = (cx + v.co.x * rx * k, cy + v.co.y * ry * k, cz + v.co.z * rz * k)
        for f in tmp.faces:
            self.face([pos[v.index] for v in f.verts], colour)
        tmp.free()

    def finish(self):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-4)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for c in self.slots:
            mesh.materials.append(material(c, self.pal[c]))
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.collection.objects.link(obj)
        return obj


def material(name, rgb):
    key = f"pal_{name}"
    if key in bpy.data.materials:
        return bpy.data.materials[key]
    m = bpy.data.materials.new(key)
    m.diffuse_color = (*[srgb_to_linear(c) for c in rgb], 1.0)
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = m.diffuse_color
    return m


# ---- the plants ----
def reed_clump():
    """Giant reed (Phragmites): 14-20 tall tapered blades from one root, a few feathery plumes."""
    b = Builder("reed_clump")
    r = b.rng
    tops = []
    for _ in range(r.randint(14, 20)):
        a, d = r.uniform(0, 2 * math.pi), r.uniform(0, 0.35)
        x, y, h = d * math.cos(a), d * math.sin(a), r.uniform(1.8, 3.2)
        lean = r.uniform(0.05, 0.25)
        b.blade(x, y, h, r.uniform(0.05, 0.09), lean, a + r.uniform(-0.4, 0.4), r.choice(["leaf_1", "leaf_2"]))
        tops.append((x + math.cos(a) * lean * h, y + math.sin(a) * lean * h, h * (1 - 0.15 * lean)))
    for x, y, z in r.sample(tops, r.randint(3, 5)):
        b.lump((x, y, z + 0.2), (0.07, 0.07, 0.25), "reed_3", jitter=0.1)
    return b.finish()


def cattail():
    """Cattail (Typha): 8-12 blades and 3-5 brown spikes on thin stems."""
    b = Builder("cattail")
    r = b.rng
    for _ in range(r.randint(8, 12)):
        a, d = r.uniform(0, 2 * math.pi), r.uniform(0, 0.2)
        b.blade(d * math.cos(a), d * math.sin(a), r.uniform(1.2, 2.0), r.uniform(0.04, 0.07),
                r.uniform(0.05, 0.3), a, r.choice(["leaf_0", "leaf_1"]))
    for _ in range(r.randint(3, 5)):
        a, d = r.uniform(0, 2 * math.pi), r.uniform(0, 0.15)
        x, y, h = d * math.cos(a), d * math.sin(a), r.uniform(1.4, 2.0)
        b.tube((x, y, 0), (x, y, h), 0.012, 0.01, "leaf_1", sides=4)
        b.tube((x, y, h), (x, y, h + 0.25), 0.035, 0.035, "palm_1", sides=6)
    return b.finish()


def tamarisk():
    """Tamarisk: a short gnarled trunk under 5-7 feathery grey-green crowns."""
    b = Builder("tamarisk")
    r = b.rng
    top = (r.uniform(-0.2, 0.2), r.uniform(-0.2, 0.2), 1.4)
    b.tube((0, 0, 0), top, 0.16, 0.1, "palm_0", sides=6)
    for _ in range(r.randint(5, 7)):
        a, d = r.uniform(0, 2 * math.pi), r.uniform(0.4, 1.2)
        c = (top[0] + d * math.cos(a), top[1] + d * math.sin(a), top[2] + r.uniform(0.5, 1.8))
        b.tube(top, (c[0] * 0.8, c[1] * 0.8, c[2] - 0.3), 0.07, 0.04, "palm_0", sides=4)
        b.lump(c, (r.uniform(0.7, 1.1), r.uniform(0.7, 1.1), r.uniform(0.5, 0.8)), "leaf_2")
    return b.finish()


def saltbush():
    """Saltbush: a low dome of three grey-green lumps."""
    b = Builder("saltbush")
    r = b.rng
    for k in range(3):
        a = 2 * math.pi * k / 3 + r.uniform(-0.3, 0.3)
        b.lump((0.25 * math.cos(a), 0.25 * math.sin(a), 0.3), (0.45, 0.4, 0.35), r.choice(["leaf_2", "stone_1"]))
    return b.finish()


def palm_dead():
    """A dead date palm: a leaning grey trunk and three broken frond stubs."""
    b = Builder("palm_dead")
    r = b.rng
    h, lean = 6.5, math.radians(r.uniform(6, 12))
    top = (math.sin(lean) * h, 0.0, math.cos(lean) * h)
    b.tube((0, 0, 0), top, 0.22, 0.15, "stone_1", sides=8)
    for k in range(3):
        a = 2 * math.pi * k / 3 + r.uniform(-0.4, 0.4)
        tip = (top[0] + 0.9 * math.cos(a), top[1] + 0.9 * math.sin(a), top[2] - r.uniform(0.2, 0.6))
        b.tube(top, tip, 0.05, 0.02, "palm_1", sides=4)
    return b.finish()


BUILDERS = {"reed_clump": reed_clump, "cattail": cattail, "tamarisk": tamarisk, "saltbush": saltbush,
            "palm_dead": palm_dead}
