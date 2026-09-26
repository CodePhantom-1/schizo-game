"""Stage V batch 5 Task 3: the night sky (tools/art/star_dome.py, unreal/Content/Sim/stars.csv).

The catalogue itself (art/source/bsc5/, re-fetchable) is not in the repo: its test skips without it; the
rest read the tracked stars.csv, so CI checks the sky the game draws.
"""
import math
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "art"))
import star_dome as sd  # noqa: E402


def rows():
    return sd.read_csv(sd.OUT)


class CatalogueTest(unittest.TestCase):
    @unittest.skipUnless(os.path.exists(sd.CATALOGUE), "art/source/bsc5 not fetched (tools/art/sources.py bsc5)")
    def test_parses_the_naked_eye_sky(self):
        stars = sd.parse(sd.CATALOGUE)
        bright = [s for s in stars if s.mag < 5.0]
        self.assertGreaterEqual(len(bright), 1500)
        polaris = next(s for s in stars if s.hr == 424)
        self.assertEqual(polaris.con, "UMi")
        self.assertAlmostEqual(polaris.dec, 89.264, places=2)
        self.assertAlmostEqual(polaris.ra, 37.95, places=1)

    @unittest.skipUnless(os.path.exists(sd.CATALOGUE), "art/source/bsc5 not fetched (tools/art/sources.py bsc5)")
    def test_galactic_conversion_matches_the_catalogue(self):
        # The catalogue's own galactic coordinates, converted back, land on its RA/Dec (the Milky Way's frame).
        for s in sd.parse(sd.CATALOGUE)[::400]:
            ra, dec = sd.galactic_to_equatorial(s.glon, s.glat)
            self.assertLess(sd.separation(ra, dec, s.ra, s.dec), 0.3, s.hr)


class SkyTest(unittest.TestCase):
    def test_polaris_stands_31_degrees_up_in_the_north_at_any_hour(self):
        p = next(r for r in rows() if r["kind"] == "star" and r["hr"] == 424)
        base = (p["x"], p["y"], p["z"])
        for day, hour in ((0, 0.0), (37, 13.3), (211, 22.0), (359, 5.5)):
            v = sd.rotate(base, sd.POLE, sd.SIDEREAL_SIGN * math.radians(sd.sidereal(day, hour)))
            alt = math.degrees(math.asin(v[2]))
            self.assertAlmostEqual(alt, sd.LATITUDE, delta=1.0)
            self.assertLess(v[1], -0.8)  # north is -Y (SimCompass)
            self.assertLess(abs(v[0]), 0.03)

    def test_the_rotation_is_the_sky_turning(self):
        # The baked dome turned about the pole equals each star's true place at that sidereal time.
        for r in [r for r in rows() if r["kind"] == "star"][::97]:
            for lst in (0.0, 71.0, 190.0, 300.0):
                want = sd.world_dir(r["ra"], r["dec"], lst)
                got = sd.rotate((r["x"], r["y"], r["z"]), sd.POLE, sd.SIDEREAL_SIGN * math.radians(lst))
                self.assertLess(max(abs(a - b) for a, b in zip(want, got)), 1e-3)

    def test_stars_rise_in_the_east_and_culminate_in_the_south(self):
        # An equator star on the meridian stands 59 degrees up in the south (+Y); six hours later it sets west (-X).
        south = sd.world_dir(100.0, 0.0, 100.0)
        self.assertAlmostEqual(math.degrees(math.asin(south[2])), 90.0 - sd.LATITUDE, delta=0.01)
        self.assertGreater(south[1], 0.5)
        west = sd.world_dir(100.0, 0.0, 190.0)
        self.assertLess(west[0], -0.99)

    def test_the_zodiac_stars_sit_on_the_ecliptic(self):
        by_hr = {r["hr"]: r for r in rows() if r["kind"] == "star"}
        for hr in (1457, 3982, 6134, 5056):  # Aldebaran, Regulus, Antares, Spica
            self.assertIn(hr, by_hr)
            self.assertLess(abs(sd.ecliptic_latitude(by_hr[hr]["ra"], by_hr[hr]["dec"])), 6.0, hr)

    def test_the_zodiac_figures(self):
        lines = [r for r in rows() if r["kind"] == "zodiac"]
        cons = {r["con"] for r in lines}
        self.assertEqual(cons, set(sd.ZODIAC))
        for r in lines:  # every segment joins two stars of its figure: short arcs, never across the sky
            a, b = (r["x"], r["y"], r["z"]), (r["x2"], r["y2"], r["z2"])
            self.assertLess(math.degrees(math.acos(max(-1.0, min(1.0, sum(p * q for p, q in zip(a, b)))))), 30.0)

    def test_the_milky_way_follows_the_galactic_plane(self):
        milky = [r for r in rows() if r["kind"] == "milky"]
        self.assertGreater(len(milky), 300)
        lat = [abs(sd.galactic_latitude(r["ra"], r["dec"])) for r in milky]
        self.assertLess(sorted(lat)[len(lat) // 2], 8.0)  # the median blob lies within 8 degrees of the plane

    def test_the_dome_stays_under_20000_triangles(self):
        self.assertLessEqual(2 * len(rows()), 20000)

    def test_the_file_is_what_the_catalogue_makes(self):
        if not os.path.exists(sd.CATALOGUE):
            self.skipTest("art/source/bsc5 not fetched")
        with open(sd.OUT, encoding="utf-8") as f:
            self.assertEqual(sd.render(sd.build(sd.parse(sd.CATALOGUE))), f.read())


if __name__ == "__main__":
    unittest.main()
