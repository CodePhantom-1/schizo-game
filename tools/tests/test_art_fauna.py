"""Stage V batch 4 Task 1: fauna.csv — the animals of the city and the land, period-correct (world-art-plan §8, §10)."""
import csv
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
CANON = ROOT / "db" / "canon"
GROUPS = {"herd", "work", "city", "bird_flock", "bird_wader", "bird_raptor", "wild_night", "water", "small"}
HABITATS = {"shore", "water", "yard", "garden", "street_edge", "open", "wall_foot", "roof", "precinct", "tombs", "camp",
            "levee", "field", "bank", "desert", "tell_top"}
BANNED = ("chicken", "hen", "rooster", "camel", "dromedary")
EQUIDS = ("horse", "kunga", "onager")  # §10: horse-kin only at the eastern peoples' and the elite's places
EQUID_PLACES = {"@warchief_tent", "@envoys_house"}


def rows(path):
    return list(csv.DictReader(open(path, newline="", encoding="utf-8")))


class FaunaTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fauna = rows(CANON / "fauna.csv")
        cls.models = {m["id"]: m for m in rows(ROOT / "art" / "fauna_models.csv")}
        cls.typologies = {t["id"] for t in rows(CANON / "building_types.csv")}

    def test_columns(self):
        self.assertEqual(list(self.fauna[0].keys()),
                         ["id", "name", "group", "habitats", "hours", "group_min", "group_max", "per_place",
                          "herd_share", "model", "tag", "source_ref"])

    def test_rows_valid(self):
        for r in self.fauna:
            with self.subTest(r["id"]):
                self.assertIn(r["group"], GROUPS)
                for h in r["habitats"].split(";"):
                    if h.startswith("@"):
                        self.assertIn(h[1:], self.typologies, h)
                    else:
                        self.assertIn(h, HABITATS)
                m = re.fullmatch(r"(\d\d)-(\d\d)", r["hours"])
                self.assertIsNotNone(m, r["hours"])
                self.assertTrue(0 <= int(m[1]) <= 24 and 0 <= int(m[2]) <= 24)
                self.assertLessEqual(int(r["group_min"]), int(r["group_max"]))
                self.assertGreaterEqual(int(r["per_place"]), 0)
                if r["model"]:
                    self.assertIn(r["model"], self.models)

    def test_period_guard_rails(self):
        for r in self.fauna:
            text = (r["id"] + " " + r["model"]).lower()
            for b in BANNED:
                self.assertNotIn(b, text, r["id"])
            if any(e in text for e in EQUIDS):
                self.assertTrue(set(r["habitats"].split(";")) <= EQUID_PLACES, f"{r['id']} outside the allowed places")
        for m in self.models.values():
            for b in BANNED:
                self.assertNotIn(b, (m["id"] + " " + m["file"]).lower(), m["id"])

    def test_herd_shares_sum_to_one(self):
        herd = [float(r["herd_share"]) for r in self.fauna if r["group"] == "herd"]
        self.assertTrue(herd)
        self.assertAlmostEqual(sum(herd), 1.0, places=6)
        for r in self.fauna:
            if r["group"] != "herd":
                self.assertEqual(float(r["herd_share"]), 0.0, r["id"])

    def test_night_predators_keep_night_hours(self):
        for r in self.fauna:
            if r["group"] == "wild_night":
                start, end = map(int, r["hours"].split("-"))
                self.assertGreater(start, end, f"{r['id']} must wrap midnight")

    def test_variants_name_a_base(self):
        for m in self.models.values():
            if m["variant_of"]:
                self.assertIn(m["variant_of"], self.models, m["id"])


if __name__ == "__main__":
    unittest.main()
