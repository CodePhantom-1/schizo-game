"""The trim atlas: every building material of world-art-plan §2 in four wear states, one texture.

  python3 tools/art/trim_atlas.py [--out art/generated/tex]

Writes T_Trim.png (GRID x GRID cells of CELL px, every cell tiling on its own) and trim_cells.json
(cell per "material/wear", each material's real-world tile size in metres), plus a preview in
art/review/trim_atlas.png. Photo cells come from the CC0 sources (art/sources.csv), procedural
cells (cone mosaic, glazed brick, textiles) are drawn here; the wear states are procedural
overlays; everything is then locked to art/palette.csv. The building mesher (building_mesh.py)
maps each face into its cell; M_Building wraps UV0 inside the cell (UV1 = the cell).
"""
import glob
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stylize  # noqa: E402

REPO = stylize.REPO
SRC = os.path.join(REPO, "art", "source")
GRID, CELL = 8, 256
MATERIALS = ["mudbrick", "plaster", "whitewash", "fired_brick", "bitumen", "reed", "palm", "cedar",
             "cone_mosaic", "glazed_lapis", "ochre_band", "packed_earth", "stone", "textile_madder",
             "textile_indigo", "goat_hair"]
WEAR = ["fresh", "worn", "crumbling", "cracked"]
# Metres of wall one cell covers (texel density: CELL px per TILE_M metres).
TILE_M = {"mudbrick": 2.0, "plaster": 2.0, "whitewash": 2.0, "fired_brick": 1.5, "bitumen": 2.0, "reed": 1.0,
          "palm": 1.0, "cedar": 1.0, "cone_mosaic": 1.0, "glazed_lapis": 1.5, "ochre_band": 2.0,
          "packed_earth": 3.0, "stone": 1.5, "textile_madder": 1.0, "textile_indigo": 1.0, "goat_hair": 1.0}
# What shows through when the surface wears away.
SUBSTRATE = {"plaster": "mudbrick", "whitewash": "plaster", "ochre_band": "plaster", "cone_mosaic": "plaster",
             "glazed_lapis": "fired_brick"}


def cell_map():
    return {f"{m}/{w}": [(mi * len(WEAR) + wi) % GRID, (mi * len(WEAR) + wi) // GRID]
            for mi, m in enumerate(MATERIALS) for wi, w in enumerate(WEAR)}


def cell(material, wear):
    c, r = cell_map()[f"{material}/{wear}"]
    return (c / GRID, r / GRID, (c + 1) / GRID, (r + 1) / GRID)


# ---- tileable noise ----

def fnoise(rng, n=CELL, beta=2.2, lo=2):
    """Periodic 1/f noise in [0,1] (an inverse FFT, so it tiles exactly)."""
    fy = np.fft.fftfreq(n)[:, None] * n
    fx = np.fft.fftfreq(n)[None, :] * n
    f = np.sqrt(fx * fx + fy * fy)
    amp = np.where(f >= lo, 1.0 / np.maximum(f, 1) ** (beta / 2), 0)
    spec = amp * np.exp(2j * np.pi * rng.random((n, n)))
    a = np.real(np.fft.ifft2(spec))
    return (a - a.min()) / (np.ptp(a) + 1e-9)


def voronoi_edges(rng, points, n=CELL):
    """Periodic Voronoi: distance to the cell border (small = on a crack line)."""
    pts = rng.random((points, 2)) * n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    d = []
    for ox in (-n, 0, n):
        for oy in (-n, 0, n):
            for px, py in pts:
                d.append(np.hypot(xx - px - ox, yy - py - oy))
    d = np.sort(np.stack(d), axis=0)
    return d[1] - d[0]


def pal(name):
    return np.array(PAL[name], np.float32)


# ---- base cells ----

def photo(pattern, blur=1.0):
    files = sorted(glob.glob(os.path.join(SRC, pattern)))
    if not files:
        raise FileNotFoundError(f"no source matches art/source/{pattern}: run tools/art/sources.py")
    a = stylize.downscale(np.asarray(Image.open(files[0]).convert("RGB"), np.float32), CELL * 2)
    return stylize.downscale(stylize.wrap_blur(a, blur), CELL)


def remap(a, dark, light, gamma=1.0):
    """Luminance of a -> a ramp from palette colour dark to light (keeps the photo's detail, sets its hue)."""
    lum = a.mean(axis=2, keepdims=True) / 255.0
    lum = (lum - lum.min()) / (np.ptp(lum) + 1e-9)
    return pal(dark) + (pal(light) - pal(dark)) * lum ** gamma


def courses(a, rng, bh, bw, joint):
    """Brick courses in running bond: darker mortar joints, each brick a touch lighter or darker."""
    yy, xx = np.mgrid[0:CELL, 0:CELL]
    row = yy // bh
    col = (xx + (bw // 2) * (row % 2)) // bw
    tone = rng.random((CELL // bh + 1, CELL // bw + 2))[row, col % (CELL // bw)][..., None]
    a = a * (0.9 + 0.2 * tone)
    j = ((yy % bh) < 2) | (((xx + (bw // 2) * (row % 2)) % bw) < 2)
    return np.where(j[..., None], a * 0.55 + pal(joint) * 0.45, a)


def cone_mosaic(rng):
    """Clay cones pressed into the wall, heads painted red, black and cream in zigzag bands (Uruk, [A])."""
    a = np.empty((CELL, CELL, 3), np.float32)
    a[:] = pal("mud_2")
    k = 16  # cone pitch in px
    yy, xx = np.mgrid[0:k, 0:k]
    r = np.hypot(xx - k / 2 + 0.5, yy - k / 2 + 0.5)
    cols = [pal("madder_1"), pal("bitumen_1"), pal("cream_0")]
    for j in range(CELL // k):
        for i in range(CELL // k):
            band = (j + abs((i % 8) - 4)) // 2 % 3  # zigzag; period divides the cell, so it tiles
            c = cols[band] * (0.92 + 0.16 * rng.random())
            shade = np.clip(1.15 - r / (k / 2) * 0.45, 0.6, 1.15)[..., None]
            blk = a[j * k:(j + 1) * k, i * k:(i + 1) * k]
            blk[:] = np.where((r < k / 2 - 1)[..., None], c * shade, blk)
    return a


def glazed_lapis(rng):
    """Glazed brick: lapis courses in running bond with a gold-yellow band every fourth course."""
    a = np.empty((CELL, CELL, 3), np.float32)
    a[:] = pal("bitumen_2")
    bh, bw = 32, 64
    for j in range(CELL // bh):
        off = (bw // 2) * (j % 2)
        base = pal("gold_1") if j % 4 == 3 else pal("lapis_1")
        for i in range(-1, CELL // bw + 1):
            x0 = i * bw + off
            c = base * (0.9 + 0.2 * rng.random())
            for x in range(x0 + 2, x0 + bw - 2):
                a[j * bh + 2:(j + 1) * bh - 2, x % CELL] = c
            hl = pal("gold_2") if j % 4 == 3 else pal("lapis_3")
            for x in range(x0 + 6, x0 + bw - 12):
                a[j * bh + 5:j * bh + 7, x % CELL] = hl  # the glaze catching the light
    return a


def textile(rng, a_name, b_name, stripe):
    yy, xx = np.mgrid[0:CELL, 0:CELL]
    weave = ((xx // 2 + yy // 2) % 2).astype(np.float32)[..., None]
    base = pal(a_name) * (0.9 + 0.1 * weave)
    band = ((yy // stripe) % 4 == 0)[..., None]
    a = np.where(band, pal(b_name) * (0.9 + 0.1 * weave), base)
    return a * (0.9 + 0.2 * fnoise(rng)[..., None])


def goat_hair(rng):
    xx = np.mgrid[0:CELL, 0:CELL][1]
    stripe = (xx // 16) % 5
    a = np.where((stripe == 4)[..., None], pal("cream_0") * 0.8, np.where((stripe % 2 == 0)[..., None], pal("goat_0"), pal("goat_1")))
    return a * (0.85 + 0.3 * fnoise(rng, beta=1.2)[..., None])


def base_cell(m, rng):
    if m == "mudbrick":
        a = 0.6 * remap(photo("ph_clay_block_wall/*diff*"), "mud_1", "mud_6") + 0.4 * remap(photo("Bricks100/*Color*"), "mud_1", "mud_5")
        return courses(a, rng, 16, 48, "mud_1")
    if m == "plaster":
        return remap(photo("ph_clay_plaster/*diff*"), "plaster_0", "plaster_3")
    if m == "whitewash":
        return remap(photo("ph_clay_plaster/*diff*"), "lime_0", "lime_2", 0.6)
    if m == "fired_brick":
        return remap(photo("ph_slumped_mortar_brick/*diff*"), "brick_0", "brick_3")
    if m == "bitumen":
        return remap(photo("ph_bitumen/*diff*"), "bitumen_0", "bitumen_2")
    if m == "reed":
        return remap(photo("ph_reed_roof/*diff*"), "reed_0", "reed_4")
    if m == "palm":
        return remap(photo("ph_palm_bark/*diff*"), "palm_0", "palm_2", 0.9)
    if m == "cedar":
        return remap(photo("ph_wood_planks/*diff*"), "cedar_0", "cedar_2")
    if m == "cone_mosaic":
        return cone_mosaic(rng)
    if m == "glazed_lapis":
        return glazed_lapis(rng)
    if m == "ochre_band":
        return remap(photo("ph_patterned_clay_plaster/*diff*"), "ochre_0", "ochre_2")
    if m == "packed_earth":
        return remap(photo("ph_dry_mud_field/*diff*"), "mud_2", "mud_6")
    if m == "stone":
        return remap(photo("ph_sandstone_brick/*diff*"), "stone_0", "stone_3")
    if m == "textile_madder":
        return textile(rng, "madder_1", "cream_0", 16)
    if m == "textile_indigo":
        return textile(rng, "indigo_1", "saffron_0", 24)
    if m == "goat_hair":
        return goat_hair(rng)
    raise KeyError(m)


# ---- wear ----

def wear_cell(base, substrate, wear, rng):
    if wear == "fresh":
        return base
    n = fnoise(rng)[..., None]
    stain = 1.0 - 0.18 * fnoise(rng, beta=1.6)[..., None]
    if wear == "worn":
        flake = n > 0.72
        return np.where(flake, substrate * 0.92, base * stain)
    if wear == "crumbling":
        flake = n > 0.55
        pits = (voronoi_edges(rng, 60) > 7)[..., None] & (fnoise(rng)[..., None] > 0.7)
        a = np.where(flake, substrate * 0.85, base * stain * 0.93)
        return np.where(pits, a * 0.55, a)
    # cracked: sun-bleached, a crack network, salt bloom (the drought)
    bleached = base * 0.8 + pal("mud_7") * 0.2
    cracks = ((voronoi_edges(rng, 22) < 1.1) & (fnoise(rng, beta=2.6) > 0.45))[..., None]
    salt = (fnoise(rng, beta=1.4)[..., None] > 0.78)
    a = np.where(salt, pal("salt_0") * 0.95, bleached)
    return np.where(cracks, pal("mud_0"), a)


def build(out_dir):
    global PAL
    palette = stylize.load_palette()
    PAL = {name: c for name, c in zip(_palette_names(), palette)}
    atlas = np.zeros((GRID * CELL, GRID * CELL, 3), np.uint8)
    bases = {m: base_cell(m, np.random.default_rng(100 + i)) for i, m in enumerate(MATERIALS)}
    for mi, m in enumerate(MATERIALS):
        sub = bases[SUBSTRATE[m]] if m in SUBSTRATE else bases[m] * 0.7 + pal("mud_1") * 0.3
        for wi, w in enumerate(WEAR):
            rng = np.random.default_rng(1000 + mi * 10 + wi)
            a = wear_cell(bases[m], sub, w, rng)
            c, r = cell_map()[f"{m}/{w}"]
            atlas[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL] = np.asarray(stylize.quantize(a, palette))
    os.makedirs(out_dir, exist_ok=True)
    png = os.path.join(out_dir, "T_Trim.png")
    Image.fromarray(atlas).save(png, optimize=False)
    with open(os.path.join(out_dir, "trim_cells.json"), "w") as f:
        json.dump({"grid": GRID, "cell_px": CELL, "cells": cell_map(), "tile_m": TILE_M}, f, indent=1, sort_keys=True)
    return png


def _palette_names():
    import csv  # noqa: PLC0415
    with open(stylize.PALETTE, newline="") as f:
        return [r["name"] for r in csv.DictReader(f)]


PAL = {}


def main(argv):
    out = argv[argv.index("--out") + 1] if "--out" in argv else os.path.join(REPO, "art", "generated", "tex")
    png = build(out)
    prev = os.path.join(REPO, "art", "review", "trim_atlas.png")
    Image.open(png).resize((1024, 1024), Image.Resampling.NEAREST).save(prev)
    print(f"trim_atlas: {png} ({len(cell_map())} cells) + {os.path.relpath(prev, REPO)}")


if __name__ == "__main__":
    main(sys.argv[1:])
