"""Stage V batch 1 Task 5: the building grammar (world-art-plan §3-4), pure Python."""
import csv
import json
import math
import os
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "art"))
import building_grammar as bg  # noqa: E402


def rows(name):
    return list(csv.DictReader(open(ROOT / "db" / "canon" / name, newline="", encoding="utf-8")))


PLACES = rows("places.csv")
TYPES = {t["id"]: t for t in rows("building_types.csv")}


def aabb(p):
    """Axis-aligned bounds of a part (the exact bounds of its yawed footprint box)."""
    (x, y, z), (sx, sy, sz) = p["at"], p["size"]
    if p["shape"] == "prism":  # [r_base, r_top, h]
        sx = sy = 2 * max(sx, sy)
    elif p["shape"] == "dome":  # [rx, ry, h]
        sx, sy = 2 * sx, 2 * sy
    a = math.radians(p.get("yaw") or 0)
    c, s = abs(math.cos(a)), abs(math.sin(a))
    hx, hy = (c * sx + s * sy) / 2, (s * sx + c * sy) / 2
    return x - hx, x + hx, y - hy, y + hy, z, z + sz


class GrammarTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.specs = bg.all_specs(PLACES, TYPES)
        cls.by_id = {s["id"]: s for s in cls.specs}

    def test_every_building_place_gets_a_spec(self):
        want = {p["id"] for p in PLACES if bg.is_building(p)}
        self.assertEqual(set(self.by_id), want)
        self.assertGreater(len(want), 80)

    def test_deterministic(self):
        again = bg.all_specs(PLACES, TYPES)
        self.assertEqual(json.dumps(again, sort_keys=True), json.dumps(self.specs, sort_keys=True))

    def test_homes_are_all_different(self):
        homes = [s for s in self.specs if TYPES[s["typology"]]["group"] == "home"][:20]
        self.assertEqual(len(homes), 20)
        self.assertEqual(len({bg.signature(s) for s in homes}), 20)

    def test_parts_stay_on_the_plot(self):
        for s in self.specs:
            hw, hd = s["w"] / 2, s["d"] / 2
            for p in s["parts"]:
                x0, x1, y0, y1, z0, _ = aabb(p)
                self.assertGreaterEqual(z0, -0.01, (s["id"], p["tag"]))
                self.assertTrue(x0 >= -hw - 1.6 and x1 <= hw + 1.6 and y0 >= -hd - 1.6 and y1 <= hd + 1.6,
                                (s["id"], p["tag"], p["at"], p["size"]))

    def test_the_doorway_is_open_where_the_street_builder_puts_the_door(self):
        for s in self.specs:
            d = s["door"]
            self.assertAlmostEqual(d["x"], bg.door_x(s["w"]), places=4)
            sign = 1 if d["side"] == "+y" else -1
            ox0, ox1 = d["x"] - d["width"] / 2 + 0.05, d["x"] + d["width"] / 2 - 0.05
            for p in s["parts"]:
                if p["tag"] not in ("wall", "plinth"):
                    continue
                x0, x1, y0, y1, z0, z1 = aabb(p)
                in_band = y0 <= sign * (s["d"] / 2 - 0.2) <= y1
                blocks = in_band and x0 < ox1 and x1 > ox0 and z0 < d["height"] - 0.05 and z1 > 0.3
                self.assertFalse(blocks, (s["id"], p["tag"], p["at"]))

    def test_the_doorway_passage_is_clear(self):
        """Nothing stands in the walk through the door: 0.6 m out, 1 m in, from a step (0.45 m, UE's
        MaxStepHeight) up to the lintel."""
        for s in self.specs:
            d, hd = s["door"], s["d"] / 2
            sign = 1 if d["side"] == "+y" else -1
            px0, px1 = d["x"] - d["width"] / 2 + 0.05, d["x"] + d["width"] / 2 - 0.05
            py0, py1 = sorted((sign * (hd + 0.6), sign * (hd - 1.0)))
            for p in s["parts"]:
                if p["tag"] in ("roof", "window"):
                    continue  # overhead; wall insets
                x0, x1, y0, y1, z0, z1 = aabb(p)
                if p["shape"] in ("vault", "gable"):
                    continue  # shells: their openings are checked on the mesh (check_buildings.py)
                blocks = x0 < px1 and x1 > px0 and y0 < py1 and y1 > py0 and z0 < d["height"] - 0.05 and z1 > 0.45
                self.assertFalse(blocks, (s["id"], p["tag"], p["at"], p["size"]))

    def test_door_side_comes_from_the_data(self):
        for p in PLACES:
            if bg.is_building(p):
                self.assertEqual(self.by_id[p["id"]]["door"]["side"], p["door_side"])

    def test_every_trade_reads_without_a_sign(self):
        for s in self.specs:
            if TYPES[s["typology"]]["group"] != "home":
                self.assertTrue(any(p["tag"] == "marker" for p in s["parts"]), s["typology"])

    def test_small_footprints_are_still_buildings(self):
        for w, d in ((1, 1), (2, 2), (3, 2)):
            place = dict(PLACES[5], id=f"tiny_{w}x{d}", w_m=str(w), d_m=str(d), typology="home_hut")
            s = bg.spec(place, TYPES["home_hut"])
            self.assertTrue(any(p["tag"] == "wall" for p in s["parts"]))
            self.assertGreater(s["door"]["width"], 0.3)
            self.assertLessEqual(s["door"]["width"], w - 0.2)

    def test_an_unknown_typology_falls_back_to_its_wealth_home(self):
        place = dict(PLACES[5], id="mystery_place", typology="mystery")
        t = dict(TYPES["home_modest"], id="mystery", group="trade", look="something new")
        s = bg.spec(place, t)
        self.assertTrue(s["parts"])

    def test_triangle_budget(self):
        for s in self.specs:
            cap = 6000 if s["wealth"] in ("elite", "sacred") or s["w"] * s["d"] > 200 else 3500
            self.assertLessEqual(s["tris_est"], cap, s["id"])

    def test_materials_exist_in_the_atlas(self):
        import trim_atlas  # noqa: PLC0415
        cells = trim_atlas.cell_map()
        for s in self.specs:
            for p in s["parts"]:
                self.assertIn(p["mat"], cells, (s["id"], p["tag"]))

    def test_wealth_shows(self):
        mats = {w: set() for w in ("poor", "elite")}
        for s in self.specs:
            if s["wealth"] in mats:
                mats[s["wealth"]] |= {p["mat"].split("/")[0] for p in s["parts"] if p["tag"] != "marker"}  # a quern is stone anywhere
        self.assertTrue(mats["elite"] & {"whitewash", "cone_mosaic", "stone", "cedar", "glazed_lapis"})
        self.assertFalse(mats["poor"] & {"cone_mosaic", "glazed_lapis", "stone"})


if __name__ == "__main__":
    unittest.main()
