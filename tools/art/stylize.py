"""The texture stylizer: blur, BOX-downscale, lock to the project palette (D-023; research §6).

Mixed sources (Poly Haven photos, ambientCG, procedural cells) end up in one set of colours,
art/palette.csv, so the world reads as one hand-painted style. The blur wraps round the edges,
so a tiling texture stays tiling. Plain Python: Pillow + numpy.
"""
import csv
import os

import numpy as np
from PIL import Image, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PALETTE = os.path.join(REPO, "art", "palette.csv")


def load_palette(path=PALETTE):
    with open(path, newline="") as f:
        return [(int(r["r"]), int(r["g"]), int(r["b"])) for r in csv.DictReader(f)]


def _palette_image(palette):
    flat = [v for c in palette for v in c]
    flat += list(palette[0]) * (256 - len(palette))
    img = Image.new("P", (1, 1))
    img.putpalette(flat)
    return img


def wrap_blur(arr, radius):
    """Gaussian blur of an HxWx3 array as if it tiled forever (so its edges still meet)."""
    if radius <= 0:
        return arr
    a = np.asarray(arr, np.float32)
    h, w = a.shape[:2]
    big = np.tile(a, (3, 3, 1))
    img = Image.fromarray(np.clip(big, 0, 255).astype(np.uint8))
    out = np.asarray(img.filter(ImageFilter.GaussianBlur(radius)), np.float32)
    return out[h:2 * h, w:2 * w]


def quantize(arr, palette):
    """Nearest palette colour for every pixel, no dithering (pixel-crisp flat colour)."""
    img = Image.fromarray(np.clip(np.asarray(arr), 0, 255).astype(np.uint8), "RGB")
    return img.quantize(palette=_palette_image(palette), dither=Image.Dither.NONE).convert("RGB")


def downscale(arr, size):
    img = Image.fromarray(np.clip(np.asarray(arr), 0, 255).astype(np.uint8), "RGB")
    return np.asarray(img.resize((size, size), Image.Resampling.BOX), np.float32)


def lock(img, palette, size, blur=0.6):
    """Any image -> a size x size tile in palette colours: wrap-blur (source scale) -> BOX -> quantize."""
    a = downscale(np.asarray(img.convert("RGB"), np.float32), size * 2)  # BOX keeps a tile tiling
    return quantize(downscale(wrap_blur(a, blur * 2), size), palette)
