"""Assemble 3 example houses from the mudbrick kit pieces (T8) and export each
as one combined mesh for review.

Run:
    ~/opt/blender/blender -b --factory-startup -P tools/art/assemble_house.py -- --out art/generated

Reuses the piece builders in house_kit.py directly (same process, same
functions) rather than re-importing the exported FBX/GLB, so assembly always
matches the exact geometry the kit produced.

Houses (grid units = GRID metres, footprint on the X/Y grid):
  1. small single-room   — 3x3 walled room, one door, flat roof, no courtyard.
  2. two-room + courtyard — two rooms side by side sharing a wall, open
     courtyard with a reed awning, matching a real Mesopotamian courtyard
     house plan [A].
  3. two-storey-ish w/ roof access — a 3x3 room with an external stair
     rising to a roof-access hole, for roof-top sleeping [A] (flat mudbrick
     roofs were used as an extra room, common practice in the hot climate).
"""
import bpy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_common as kc  # noqa: E402
import house_kit as hk  # noqa: E402

REPO_ROOT = hk.REPO_ROOT
G = kc.GRID


import math as _math

WALL_SIZE = (kc.GRID, kc.WALL_THICK)
CELL_SIZE = (kc.GRID, kc.GRID)


def place(obj, target_x, target_y, z, rot_deg_z=0, size=CELL_SIZE):
    """Place a copy of obj so that, AFTER rotating rot_deg_z (a multiple of
    90) around its local corner-origin, its footprint's min corner lands at
    world (target_x, target_y). `size` is the piece's own local (sx, sy)
    footprint (metres) before rotation — required because rotating a
    corner-pivoted box swings it outside the target cell unless the
    translation compensates for it."""
    sx, sy = size
    r = rot_deg_z % 360
    if r == 0:
        ox, oy = 0.0, 0.0
    elif r == 90:
        ox, oy = sy, 0.0
    elif r == 180:
        ox, oy = sx, sy
    elif r == 270:
        ox, oy = 0.0, sx
    else:
        raise ValueError("rot_deg_z must be a multiple of 90")
    dup = obj.copy()
    dup.data = obj.data.copy()
    bpy.context.collection.objects.link(dup)
    dup.location = (target_x + ox, target_y + oy, z)
    dup.rotation_euler[2] = _math.radians(rot_deg_z)
    return dup


def build_room_walls(w, d, door_side="south", window_sides=("east",), x0=0, y0=0):
    """Build the perimeter walls of a w x d (grid units) room, anchored at
    grid origin (x0, y0). The four wall runs alone form a complete, closed,
    non-overlapping perimeter (each run's thickness insets from the outer
    boundary) — no separate corner piece is needed to close the loop, so
    build_corner() is not used here (real mudbrick walls simply butt-join
    at the corner; SM_Corner is offered as a decorative/reinforcing option,
    demonstrated separately in house_roof_access)."""
    objs = []
    # Exactly one door on door_side and one window per side in
    # window_sides, at that run's middle tile — not every tile along the
    # run (a wall run of length > 1 would otherwise get a door cut into
    # every one of its tiles).
    mid_idx = {"south": w // 2, "north": w // 2, "west": d // 2, "east": d // 2}

    def wall_piece(side, idx):
        if side == door_side and idx == mid_idx[side]:
            return hk.build_wall_door()
        if side in window_sides and idx == mid_idx[side]:
            return hk.build_wall_window()
        return hk.build_wall_plain()

    # south + north runs (along X), west + east runs (along Y). Targets are
    # the final (post-rotation) min corner of each wall run in world metres.
    for i in range(w):
        p = wall_piece("south", i)
        objs.append(place(p, (x0 + i) * G, y0 * G, 0, 0, WALL_SIZE))
        bpy.data.objects.remove(p, do_unlink=True)
        p = wall_piece("north", i)
        objs.append(place(
            p, (x0 + i) * G, (y0 + d) * G - kc.WALL_THICK, 0, 180, WALL_SIZE
        ))
        bpy.data.objects.remove(p, do_unlink=True)
    for j in range(d):
        p = wall_piece("west", j)
        objs.append(place(p, x0 * G, (y0 + j) * G, 0, 270, WALL_SIZE))
        bpy.data.objects.remove(p, do_unlink=True)
        p = wall_piece("east", j)
        objs.append(place(
            p, (x0 + w) * G - kc.WALL_THICK, (y0 + j) * G, 0, 90, WALL_SIZE
        ))
        bpy.data.objects.remove(p, do_unlink=True)

    return objs


def build_flat_roof(w, d, height, access_at=None, x0=0, y0=0):
    """Tile the roof surface only (no parapet — see build_parapet_ring,
    which wraps the whole footprint's outer edge in one continuous run
    instead of stitching per-tile parapet fragments together)."""
    objs = []
    for i in range(w):
        for j in range(d):
            if access_at == (i, j):
                r = hk.build_roof_access()
            else:
                r = hk.build_roof_slab(False)
            objs.append(place(r, (x0 + i) * G, (y0 + j) * G, height, 0, CELL_SIZE))
            bpy.data.objects.remove(r, do_unlink=True)
    return objs


def build_parapet_ring(w, d, roof_z, x0=0, y0=0):
    """One continuous mudbrick parapet wrapping the roof footprint's full
    outer perimeter (all 4 sides + corners), built directly rather than
    from per-tile roof_slab parapet edges — a parapet tile per roof tile
    left gaps at every interior-facing tile edge and a broken look at
    corners (only one of a corner tile's two outer edges had a parapet)."""
    bm = kc.new_bmesh()
    t = kc.PARAPET_T
    z0 = roof_z + kc.ROOF_THICK
    z1 = z0 + kc.PARAPET_H
    x_min, x_max = x0 * G, (x0 + w) * G
    y_min, y_max = y0 * G, (y0 + d) * G
    kc.add_box(bm, (x_min, y_min, z0), (x_max, y_min + t, z1), mat_index=0)  # south
    kc.add_box(bm, (x_min, y_max - t, z0), (x_max, y_max, z1), mat_index=0)  # north
    kc.add_box(bm, (x_min, y_min, z0), (x_min + t, y_max, z1), mat_index=0)  # west
    kc.add_box(bm, (x_max - t, y_min, z0), (x_max, y_max, z1), mat_index=0)  # east
    return kc.finalize_object(bm, "parapet_ring", ["M_Mudbrick"])


def build_courtyard_floor(x0, y0, w, d):
    objs = []
    for i in range(w):
        for j in range(d):
            t = hk.build_courtyard_tile()
            # sit the floor slab just below z=0 (the walls' own base), so it
            # reads as a foundation slab flush under the walls rather than
            # sharing the same z-band as the wall base (which volumetrically
            # overlapped every wall tile it was under).
            objs.append(
                place(t, (x0 + i) * G, (y0 + j) * G, -kc.TILE_THICK, 0, CELL_SIZE)
            )
            bpy.data.objects.remove(t, do_unlink=True)
    return objs


def join_all(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    merged = bpy.context.view_layer.objects.active
    merged.name = name
    return merged


def house_small_single_room_objs():
    """3x3 single room, one door (south), one window (east), flat roof with
    a continuous parapet."""
    objs = build_room_walls(3, 3, door_side="south", window_sides=("east",))
    objs += build_flat_roof(3, 3, kc.WALL_HEIGHT)
    objs.append(build_parapet_ring(3, 3, kc.WALL_HEIGHT))
    objs += build_courtyard_floor(0, 0, 3, 3)
    return objs


def house_two_room_courtyard_objs():
    """Two 2x2 rooms side by side, each opening onto a shared 4x2 open
    courtyard with a reed awning, per the real courtyard-house plan [A]:
    rooms arranged around an open-air yard, not a fully roofed block.
    (Party wall between the rooms is two standing wall runs, not merged
    into one thin slab — real mudbrick party walls were doubled; ponytail:
    skip merging, the extra thickness is visually negligible at this scale.)
    """
    objs = []
    # room A at x[0..2], y[0..2]; door faces the courtyard (north)
    objs += build_room_walls(2, 2, door_side="north", window_sides=("west",), x0=0, y0=0)
    objs += build_flat_roof(2, 2, kc.WALL_HEIGHT, x0=0, y0=0)
    objs.append(build_parapet_ring(2, 2, kc.WALL_HEIGHT, x0=0, y0=0))
    # room B at x[2..4], y[0..2]; door also faces the courtyard (north)
    objs += build_room_walls(2, 2, door_side="north", window_sides=("east",), x0=2, y0=0)
    objs += build_flat_roof(2, 2, kc.WALL_HEIGHT, x0=2, y0=0)
    objs.append(build_parapet_ring(2, 2, kc.WALL_HEIGHT, x0=2, y0=0))
    # courtyard: 4x2 open yard to the north of both rooms
    objs += build_courtyard_floor(0, 2, 4, 2)
    objs += build_courtyard_floor(0, 0, 4, 2)
    # reed awning against room A's north (courtyard-facing) wall, sloping
    # out into the yard
    awning = hk.build_awning()
    objs.append(place(awning, 0.0, 2.0 * G, 0.0, 0, CELL_SIZE))
    bpy.data.objects.remove(awning, do_unlink=True)
    return objs


def house_roof_access_objs():
    """3x3 room with an external stair against the west wall, flush with
    the wall face and rising the full storey height to exactly roof level,
    leading to a roof-access hole one tile in from that wall. Flat roof
    with a continuous parapet. A buttress pilaster stands against the
    south wall's exterior face, clear of the door tile."""
    objs = build_room_walls(3, 3, door_side="south", window_sides=("east", "north"))
    objs += build_flat_roof(3, 3, kc.WALL_HEIGHT, access_at=(0, 1))
    objs.append(build_parapet_ring(3, 3, kc.WALL_HEIGHT))
    objs += build_courtyard_floor(0, 0, 3, 3)
    # stair runs flush against the west wall (x=0), full height, arriving
    # at roof level right next to the access hole at tile (0,1)
    stair = hk.build_stair()
    objs.append(place(stair, -1.0 * G, 0.0, 0.0, 0, (G, 2 * G)))
    bpy.data.objects.remove(stair, do_unlink=True)
    # buttress pilaster against the south wall's exterior face, on the
    # tile away from the door (door is the middle tile, i=1) so it never
    # stands in the doorway
    pil = hk.build_pilaster()
    objs.append(place(pil, 2.35 * G, -0.15 * G, 0.0, 0, (0.3, 0.15)))
    bpy.data.objects.remove(pil, do_unlink=True)
    return objs


def house_small_single_room():
    return join_all(house_small_single_room_objs(), "House_SmallSingleRoom")


def house_two_room_courtyard():
    return join_all(house_two_room_courtyard_objs(), "House_TwoRoomCourtyard")


def house_roof_access():
    return join_all(house_roof_access_objs(), "House_RoofAccess")


HOUSES = [
    ("house_small", house_small_single_room),
    ("house_courtyard", house_two_room_courtyard),
    ("house_roofaccess", house_roof_access),
]

# name -> pre-join objs builder, used by check_kit.py to validate the
# assembly (overlap / floater checks) before geometry disappears into one
# merged mesh.
HOUSE_OBJS_BUILDERS = [
    ("House_SmallSingleRoom", house_small_single_room_objs),
    ("House_TwoRoomCourtyard", house_two_room_courtyard_objs),
    ("House_RoofAccess", house_roof_access_objs),
]


def write_manifest_rows(rows):
    # Upsert-by-id lives in kit_common (shared with house_kit.py).
    kc.write_manifest_rows(rows)


def main():
    args = kc.parse_args(sys.argv)
    out_dir = args.get("out", "art/generated")
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(REPO_ROOT, out_dir)
    os.makedirs(out_dir, exist_ok=True)

    kc.clear_scene()
    manifest_rows = []
    for key, build_fn in HOUSES:
        obj = build_fn()
        tris = kc.triangulate_count(obj)
        fbx_path, glb_path = kc.export_piece(obj, out_dir)
        rel = os.path.relpath(fbx_path, REPO_ROOT)
        print(f"{obj.name}: {tris} tris -> {rel}")
        manifest_rows.append(
            [
                obj.name,
                rel,
                "mesh_house_example",
                "original (procedural, generated code)",
                "docs/art-pipeline-research.md; real-world Mesopotamian mudbrick architecture [A]",
                "A",
            ]
        )
        kc.clear_scene()
    write_manifest_rows(manifest_rows)
    print(f"Assembled {len(HOUSES)} example houses to {out_dir}")


if __name__ == "__main__":
    main()
