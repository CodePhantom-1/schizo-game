"""The weather's cards (Stage V batch 5): a rain streak, a dust mote and a smoke card. Runs in Blender:

  blender -b --factory-startup -P tools/art/fx_gen.py

Each is two crossed vertical quads (seen from any side, no camera-facing needed), UV 0..1, origin at the
bottom centre, exported to art/generated/fx/SM_FX_<name>.glb. The motion (falling, drifting, rising) and the
alpha live in the materials (tools/art/ue_make_fx_materials.py), driven by MPC_World.
  Rain   4 cm x 90 cm        Dust  10 cm x 10 cm        Smoke  120 cm x 120 cm
"""
import bmesh
import bpy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit_common as kc  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(REPO, "art", "generated", "fx")
CARDS = {"Rain": (0.04, 0.9), "Dust": (0.1, 0.1), "Smoke": (1.2, 1.2)}


def card(name, w, h):
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    for axis in (0, 1):  # a quad in XZ and one in YZ
        pts = [(-w / 2, 0, 0), (w / 2, 0, 0), (w / 2, 0, h), (-w / 2, 0, h)]
        if axis:
            pts = [(0, x, z) for x, _, z in pts]
        f = bm.faces.new([bm.verts.new(p) for p in pts])
        for loop, (u, v) in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
            loop[uv].uv = (u, v)
    me = bpy.data.meshes.new(f"SM_FX_{name}")
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(me.name, me)
    bpy.context.collection.objects.link(obj)
    return obj


def main():
    os.makedirs(OUT, exist_ok=True)
    kc.clear_scene()
    for name, (w, h) in CARDS.items():
        obj = card(name, w, h)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = os.path.join(OUT, f"{obj.name}.glb")
        bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_materials="PLACEHOLDER",
                                  export_yup=True)
        print(f"fx_gen: {obj.name} -> {path}")


if __name__ == "__main__":
    main()
