"""The scatter mesher (Stage V batch 2): art/scatter_meshes.csv -> one palette-locked GLB per row.
Runs inside Blender:

  blender -b --factory-startup -P tools/art/scatter_mesh.py -- \
      [--meshes art/scatter_meshes.csv] [--out art/generated/scatter] [--only <id>] [--sheet]

Per row: import the pack file (or build the `code` plant with flora_gen), join to one object with its
transforms applied, scale its longest side to `size_m` (the packs are toy scale; blank keeps the
source size), put the origin at the bottom centre, and recolour: every face takes its material's
colour (a textured material: the texture sampled at the face's UV centre — KayKit packs one atlas),
snapped to the nearest art/palette.csv colour, written to the CORNER colour attribute `Col` (linear,
as glTF COLOR_0 wants; UE's mesh build stores it sRGB-encoded, so M_Scatter decodes it with a 2.2 power). A = 1 on green-dominant faces of `leafy` rows (M_Scatter browns them with the
drought), else 0. Materials are dropped for one slot `M_Scatter`; flat shading; export SM_F_<id>.glb
and upsert its manifest row. A missing source prints `FAIL <id>: missing <path>`, exports nothing for
that row, and the run exits 1.
"""
import bpy
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flora_gen  # noqa: E402
import kit_common as kc  # noqa: E402
import manifest  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
PALETTE = flora_gen.palette()
LINEAR = {n: tuple(flora_gen.srgb_to_linear(c) for c in rgb) for n, rgb in PALETTE.items()}


def linear_to_srgb(c):
    c = max(0.0, min(1.0, c))
    return 255.0 * (c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055)


# Kenney's colours are cartoon-saturated (its palm leaves are mint, 112,229,214): a plain nearest snap
# lands them on glaze blue. Its named materials map to the palette by hand; anything else snaps.
MATERIAL_MAP = {
    "leafsGreen": "leaf_1", "leafsDark": "leaf_0", "grass": "leaf_2",
    "woodBark": "palm_2", "woodBarkDark": "ochre_0", "woodInner": "reed_3", "wood": "ochre_1",
    "_defaultMat": "ochre_1", "stone": "stone_2", "dirt": "mud_4", "dirtDark": "mud_2",
    "colorRed": "madder_1", "colorWhite": "cream_0", "colorPurple": "indigo_1", "colorYellow": "saffron_1",
}
# Whole-row colours where the source colour reads wrong (KayKit's lilies snap to lagoon water).
ROW_COLOUR = {"kk_waterlily_a": "leaf_0", "kk_waterlily_b": "leaf_0",
              # Kenney's wheat: green shoots; the ripe stage's "_defaultMat" would read as terracotta
              "crops_wheat_a": "leaf_1", "crops_wheat_b": "reed_3"}


def nearest(rgb):
    """The palette name nearest an sRGB 0..255 colour (plain RGB distance, like stylize.quantize)."""
    return min(PALETTE, key=lambda n: sum((a - b) ** 2 for a, b in zip(PALETTE[n], rgb)))


# ---- import ----
def import_source(row):
    """The row's source as one mesh object in the scene, or raise FileNotFoundError."""
    if row["source"] == "code":
        return flora_gen.BUILDERS[row["file"]]()
    path = os.path.join(REPO, "art", "source", row["source"], row["file"])
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        raise FileNotFoundError(f"{path} (no mesh inside)")
    for o in meshes:  # bake parents' transforms in, then drop the empties
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    for o in new:
        if o.type != "MESH":
            bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    return bpy.context.view_layer.objects.active


# ---- colour ----
_images = {}


def _pixels(img):
    import numpy as np  # noqa: PLC0415 -- Blender bundles numpy
    if img.name not in _images:
        w, h = img.size
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        _images[img.name] = (w, h, px.reshape(h, w, 4))
    return _images[img.name]


def _base_image(mat):
    if not mat or not mat.use_nodes:
        return None
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        return None
    stack = [bsdf.inputs["Base Color"]]
    while stack:  # walk back through mix/multiply nodes to the first image
        sock = stack.pop()
        for link in sock.links:
            n = link.from_node
            if n.type == "TEX_IMAGE" and n.image:
                return n.image
            stack += [i for i in n.inputs if i.is_linked]
    return None


def face_srgb(obj, poly, uv):
    """The face's colour in sRGB 0..255: its texture at the UV centre, else its base colour."""
    mat = obj.material_slots[poly.material_index].material if obj.material_slots else None
    img = _base_image(mat)
    if img is not None and uv is not None:
        w, h, px = _pixels(img)
        us = [uv.data[i].uv for i in poly.loop_indices]
        u, v = sum(p.x for p in us) / len(us), sum(p.y for p in us) / len(us)
        r, g, b, _ = px[min(h - 1, int((v % 1.0) * h)), min(w - 1, int((u % 1.0) * w))]
        return (255 * r, 255 * g, 255 * b)  # 8-bit images hold sRGB values
    if mat is None:
        return (128, 128, 128)
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None) if mat.use_nodes else None
    c = bsdf.inputs["Base Color"].default_value if bsdf else mat.diffuse_color
    return tuple(linear_to_srgb(c[i]) for i in range(3))


def face_colour(obj, poly, uv):
    """The face's palette name: the hand map for a named pack material, else the nearest snap."""
    mat = obj.material_slots[poly.material_index].material if obj.material_slots else None
    base = mat.name.split(".")[0] if mat else ""
    if base in MATERIAL_MAP:
        return MATERIAL_MAP[base]
    if base.startswith("pal_"):  # flora_gen's plants carry their palette name
        return base[4:]
    return nearest(face_srgb(obj, poly, uv))


def recolour(obj, leafy, row_id=""):
    me = obj.data
    uv = me.uv_layers.active
    names = {}
    for p in me.polygons:
        names[p.index] = ROW_COLOUR.get(row_id) or face_colour(obj, p, uv)
    col = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    for p in me.polygons:
        n = names[p.index]
        r, g, b = PALETTE[n]
        a = 1.0 if leafy and g > r and g > b else 0.0
        c = (*LINEAR[n], a)
        for li in p.loop_indices:
            col.data[li].color = c
    me.color_attributes.active_color = col
    return sorted(set(names.values()))


# ---- shape ----
def normalise(obj, size_m):
    """Apply transforms, scale the longest side to size_m, origin at the bottom centre."""
    me = obj.data
    me.transform(obj.matrix_world)
    obj.matrix_world.identity()
    xs, ys, zs = zip(*[v.co for v in me.vertices])
    k = 1.0
    if size_m:
        k = float(size_m) / max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    cx, cy, z0 = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, min(zs)
    for v in me.vertices:
        v.co = ((v.co.x - cx) * k, (v.co.y - cy) * k, (v.co.z - z0) * k)
    me.update()


def flat(obj):
    me = obj.data
    if me.has_custom_normals:
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj]):
            bpy.ops.mesh.customdata_custom_splitnormals_clear()
    for p in me.polygons:
        p.use_smooth = False


def scatter_material():
    """The preview twin of UE's M_Scatter: base colour = vertex colour `Col`."""
    if "M_Scatter" in bpy.data.materials:
        return bpy.data.materials["M_Scatter"]
    m = bpy.data.materials.new("M_Scatter")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.9
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = "Col"
    nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
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


def manifest_row(row, path):
    src = {r["id"]: r for r in manifest.load()}.get(row["source"])
    rel = os.path.relpath(path, REPO).replace(os.sep, "/")
    r = {"id": f"SM_F_{row['id']}", "file": rel, "kind": "mesh_scatter", "ai": "false", "tag": "A",
         "source_ref": "art/scatter_meshes.csv; world-art-plan 7 (flora) and 5 (props)"}
    if row["source"] == "code":
        r.update(license="LicenseRef-Original", author="schizo-game contributors",
                 source_ref=r["source_ref"] + f"; code-built by tools/art/flora_gen.py:{row['file']}")
    else:
        r.update(license=src["license"], author=src["author"], url=src["url"], acquired=src["acquired"],
                 source_ref=r["source_ref"] + f"; stylized from {row['source']}:{row['file']}")
    return r


def main():
    args = kc.parse_args(sys.argv)
    rows = list(csv.DictReader(open(args.get("meshes", os.path.join(REPO, "art", "scatter_meshes.csv")), newline="")))
    out = args.get("out", os.path.join(REPO, "art", "generated", "scatter"))
    if "only" in args:
        rows = [r for r in rows if r["id"] == args["only"]]
    os.makedirs(out, exist_ok=True)
    kc.clear_scene()
    mat = scatter_material()
    objs, bad, over, mrows = [], 0, [], []
    for row in rows:
        try:
            obj = import_source(row)
        except FileNotFoundError as e:
            print(f"FAIL {row['id']}: missing {e}")
            bad += 1
            continue
        obj.name = obj.data.name = f"SM_F_{row['id']}"
        normalise(obj, row.get("size_m"))
        colours = recolour(obj, row["leafy"] == "1", row["id"])
        flat(obj)
        obj.data.materials.clear()
        obj.data.materials.append(mat)
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        if tris == 0:
            print(f"FAIL {row['id']}: zero faces")
            bad += 1
            continue
        if tris > int(row["max_tris"]):
            over.append(f"{row['id']} {tris}>{row['max_tris']}")
        path = export(obj, out)
        mrows.append(manifest_row(row, path))
        bpy.context.collection.objects.unlink(obj)  # keep it for the sheet, out of the next import's way
        objs.append(obj)
        print(f"scatter_mesh: {row['id']} {tris} tris, colours {','.join(colours)}")
    manifest.upsert(manifest.PATH, mrows)
    print(f"scatter_mesh: {len(objs)}/{len(rows)} GLBs -> {out}")
    if over:
        print("OVER BUDGET: " + ", ".join(over))
    if "sheet" in args and objs:
        from building_mesh import contact_sheet  # noqa: PLC0415 -- its Cycles CPU sheet
        path = os.path.join(REPO, "art", "review", "scatter_contact.png")
        contact_sheet(objs, path)
        print(f"contact sheet -> {path}")
    sys.exit(1 if bad or over else 0)


if __name__ == "__main__":
    main()
