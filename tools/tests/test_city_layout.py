import csv, math, pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import city_layout

ROOT = pathlib.Path(__file__).resolve().parents[2]


class Layout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = city_layout.layout()
        cls.by_id = {r["id"]: r for r in cls.rows}

    def test_about_110_buildings_every_one_typed(self):
        types = {r["id"] for r in csv.DictReader(open(ROOT / "db/canon/building_types.csv"))}
        city = [r for r in self.rows if r["quarter"] != "beyond_the_gate"]
        self.assertGreaterEqual(len(city), 100)
        self.assertLessEqual(len(city), 125)
        for r in self.rows:
            self.assertIn(r["typology"], types, r["id"])
            self.assertIn(r["wealth"], {"poor", "modest", "comfortable", "elite", "civic", "sacred"}, r["id"])

    def test_existing_places_keep_their_ids(self):
        old = [r["id"] for r in csv.DictReader(open(ROOT / "db/canon/places.csv"))]
        for pid in old:
            self.assertIn(pid, self.by_id)

    def test_no_two_footprints_overlap(self):
        boxes = [(r["id"], city_layout.corners(r)) for r in self.rows if r["quarter"] != "beyond_the_gate"]
        for i, (a, ca) in enumerate(boxes):
            for b, cb in boxes[i + 1:]:
                self.assertFalse(city_layout.overlap(ca, cb), f"{a} overlaps {b}")

    def test_arc_quarters_stay_in_their_band(self):
        for r in self.rows:
            if r["typology"] in ("ziggurat", "lighthouse"):
                continue  # the ziggurat straddles the band; the lighthouse stands out on its mole
            band = city_layout.band_of(r["quarter"])
            if band is None:
                continue
            rad = math.hypot(float(r["x_m"]) - city_layout.O[0], float(r["y_m"]) - city_layout.O[1])
            self.assertTrue(band[0] - 1 <= rad <= band[1] + 1, f'{r["id"]} at r={rad:.0f}m outside {band}')

    def test_door_side_faces_the_ring_street(self):
        # The street builder's rule (V-B1 T5 moved it into the data): arc buildings nearer the
        # lagoon than the mid-band open on local +y (outward), the rest on local -y.
        split = (city_layout.LAGOON_R + city_layout.WALL_R) / 2
        clusters = {"reed_quarter", "newcomers_terraces", "garden_of_tombs", "beyond_the_gate"}
        for r in self.rows:
            rad = math.hypot(float(r["x_m"]) - city_layout.O[0], float(r["y_m"]) - city_layout.O[1])
            want = "+y" if r["quarter"] not in clusters and rad < split else "-y"
            self.assertEqual(city_layout.door_side(r), want, r["id"])
        head = next(csv.reader(open(ROOT / "db/canon/places.csv")))
        self.assertIn("door_side", head)

    def test_clusters_keep_to_their_ground(self):
        for r in self.rows:
            rad = [math.hypot(x - city_layout.O[0], y - city_layout.O[1]) for x, y in city_layout.corners(r)]
            if r["quarter"] == "reed_quarter":
                self.assertLess(max(rad), 100.0, f'{r["id"]} is not in the lagoon')
            if r["quarter"] in ("newcomers_terraces", "garden_of_tombs"):
                self.assertGreater(min(rad), 212.0, f'{r["id"]} is inside the walls')

    def test_nothing_on_land_stands_in_the_sea(self):
        for r in self.rows:
            if r["typology"] == "lighthouse":
                continue  # on its mole
            for x, _ in city_layout.corners(r):
                self.assertLess(x, city_layout.COAST_X, f'{r["id"]} stands in the sea')

    def test_deterministic(self):
        self.assertEqual(city_layout.layout(), self.rows)

    def test_monuments_where_the_design_puts_them(self):
        zig = self.by_id["temple_front_place"]
        gate = self.by_id["moon_gate_place"]
        light = self.by_id["great_lighthouse_place"]
        self.assertGreater(float(zig["y_m"]), city_layout.O[1] + 100)       # the northern belly
        self.assertLess(float(gate["x_m"]), city_layout.O[0] - 100)         # the western side
        self.assertAlmostEqual(math.hypot(float(gate["x_m"]) - city_layout.O[0], float(gate["y_m"]) - city_layout.O[1]),
                               city_layout.WALL_R, delta=1.0)                  # in the wall itself
        self.assertGreater(float(light["x_m"]), city_layout.O[0] + 150)     # the eastern horn, at the sea


if __name__ == "__main__":
    unittest.main()
