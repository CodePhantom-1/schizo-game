"""Render fixed-angle Workbench thumbnails of every kit piece and example
house, plus one contact sheet PNG.

Run (after house_kit.py and assemble_house.py have populated <out>/meshes/):
    ~/opt/blender/blender -b --factory-startup -P tools/art/thumbs.py -- --out art/generated

Uses CPU Workbench rendering (no GPU renderer, per the hard rule) at 512px.
Rebuilds each piece/house from the same builder functions used by
house_kit.py / assemble_house.py (avoids re-importing FBX, keeps the render
in the same process) and frames it with a fixed 3/4 camera angle.
"""
import bpy
import math
import os
import sys
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_common as kc  # noqa: E402
import house_kit as hk  # noqa: E402
import assemble_house as ah  # noqa: E402

REPO_ROOT = hk.REPO_ROOT
SIZE = 512


def setup_render_scene():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = SIZE
    scene.render.resolution_y = SIZE
    scene.render.film_transparent = False
    scene.render.image_settings.file_format = "PNG"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.world = scene.world or bpy.data.worlds.new("World")
    scene.world.color = (0.82, 0.82, 0.85)

    cam_data = bpy.data.cameras.new("ThumbCam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("ThumbCam", cam_data)
    bpy.context.collection.objects.link(cam)
    scene.camera = cam
    return cam


# Fixed 3/4 view direction (camera sits along +VIEW_DIR from the subject,
# looking back along -VIEW_DIR) — the same angle for every render.
_AZ, _EL = math.radians(-40), math.radians(28)
VIEW_DIR = Vector((
    math.cos(_EL) * math.sin(_AZ) * -1,
    math.cos(_EL) * math.cos(_AZ) * -1,
    math.sin(_EL),
)).normalized()
MARGIN = 1.2  # fraction of headroom around the tight projected bounding box


def frame_camera(cam, obj):
    """Frame obj fully in view: same fixed angle every time, but the
    orthographic scale and camera distance are derived from obj's own
    bounding box (projected into camera space), so nothing gets clipped
    regardless of the object's size or aspect ratio."""
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    center = sum(corners, Vector((0.0, 0.0, 0.0))) / 8
    radius = max((c - center).length for c in corners)
    dist = max(radius * 4.0, 2.0)

    cam.location = center + VIEW_DIR * dist
    cam.rotation_euler = (-VIEW_DIR).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_end = dist * 4.0 + radius * 4.0

    # project the bounding box into the camera's own local space to get
    # the exact width/height that must fit in frame (a single "radius"
    # heuristic under-fits elongated objects like the stair or a 3-room
    # house), then apply a margin.
    bpy.context.view_layer.update()
    inv = cam.matrix_world.inverted()
    local_pts = [inv @ c for c in corners]
    us = [p.x for p in local_pts]
    vs = [p.y for p in local_pts]
    width = max(us) - min(us)
    height = max(vs) - min(vs)
    cam.data.ortho_scale = max(width, height, 0.3) * MARGIN


def render_object(obj, cam, out_path):
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    frame_camera(cam, obj)
    bpy.context.scene.render.filepath = out_path
    bpy.ops.render.render(write_still=True)


def build_contact_sheet(image_paths, out_path, cols=4):
    """Tile the rendered PNGs into one contact sheet using Blender's own
    image API (no extra image-processing dependency needed)."""
    imgs = [bpy.data.images.load(p) for p in image_paths]
    rows = math.ceil(len(imgs) / cols)
    cell = SIZE
    pad = 8
    sheet_w = cols * cell + (cols + 1) * pad
    sheet_h = rows * cell + (rows + 1) * pad
    sheet = bpy.data.images.new("contact_sheet", sheet_w, sheet_h, alpha=False)
    pixels = [0.12, 0.12, 0.13, 1.0] * (sheet_w * sheet_h)

    for idx, img in enumerate(imgs):
        col = idx % cols
        row = idx // cols
        ox = pad + col * (cell + pad)
        # image rows are bottom-up; place newest rows at the top visually
        oy = sheet_h - pad - (row + 1) * cell - row * pad
        src = list(img.pixels)
        iw, ih = img.size
        for y in range(min(ih, cell)):
            src_row_off = y * iw * 4
            dst_y = oy + y
            if dst_y < 0 or dst_y >= sheet_h:
                continue
            dst_off = (dst_y * sheet_w + ox) * 4
            row_px = src[src_row_off: src_row_off + min(iw, cell) * 4]
            pixels[dst_off: dst_off + len(row_px)] = row_px

    sheet.pixels = pixels
    sheet.filepath_raw = out_path
    sheet.file_format = "PNG"
    sheet.save()


def main():
    args = kc.parse_args(sys.argv)
    out_dir = args.get("out", "art/generated")
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(REPO_ROOT, out_dir)
    thumbs_dir = os.path.join(out_dir, "thumbs")
    os.makedirs(thumbs_dir, exist_ok=True)

    kc.clear_scene()
    cam = setup_render_scene()

    rendered = []
    for _, build_fn in hk.PIECES:
        obj = build_fn()
        kc.apply_bevel(obj)
        out_path = os.path.join(thumbs_dir, f"{obj.name}.png")
        render_object(obj, cam, out_path)
        rendered.append(out_path)
        print(f"rendered {obj.name}")
        bpy.data.objects.remove(obj, do_unlink=True)

    for key, build_fn in ah.HOUSES:
        obj = build_fn()
        out_path = os.path.join(thumbs_dir, f"{obj.name}.png")
        render_object(obj, cam, out_path)
        rendered.append(out_path)
        print(f"rendered {obj.name}")
        kc.clear_scene()
        cam = setup_render_scene()

    contact_path_generated = os.path.join(thumbs_dir, "contact_sheet.png")
    build_contact_sheet(rendered, contact_path_generated, cols=4)
    print(f"contact sheet -> {contact_path_generated}")

    # also publish the committed review copy per the task spec
    review_dir = os.path.join(REPO_ROOT, "art", "review")
    os.makedirs(review_dir, exist_ok=True)
    review_path = os.path.join(review_dir, "house_kit_contact.png")
    import shutil
    shutil.copyfile(contact_path_generated, review_path)
    print(f"review copy -> {review_path}")


if __name__ == "__main__":
    main()
