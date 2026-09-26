"""Stage V batch 2 Task 1: flora.csv — the species the city grows, with habitat and season."""
import csv
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
FLORA = ROOT / "db" / "canon" / "flora.csv"
MESHES = ROOT / "art" / "scatter_meshes.csv"

HABITATS = {"shore", "water", "yard", "garden", "street_edge", "open", "wall_foot", "roof", "precinct", "tombs", "camp"}
GROUPS = {"tree", "shrub", "grass", "water", "flower", "crop", "prop"}
# world-art-plan §10 guard rails: substring test on the source file name
BANNED = ("barrel", "crate", "cactus", "pine", "oak", "mushroom", "windmill", "watermill", "chicken")


def rows(path):
    return list(csv.DictReader(open(path, newline="", encoding="utf-8")))


class FloraTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.flora = rows(FLORA)
        cls.meshes = {m["id"]: m for m in rows(MESHES)}
        cls.seasons = {s["id"] for s in rows(ROOT / "db" / "canon" / "seasons.csv")}

    def test_columns(self):
        self.assertEqual(list(self.flora[0].keys()),
                         ["id", "name", "group", "habitats", "seasons", "withers", "meshes", "per_100m2",
                          "scale_min", "scale_max", "tag", "source_ref"])

    def test_rows_valid(self):
        self.assertTrue(self.flora)
        for r in self.flora:
            with self.subTest(r["id"]):
                self.assertIn(r["group"], GROUPS)
                self.assertTrue(set(r["habitats"].split(";")) <= HABITATS, r["habitats"])
                if r["seasons"] != "all":
                    self.assertTrue(set(r["seasons"].split(";")) <= self.seasons, r["seasons"])
                self.assertIn(r["withers"], {"0", "1"})
                self.assertGreater(float(r["per_100m2"]), 0)
                self.assertLessEqual(float(r["scale_min"]), float(r["scale_max"]))

    def test_meshes_exist_and_allowed(self):
        for r in self.flora:
            for m in r["meshes"].split(";"):
                with self.subTest(flora=r["id"], mesh=m):
                    self.assertIn(m, self.meshes)
        for m in self.meshes.values():
            with self.subTest(mesh=m["id"]):
                name = m["file"].rsplit("/", 1)[-1].lower()
                for b in BANNED:
                    self.assertNotIn(b, name)


if __name__ == "__main__":
    unittest.main()
