"""Stage V batch 1 Task 4: the palette lock and the trim atlas (materials x wear states)."""
import os
import random
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "art"))
import stylize  # noqa: E402
import trim_atlas  # noqa: E402


class StylizeTest(unittest.TestCase):
    def test_every_pixel_is_a_palette_colour(self):
        pal = stylize.load_palette()
        rng = np.random.default_rng(1)
        img = Image.fromarray(rng.integers(0, 256, (300, 400, 3), dtype=np.uint8))
        out = np.asarray(stylize.lock(img, pal, 128))
        self.assertEqual(out.shape, (128, 128, 3))
        allowed = {tuple(c) for c in pal}
        self.assertTrue({tuple(p) for p in out.reshape(-1, 3)} <= allowed)

    def test_wrap_blur_keeps_a_tile_seamless(self):
        a = np.zeros((64, 64, 3), np.float32)
        a[:, :4] = 255  # a stripe on the left edge: blurring must bleed it across the right edge too
        b = stylize.wrap_blur(a, 2.0)
        self.assertGreater(b[:, -1].mean(), 10)

    def test_palette_is_well_formed(self):
        pal = stylize.load_palette()
        self.assertGreaterEqual(len(pal), 32)
        self.assertLessEqual(len(pal), 256)
        self.assertEqual(len({tuple(c) for c in pal}), len(pal))


class AtlasTest(unittest.TestCase):
    def test_cells_cover_every_material_and_wear_once(self):
        cells = trim_atlas.cell_map()
        self.assertEqual(len(cells), len(trim_atlas.MATERIALS) * len(trim_atlas.WEAR))
        self.assertEqual(len(set(map(tuple, cells.values()))), len(cells))
        self.assertEqual(len(cells), trim_atlas.GRID * trim_atlas.GRID)

    def test_cell_uv(self):
        last = f"{trim_atlas.MATERIALS[-1]}/{trim_atlas.WEAR[-1]}"
        self.assertEqual(trim_atlas.cell(*last.split("/")), (0.875, 0.875, 1.0, 1.0))
        self.assertEqual(trim_atlas.cell(trim_atlas.MATERIALS[0], "fresh"), (0.0, 0.0, 0.125, 0.125))

    def test_every_material_has_a_tile_size(self):
        for m in trim_atlas.MATERIALS:
            self.assertGreater(trim_atlas.TILE_M[m], 0)


@unittest.skipUnless(os.path.isdir(os.path.join(trim_atlas.SRC, "ph_clay_block_wall")), "fetch the sources first (tools/art/sources.py)")
class BuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.png = trim_atlas.build(cls.tmp.name)
        cls.img = np.asarray(Image.open(cls.png).convert("RGB")).astype(np.int32)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_size(self):
        self.assertEqual(self.img.shape, (trim_atlas.GRID * trim_atlas.CELL,) * 2 + (3,))

    def test_only_palette_colours(self):
        allowed = {tuple(c) for c in stylize.load_palette()}
        self.assertTrue({tuple(p) for p in self.img[::7, ::7].reshape(-1, 3)} <= allowed)

    def test_every_cell_tiles_seamlessly(self):
        n = trim_atlas.CELL
        rnd = random.Random(0)
        for key, (c, r) in trim_atlas.cell_map().items():
            cell = self.img[r * n:(r + 1) * n, c * n:(c + 1) * n]
            edge = np.abs(cell[:, 0] - cell[:, -1]).mean()
            i, j = rnd.sample(range(n), 2)
            far = np.abs(cell[:, i] - cell[:, j]).mean()
            self.assertLessEqual(edge, far + 12, key)

    def test_wear_states_differ(self):
        n = trim_atlas.CELL
        cm = trim_atlas.cell_map()
        for m in trim_atlas.MATERIALS:
            (c0, r0), (c3, r3) = cm[f"{m}/fresh"], cm[f"{m}/cracked"]
            a = self.img[r0 * n:(r0 + 1) * n, c0 * n:(c0 + 1) * n]
            b = self.img[r3 * n:(r3 + 1) * n, c3 * n:(c3 + 1) * n]
            self.assertGreater(np.abs(a - b).mean(), 3, m)

    def test_deterministic(self):
        with tempfile.TemporaryDirectory() as d:
            again = trim_atlas.build(d)
            with open(again, "rb") as a, open(self.png, "rb") as b:
                self.assertEqual(a.read(), b.read())


if __name__ == "__main__":
    unittest.main()
