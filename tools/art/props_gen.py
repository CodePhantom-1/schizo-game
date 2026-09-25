"""Generate the street props + sky dome for the Moon Gate Quarter (dressing wave).

Run:
    ~/opt/blender/blender -b --factory-startup -P tools/art/props_gen.py -- --out art/generated/props

Produces one FBX + one GLB per prop in <out>/meshes/ and upserts each into
art/assets.csv (kind = mesh_street_prop). Exits non-zero if any prop misses
its triangle budget — the gate for this generator.

Style: same low-fi kit language as house_kit.py (D-023): real-world metres,
kit_common helpers for materials/export/manifest, flat colours for the small
clutter, the kit's existing CC0 ambientCG sets (reed weave, bark) where a
surface reads. All shapes follow real Mesopotamian daily-life material
culture, tag [A] — clay jars/bowls (Ubaid through Old Babylonian pottery
traditions), reed baskets and matting, low wooden furniture, tripod braziers
for the watch fire; specifics are general practice, not canon.

Origins: unlike the grid kit pieces (corner-origin for tiling), props are
modelled with local origin at BASE CENTRE, so runtime placement yaws them
about their own centre (SimPropsBuilder.cpp).
"""
import bmesh
import math
import os
import sys

import bpy  # noqa: F401 -- imported for clarity; tools run inside blender -b -P

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_common as kc  # noqa: E402

REPO_ROOT = kc._repo_root()  # noqa: SLF001 -- same path the kit manifest writer uses

# --- prop material palette (extends the kit's; flat D-023 colours, no new
# fetches — textures reuse the sets fetch_textures.py already pulled) --------
PROP_MAT_DEFS = {
    "M_Clay":   ((0.63, 0.36, 0.22), 0.90),  # plain wheel-thrown clay, unglazed
    "M_Dates":  ((0.32, 0.16, 0.05), 0.45),  # piled dried dates, faint sheen
    "M_Bread":  ((0.72, 0.50, 0.28), 0.95),  # barley flatbread crust
    "M_Hide":   ((0.55, 0.42, 0.30), 0.80),  # leather waterskin
    "M_Grain":  ((0.78, 0.65, 0.35), 0.95),  # hulled barley
    # The dome's placeholder slot — deliberately untextured so the FBX does
    # not embed a CC0 map the runtime sky material immediately replaces.
    "M_SkyDomeSlot": ((0.60, 0.70, 0.85), 1.00),
}
kc.MAT_DEFS.update(PROP_MAT_DEFS)

# Triangle budgets per prop (documented; enforced below, printed in the report).
PROP_TRI_BUDGET = {
    "amphora": 300,
    "jar_stack": 300,
    "basket": 250,
    "mat_roll": 150,
    "bench": 280,
    "dates_tray": 150,
    "bread_tray": 220,
    "bread_loaves": 150,
    "brazier": 300,
    "water_skin": 250,
    "sky_dome": 300,
}


def revolve(bm, profile, segments, mat_index=0):
    """Lathe a closed (radius, z) profile — bottom to top, first and last
    radius 0 so the solid is watertight — about the Z axis at x=y=0.
    Apex bands become triangle fans; the rest quads. Normals are fixed by
    finalize_object's recalc (closed solids always resolve outward)."""
    rings = []
    for r, z in profile:
        ring = []
        for i in range(segments):
            a = (i / segments) * 2.0 * math.pi
            ring.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        rings.append(ring)
    for ri in range(len(rings) - 1):
        r0, r1 = rings[ri], rings[ri + 1]
        p0, p1 = profile[ri], profile[ri + 1]
        for i in range(segments):
            j = (i + 1) % segments
            if p0[0] <= 1e-6:
                face = bm.faces.new([r0[i], r1[i], r1[j]])
            elif p1[0] <= 1e-6:
                face = bm.faces.new([r0[i], r0[j], r1[j]])
            else:
                face = bm.faces.new([r0[i], r0[j], r1[j], r1[i]])
            face.material_index = mat_index


def add_dome(bm, cx, cy, z_base, rx, ry, z_height, segments, rings, mat_index=0, up=True):
    """A half-ellipsoid cap on a flat base (loaves, date heaps, a hanging
    waterskin with up=False). Phi 0 at the base ring, pi/2 at the pole."""
    dir_z = 1.0 if up else -1.0
    ring_verts = []
    for i in range(rings + 1):
        phi = (i / rings) * (math.pi / 2.0)
        r = math.cos(phi)
        z = z_base + dir_z * z_height * math.sin(phi)
        ring = []
        for s in range(segments):
            a = (s / segments) * 2.0 * math.pi
            ring.append(bm.verts.new((cx + rx * r * math.cos(a), cy + ry * r * math.sin(a), z)))
        ring_verts.append(ring)
    for ri in range(rings):
        ra, rb = ring_verts[ri], ring_verts[ri + 1]
        for s in range(segments):
            t = (s + 1) % segments
            if ri == rings - 1:
                face = bm.faces.new([ra[s], ra[t], rb[t]])
            else:
                face = bm.faces.new([ra[s], ra[t], rb[t], rb[s]])
            face.material_index = mat_index
    # flat base fan (a tri fan through one centre vert)
    centre = bm.verts.new((cx, cy, z_base))
    base = ring_verts[0]
    for s in range(segments):
        t = (s + 1) % segments
        face = bm.faces.new([centre, base[s], base[t]])
        face.material_index = mat_index


def finish_prop(obj, budget_key, bevel_width=None):
    """Like kc.finish_piece but with a prop-sized optional bevel (the kit's
    1.5 cm default would gobble a 40 cm jar) and the prop budget table."""
    if bevel_width:
        kc.apply_bevel(obj, width=bevel_width, angle_deg=55)
    kc.unwrap_uv0_uv1(obj)
    tris = kc.triangulate_count(obj)
    budget = PROP_TRI_BUDGET[budget_key]
    status = "ok" if tris <= budget else "OVER BUDGET"
    print(f"{obj.name}: {tris} tris / budget {budget} ({status})")
    return tris, budget


def flip_normals(obj):
    """Turn a closed solid's normals inward (the sky dome is viewed from
    inside; finalize_object's recalc always resolves them outward)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()


# --- the props ---------------------------------------------------------------

def build_amphora():
    """A tall pointed-belly clay jar (water/beer), ~61 cm. [A] — the plain
    household storage jar of every Mesopotamian house; profile incl. the
    inward lip return so the mouth reads open."""
    bm = kc.new_bmesh()
    profile = [
        (0.000, 0.000), (0.045, 0.000), (0.058, 0.020), (0.105, 0.120),
        (0.160, 0.300), (0.150, 0.420), (0.095, 0.500), (0.068, 0.530),
        (0.066, 0.580), (0.088, 0.600), (0.080, 0.612), (0.060, 0.600),
        (0.056, 0.560), (0.085, 0.500), (0.000, 0.470),
    ]
    revolve(bm, profile, 10)
    return kc.finalize_object(bm, "SM_PropAmphora", ["M_Clay"])


def build_jar_stack():
    """The granary cluster: one big squat grain jar, a smaller one lidded on
    its rim, a third beside it. [A] — granary storage in stacked/clustered
    jars, standard practice."""
    bm = kc.new_bmesh()
    big = [
        (0.000, 0.000), (0.060, 0.000), (0.100, 0.030), (0.190, 0.220),
        (0.185, 0.360), (0.130, 0.420), (0.105, 0.435), (0.070, 0.420),
        (0.000, 0.400),
    ]
    revolve(bm, big, 8)
    top = [
        (0.000, 0.000), (0.045, 0.000), (0.080, 0.060), (0.095, 0.150),
        (0.075, 0.200), (0.040, 0.215), (0.000, 0.205),
    ]
    revolve(bm, [(r, z + 0.425) for r, z in top], 8)
    side = [
        (0.000, 0.000), (0.040, 0.000), (0.070, 0.050), (0.085, 0.130),
        (0.070, 0.170), (0.045, 0.190), (0.000, 0.180),
    ]
    revolve(bm, [(r + 0.24, z) for r, z in side], 8)
    return kc.finalize_object(bm, "SM_PropJarStack", ["M_Clay"])


def build_basket():
    """A woven reed basket heaped with barley. [A] — reed/palm-fibre baskets
    and grain measured by the basketload."""
    bm = kc.new_bmesh()
    profile = [
        (0.000, 0.000), (0.100, 0.000), (0.135, 0.050), (0.150, 0.180),
        (0.160, 0.260), (0.150, 0.300), (0.130, 0.285), (0.000, 0.270),
    ]
    revolve(bm, profile, 10)
    add_dome(bm, 0.0, 0.0, 0.260, 0.120, 0.120, 0.055, 10, 2, mat_index=1)
    return kc.finalize_object(bm, "SM_PropBasket", ["M_Reed", "M_Grain"])


def build_mat_roll():
    """A rolled reed mat, 80 cm, lying on its side. [A] — reed matting, the
    universal floor/roof covering; rolled for storage or sale."""
    bm = kc.new_bmesh()
    r, length, seg = 0.09, 0.80, 10
    for li in range(3):  # 2 length segments
        x0 = (li / 2.0) * length
        x1 = ((li + 1) / 2.0) * length
        pts = []
        for s in range(seg):
            a = (s / seg) * 2.0 * math.pi
            pts.append((r * math.cos(a) + 0.0, r * math.sin(a) + r))
        kc.extrude_profile_x(bm, pts, x0, x1)
    return kc.finalize_object(bm, "SM_PropMatRoll", ["M_Reed"])


def build_bench():
    """A low wooden bench, 90x25 cm seat at 26 cm. [A] — low wooden stools
    and benches of plain timber."""
    bm = kc.new_bmesh()
    kc.add_box(bm, (0.00, 0.00, 0.260), (0.90, 0.25, 0.305))
    for lx in (0.06, 0.78):
        for ly in (0.03, 0.17):
            kc.add_box(bm, (lx, ly, 0.0), (lx + 0.06, ly + 0.05, 0.26))
    kc.add_box(bm, (0.10, 0.09, 0.08), (0.80, 0.16, 0.13))  # under-seat rail
    return kc.finalize_object(bm, "SM_PropBench", ["M_Timber"])


def _tray(bm, w, d, mat_index):
    kc.add_box(bm, (0.0, 0.0, 0.0), (w, d, 0.02), mat_index=mat_index)
    t = 0.02
    kc.add_box(bm, (0.0, 0.0, 0.0), (w, t, 0.05), mat_index=mat_index)
    kc.add_box(bm, (0.0, d - t, 0.0), (w, d, 0.05), mat_index=mat_index)
    kc.add_box(bm, (0.0, t, 0.0), (t, d - t, 0.05), mat_index=mat_index)
    kc.add_box(bm, (w - t, t, 0.0), (w, d - t, 0.05), mat_index=mat_index)


def build_dates_tray():
    """A wooden tray heaped with dried dates. [A] — dates sold by measure,
    piled in trays/baskets (foods.csv dates; stall_produce_place)."""
    bm = kc.new_bmesh()
    _tray(bm, 0.45, 0.30, 0)
    add_dome(bm, 0.225, 0.15, 0.045, 0.16, 0.10, 0.05, 8, 3, mat_index=1)
    return kc.finalize_object(bm, "SM_PropDatesTray", ["M_Timber", "M_Dates"])


def build_bread_tray():
    """A bakery tray with three barley flatbreads. [A] — barley bread comes
    out at sunrise (schedules.csv baker_fires_the_oven)."""
    bm = kc.new_bmesh()
    _tray(bm, 0.40, 0.28, 0)
    for cx, cy in ((0.10, 0.09), (0.30, 0.09), (0.20, 0.20)):
        add_dome(bm, cx, cy, 0.045, 0.085, 0.055, 0.025, 8, 2, mat_index=1)
    return kc.finalize_object(bm, "SM_PropBreadTray", ["M_Timber", "M_Bread"])


def build_bread_loaves():
    """Two loose flat loaves (market goods, cook-shop counter)."""
    bm = kc.new_bmesh()
    add_dome(bm, 0.00, 0.00, 0.0, 0.09, 0.06, 0.03, 8, 2)
    add_dome(bm, 0.17, 0.09, 0.0, 0.09, 0.06, 0.03, 8, 2)
    return kc.finalize_object(bm, "SM_PropBreadLoaves", ["M_Bread"])


def build_brazier():
    """A standing tripod brazier, unlit. [A] — the watch fire / offering
    brazier; the temple and the gate watch each keep one."""
    bm = kc.new_bmesh()
    for k in range(3):
        a = math.radians(90.0 + k * 120.0)
        cx, cy = 0.13 * math.cos(a), 0.13 * math.sin(a)
        kc.add_box(bm, (cx - 0.025, cy - 0.025, 0.0), (cx + 0.025, cy + 0.025, 0.50))
    bowl = [
        (0.000, 0.475), (0.070, 0.475), (0.130, 0.500), (0.185, 0.575),
        (0.200, 0.660), (0.170, 0.675), (0.150, 0.665), (0.060, 0.600),
        (0.000, 0.585),
    ]
    revolve(bm, bowl, 10, mat_index=1)
    return kc.finalize_object(bm, "SM_PropBrazier", ["M_Timber", "M_Clay"])


def build_water_skin():
    """A filled hide waterskin hanging from a post. [A] — waterskins of hide
    hung by the well (schedules.csv water_carriers_draw)."""
    bm = kc.new_bmesh()
    kc.add_box(bm, (0.00, 0.00, 0.0), (0.06, 0.06, 1.05))
    kc.add_box(bm, (-0.16, 0.005, 1.02), (0.16, 0.055, 1.07))
    add_dome(bm, 0.03, 0.03, 1.00, 0.085, 0.085, 0.17, 8, 4, mat_index=1, up=False)
    return kc.finalize_object(bm, "SM_PropWaterSkin", ["M_Timber", "M_Hide"])


def build_sky_dome():
    """The sky dome: a 120 m inverted hemisphere (16 segments, rings every
    15 degrees plus one below-horizon skirt ring, bottom capped) viewed from
    inside — normals flipped inward, and the runtime material
    (/Game/Art/Sky/M_SkyDome, unlit two-sided gradient) carries the sky."""
    bm = kc.new_bmesh()
    radius = 120.0
    seg = 16
    angles = [0, 15, 30, 45, 60, 75, 90, 105]  # degrees from zenith
    rings = []
    for theta in angles:
        rad = math.radians(theta)
        r, z = radius * math.sin(rad), radius * math.cos(rad)
        ring = []
        for s in range(seg):
            a = (s / seg) * 2.0 * math.pi
            ring.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        rings.append(ring)
    for ri in range(len(rings) - 1):
        ra, rb = rings[ri], rings[ri + 1]
        for s in range(seg):
            t = (s + 1) % seg
            face = bm.faces.new([ra[s], ra[t], rb[t], rb[s]])
            face.material_index = 0
    centre = bm.verts.new((0.0, 0.0, rings[-1][0].co.z))
    for s in range(seg):  # cap the skirt bottom
        t = (s + 1) % seg
        face = bm.faces.new([centre, rings[-1][s], rings[-1][t]])
        face.material_index = 0
    obj = kc.finalize_object(bm, "SM_SkyDome", ["M_SkyDomeSlot"])
    flip_normals(obj)
    return obj


PROPS = [
    # (budget_key, builder, bevel_width_m or None)
    ("amphora", build_amphora, None),
    ("jar_stack", build_jar_stack, None),
    ("basket", build_basket, None),
    ("mat_roll", build_mat_roll, None),
    ("bench", build_bench, 0.004),
    ("dates_tray", build_dates_tray, None),
    ("bread_tray", build_bread_tray, None),
    ("bread_loaves", build_bread_loaves, None),
    ("brazier", build_brazier, None),
    ("water_skin", build_water_skin, None),
    ("sky_dome", build_sky_dome, None),
]


def main():
    args = kc.parse_args(sys.argv)
    out_dir = args.get("out", "art/generated/props")
    if not os.path.isabs(out_dir):
        out_dir = os.path.join(REPO_ROOT, out_dir)
    os.makedirs(out_dir, exist_ok=True)

    kc.clear_scene()
    manifest_rows = []
    failures = []
    print("props_gen: budgets (tris / budget)")
    for budget_key, build_fn, bevel in PROPS:
        obj = build_fn()
        tris, budget = finish_prop(obj, budget_key, bevel_width=bevel)
        if tris > budget:
            failures.append(f"{obj.name}: {tris} tris over budget {budget}")
        fbx_path, _glb = kc.export_piece(obj, out_dir)
        rel = os.path.relpath(fbx_path, REPO_ROOT)
        manifest_rows.append(
            [
                obj.name,
                rel,
                "mesh_street_prop",
                "original (procedural, generated code)",
                "docs/proposals/invented-ledger-dressing.md; Mesopotamian daily-life material culture [A]",
                "A",
            ]
        )
    kc.write_manifest_rows(manifest_rows)

    print(f"\nprops_gen: {len(PROPS)} props exported to {out_dir}")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(f" - {f}")
        sys.exit(1)
    print("props_gen: all props within budget")


if __name__ == "__main__":
    main()
