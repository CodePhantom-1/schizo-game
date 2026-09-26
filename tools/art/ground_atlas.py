"""The ground atlas (Stage V batch 3): one tiling DETAIL cell per ground kind, for M_Terrain.

  python3 tools/art/ground_atlas.py [--out art/generated/tex]

Writes T_Ground.png (GRID x GRID cells of CELL px, row-major in KINDS order = ESimGround) and
ground_cells.json, plus a preview in art/review/ground_atlas.png. Each cell is luminance-normalised
(mean 0.5, std about 0.12, a faint hue from its source): M_Terrain multiplies Tommy's vertex colour
by Detail x 2, so the colour stays his and a flat grey cell would reproduce today's terrain exactly.
Photo cells come from the CC0 Poly Haven sets (seamless, so a whole-image downscale still tiles;
variants are rotations, flips and blur, never crops); salt crust, dune ripples and tell sherds are
procedural and periodic.
"""
import glob
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stylize  # noqa: E402
from trim_atlas import fnoise, voronoi_edges  # noqa: E402 -- periodic noise and crack lines

REPO = stylize.REPO
SRC = os.path.join(REPO, "art", "source")
GRID, CELL = 4, 512
KINDS = ["silt", "irrigated", "cracked", "salt", "sand", "gravel", "reed_mud", "beach", "road", "street",
         "bed", "rock", "hills", "dune", "tell", "spare"]
MEAN, STD, HUE = 0.5, 0.12, 0.25  # target luminance mean and spread; how much of the source hue survives


def photo(source, blur=1.0, rot=0, flip=False):
    """A Poly Haven diffuse map as a CELL x CELL float image (0..1), still tiling."""
    files = sorted(glob.glob(os.path.join(SRC, source, "*_diff_*")))
    if not files:
        raise FileNotFoundError(f"no diffuse map in art/source/{source}: run tools/art/sources.py {source}")
    a = stylize.downscale(np.asarray(Image.open(files[0]).convert("RGB"), np.float32), CELL * 2)
    a = stylize.downscale(stylize.wrap_blur(a, blur), CELL) / 255.0
    a = np.rot90(a, rot)
    return np.ascontiguousarray(a[:, ::-1] if flip else a)


def normalise(rgb, std=STD):
    """Grey detail with a faint hue: luminance to mean 0.5 / the given spread, hue pulled toward grey."""
    lum = rgb.mean(axis=2)
    z = (lum - lum.mean()) / (lum.std() + 1e-6)
    grey = MEAN + std * z
    hue = rgb / (lum[..., None] + 1e-6)
    out = grey[..., None] * (1 + HUE * (hue - 1))
    for _ in range(4):  # clipping moves the mean: settle it
        out = np.clip(out / out.mean() * MEAN, 0, 1)
    return out


def salt(rng):
    """White crust plates over silt: polygon plates, darker cracks between them."""
    base = photo("ph_dry_mud_field", blur=2.0)
    edges = voronoi_edges(rng, 40, CELL)
    crack = np.clip(edges / 6.0, 0, 1)[..., None]
    crust = 0.55 + 0.35 * fnoise(rng, CELL, beta=2.6)[..., None]
    return base * (1 - crack) * 0.4 + crust * crack


def dune(rng):
    """Wind ripples: a periodic stripe field (integer periods, so it tiles) warped by noise."""
    base = photo("ph_coast_sand", blur=1.5, rot=1)
    yy, xx = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    warp = fnoise(rng, CELL, beta=3.0) * 2 * np.pi
    ripple = 0.5 + 0.5 * np.sin(2 * np.pi * (xx / CELL * 24 + yy / CELL * 4) + warp)
    return base * (0.8 + 0.4 * ripple[..., None])


def tell(rng):
    """A tell's surface: silt speckled with red-brown sherds and pale lime flecks."""
    base = photo("ph_dry_mud_field", blur=1.0, rot=2)
    out = base.copy()
    for colour, count in (((0.62, 0.34, 0.22), 260), ((0.85, 0.82, 0.72), 90)):
        for _ in range(count):
            x, y, r = rng.integers(0, CELL), rng.integers(0, CELL), rng.integers(2, 6)
            yy, xx = np.ogrid[-r:r + 1, -r:r + 1]
            mask = (xx * xx + yy * yy) <= r * r
            ys, xs = (np.arange(y - r, y + r + 1) % CELL), (np.arange(x - r, x + r + 1) % CELL)  # wraps: tiles
            sub = out[np.ix_(ys, xs)]
            sub[mask] = colour
            out[np.ix_(ys, xs)] = sub
    return out


def cell_image(kind, rng):
    if kind == "silt":
        return normalise(photo("ph_dry_mud_field"))
    if kind == "irrigated":
        return normalise(photo("ph_brown_mud_dry"))
    if kind == "cracked":
        return normalise(photo("ph_mud_cracked_dry"), std=0.16)
    if kind == "salt":
        return normalise(salt(rng), std=0.14)
    if kind == "sand":
        return normalise(photo("ph_coast_sand"))
    if kind == "gravel":
        return normalise(photo("ph_dry_ground_rocks"), std=0.14)
    if kind == "reed_mud":
        return normalise(photo("ph_brown_mud_dry", blur=1.5, rot=1, flip=True))
    if kind == "beach":
        return normalise(photo("ph_coast_sand", blur=2.0, rot=2), std=0.08)
    if kind == "road":
        return normalise(photo("ph_dry_mud_field", blur=3.0, rot=1), std=0.07)
    if kind == "street":
        return normalise(photo("ph_dry_mud_field", blur=4.0, rot=3, flip=True), std=0.05)
    if kind == "bed":
        return normalise(photo("ph_brown_mud_dry", blur=4.0, rot=2), std=0.06)
    if kind == "rock":
        return normalise(photo("ph_rock_boulder_dry"), std=0.15)
    if kind == "hills":
        return normalise(photo("ph_dry_ground_rocks", blur=3.0, rot=1), std=0.1)
    if kind == "dune":
        return normalise(dune(rng), std=0.1)
    if kind == "tell":
        return normalise(tell(rng))
    return np.full((CELL, CELL, 3), MEAN, np.float32)  # spare: flat grey (no change to the colour)


def cell_map():
    return {k: [i % GRID, i // GRID] for i, k in enumerate(KINDS)}


def build(out_dir):
    atlas = np.zeros((GRID * CELL, GRID * CELL, 3), np.uint8)
    for i, k in enumerate(KINDS):
        a = cell_image(k, np.random.default_rng(3000 + i))
        c, r = i % GRID, i // GRID
        atlas[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL] = np.clip(np.round(a * 255), 0, 255).astype(np.uint8)
    os.makedirs(out_dir, exist_ok=True)
    png = os.path.join(out_dir, "T_Ground.png")
    Image.fromarray(atlas).save(png, optimize=False)
    with open(os.path.join(out_dir, "ground_cells.json"), "w", newline="\n") as f:
        json.dump({"grid": GRID, "cell_px": CELL, "cells": cell_map(), "kinds": KINDS}, f, indent=1, sort_keys=True)
    return png


def main(argv):
    out = argv[argv.index("--out") + 1] if "--out" in argv else os.path.join(REPO, "art", "generated", "tex")
    png = build(out)
    prev = os.path.join(REPO, "art", "review", "ground_atlas.png")
    Image.open(png).resize((1024, 1024), Image.Resampling.BOX).save(prev)
    print(f"ground_atlas: {png} ({len(KINDS)} cells) + {os.path.relpath(prev, REPO)}")


if __name__ == "__main__":
    main(sys.argv[1:])
