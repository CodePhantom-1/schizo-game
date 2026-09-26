"""Stage V batch 3 Task 1: the ground atlas — one luminance-normalised detail cell per ground kind."""
import json
import os
import random
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "art"))
import ground_atlas  # noqa: E402


class KindsTest(unittest.TestCase):
    def test_sixteen_kinds_in_atlas_order(self):
        self.assertEqual(len(ground_atlas.KINDS), 16)
        self.assertEqual(len(set(ground_atlas.KINDS)), 16)
        self.assertEqual(ground_atlas.KINDS[0], "silt")
        self.assertEqual(ground_atlas.KINDS[-1], "spare")
        self.assertEqual(ground_atlas.GRID ** 2, len(ground_atlas.KINDS))


@unittest.skipUnless(os.path.isdir(os.path.join(ground_atlas.SRC, "ph_dry_mud_field")),
                     "fetch the sources first (tools/art/sources.py)")
class BuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.png = ground_atlas.build(cls.tmp.name)
        cls.img = np.asarray(Image.open(cls.png).convert("RGB")).astype(np.float32) / 255.0
        cls.cells = json.load(open(os.path.join(cls.tmp.name, "ground_cells.json")))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def cell(self, i):
        n, g = ground_atlas.CELL, ground_atlas.GRID
        c, r = i % g, i // g
        return self.img[r * n:(r + 1) * n, c * n:(c + 1) * n]

    def test_size(self):
        n = ground_atlas.GRID * ground_atlas.CELL
        self.assertEqual(self.img.shape, (n, n, 3))

    def test_every_cell_is_luminance_normalised(self):
        for i, k in enumerate(ground_atlas.KINDS):
            self.assertAlmostEqual(float(self.cell(i).mean()), 0.5, delta=0.01, msg=k)

    def test_every_cell_tiles_seamlessly(self):
        n = ground_atlas.CELL
        rnd = random.Random(0)
        for i, k in enumerate(ground_atlas.KINDS):
            c = self.cell(i) * 255
            i0, j0 = rnd.sample(range(n), 2)
            for edge, far in ((np.abs(c[:, 0] - c[:, -1]).mean(), np.abs(c[:, i0] - c[:, j0]).mean()),
                              (np.abs(c[0] - c[-1]).mean(), np.abs(c[i0] - c[j0]).mean())):
                self.assertLessEqual(edge, far + 12, k)

    def test_cells_json_maps_every_kind(self):
        self.assertEqual(self.cells["grid"], ground_atlas.GRID)
        self.assertEqual(sorted(self.cells["cells"]), sorted(ground_atlas.KINDS))
        for i, k in enumerate(ground_atlas.KINDS):
            self.assertEqual(self.cells["cells"][k], [i % ground_atlas.GRID, i // ground_atlas.GRID])

    def test_deterministic(self):
        with tempfile.TemporaryDirectory() as d:
            again = ground_atlas.build(d)
            with open(again, "rb") as a, open(self.png, "rb") as b:
                self.assertEqual(a.read(), b.read())


if __name__ == "__main__":
    unittest.main()
