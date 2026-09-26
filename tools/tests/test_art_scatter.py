"""Stage V batch 2 Task 3: scatter.py — where every plant and prop stands."""
import csv
import math
import pathlib
import shutil
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "art"))
sys.path.insert(0, str(ROOT / "tools"))
import scatter  # noqa: E402
import city_layout  # noqa: E402

CANON = ROOT / "db" / "canon"


def rows(path):
    return list(csv.DictReader(open(path, newline="", encoding="utf-8")))


def inside(poly, x, y):
    """Point in a convex quad (either winding)."""
    signs = []
    for i in range(4):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % 4]
        signs.append((x2 - x1) * (y - y1) - (y2 - y1) * (x - x1) > 0)
    return all(signs) or not any(signs)


class ScatterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = scatter.build(CANON)
        cls.places = rows(CANON / "places.csv")
        cls.flora = rows(CANON / "flora.csv")
        cls.group = {}
        for f in cls.flora:
            for m in f["meshes"].split(";"):
                cls.group.setdefault(m, set()).add(f["group"])
        cls.pts = [(int(r["x_cm"]) / 100, int(r["y_cm"]) / 100, r) for r in cls.out]

    def test_deterministic_and_sorted(self):
        self.assertEqual(scatter.build(CANON), self.out)
        keys = [(r["mesh"], int(r["x_cm"]), int(r["y_cm"])) for r in self.out]
        self.assertEqual(keys, sorted(keys))

    def test_budget_and_meshes(self):
        meshes = {m["id"] for m in rows(ROOT / "art" / "scatter_meshes.csv")}
        self.assertLessEqual(len(self.out), 40000)
        self.assertGreater(len(self.out), 0)
        for r in self.out:
            self.assertIn(r["mesh"], meshes)

    def test_nothing_inside_a_grown_footprint(self):
        polys = [(p["id"], city_layout.corners(city_layout.grown(p, 1.0)))
                 for p in self.places if scatter.solid(p)]
        for x, y, r in self.pts:
            for pid, poly in polys:
                self.assertFalse(inside(poly, x, y), f"{r['mesh']} at ({x},{y}) inside {pid}")

    def test_doors_clear(self):
        doors = [scatter.door_point(p) for p in self.places if scatter.bg.is_building(p)]
        self.assertGreater(len(doors), 50)
        for x, y, r in self.pts:
            for dx, dy in doors:
                self.assertGreaterEqual(math.hypot(x - dx, y - dy), 2.5, f"{r['mesh']} at a door")

    def test_lagoon_holds_only_water_plants(self):
        (cx, cy), r0 = city_layout.O, city_layout.LAGOON_R
        for x, y, r in self.pts:
            if math.hypot(x - cx, y - cy) < r0 - 1:
                self.assertEqual(self.group[r["mesh"]], {"water"}, f"{r['mesh']} in the lagoon at ({x},{y})")

    def test_every_habitat_yields(self):
        used = {h for f in self.flora for h in f["habitats"].split(";")} - scatter.COUNTRYSIDE
        got = scatter.habitat_counts(CANON)
        for h in used:
            self.assertGreater(got.get(h, 0), 0, h)

    def test_door_point_by_hand(self):
        # caravan_yard_place: centre (126.1, 16.0), yaw 92, 22 x 16, door +y. Grid 22 x 16 ->
        # local door (0.5, 9.0); rotated 92 deg: (0.5c - 9s, 0.5s + 9c) + centre = (117.088, 16.186).
        p = next(p for p in self.places if p["id"] == "caravan_yard_place")
        x, y = scatter.door_point(p)
        self.assertAlmostEqual(x, 117.088, places=2)
        self.assertAlmostEqual(y, 16.186, places=2)

    def test_a_new_place_makes_the_output_stale(self):
        with tempfile.TemporaryDirectory() as d:
            for f in CANON.glob("*.csv"):
                shutil.copy(f, d)
            path = pathlib.Path(d) / "places.csv"
            places = rows(path)
            palm = next(r for r in self.out if r["mesh"].startswith("palm_"))
            fake = dict(next(p for p in places if p["id"] == "elder_house_1_place"))
            # a new house built where a palm stands: the old table would stand the palm inside it
            fake.update(id="fake_new_house_place", x_m=f"{int(palm['x_cm']) / 100:.1f}",
                        y_m=f"{int(palm['y_cm']) / 100:.1f}")
            with open(path, "a", newline="", encoding="utf-8") as fh:
                csv.DictWriter(fh, list(places[0].keys()), lineterminator="\n").writerow(fake)
            self.assertNotEqual(scatter.render(scatter.build(pathlib.Path(d))), scatter.render(self.out))


if __name__ == "__main__":
    unittest.main()
