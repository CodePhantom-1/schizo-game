"""Stage V batch 1 Task 3: the source adapters, against saved API responses (no network)."""
import json
import os
import sys
import unittest

HERE = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(HERE, "..", "art"))
import sources  # noqa: E402

FIX = os.path.join(HERE, "fixtures", "art")


def fixture(name):
    with open(os.path.join(FIX, name)) as f:
        return f.read()


class PolyHavenTest(unittest.TestCase):
    def test_picks_the_1k_jpg_maps(self):
        maps = sources.polyhaven_files(json.loads(fixture("ph_files_clay_block_wall.json")), "1k")
        self.assertEqual(set(maps), {"diff", "nor_gl", "rough", "ao", "disp"})
        self.assertTrue(maps["diff"].endswith("clay_block_wall_diff_1k.jpg"))
        self.assertIn("/1k/", maps["nor_gl"])

    def test_authors(self):
        self.assertEqual(sources.polyhaven_author(json.loads(fixture("ph_info_clay_block_wall.json"))), "Amal Kumar")


class AmbientCgTest(unittest.TestCase):
    def test_picks_the_requested_zip(self):
        url, name = sources.ambientcg_download(json.loads(fixture("acg_Bricks100.json")), "Bricks100", "1K-JPG")
        self.assertEqual(url, "https://ambientcg.com/get?file=Bricks100_1K-JPG.zip")
        self.assertEqual(name, "Bricks100_1K-JPG.zip")

    def test_missing_resolution_raises(self):
        with self.assertRaises(ValueError):
            sources.ambientcg_download(json.loads(fixture("acg_Bricks100.json")), "Bricks100", "3K-PNG")


class KenneyTest(unittest.TestCase):
    def test_parses_the_hashed_zip_and_the_licence(self):
        html = fixture("kenney_nature_kit.html")
        self.assertEqual(sources.kenney_zip_url(html),
                         "https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip")
        self.assertEqual(sources.kenney_license(html), "CC0-1.0")

    def test_a_page_without_cc0_is_not_cc0(self):
        self.assertEqual(sources.kenney_license("<html>all rights reserved</html>"), "")


class ExpectTest(unittest.TestCase):
    def test_licence_mismatch_raises(self):
        with self.assertRaises(ValueError):
            sources.expect_license({"id": "x", "license": "CC0-1.0"}, "CC-BY-4.0")
        with self.assertRaises(ValueError):
            sources.expect_license({"id": "x", "license": "CC0-1.0"}, "")
        sources.expect_license({"id": "x", "license": "CC0-1.0"}, "CC0-1.0")

    def test_git_licence_text(self):
        self.assertEqual(sources.license_from_text("Creative Commons Zero, CC0 1.0 Universal"), "CC0-1.0")
        self.assertEqual(sources.license_from_text("Attribution 4.0 International (CC BY 4.0)"), "CC-BY-4.0")
        self.assertEqual(sources.license_from_text("Attribution-NonCommercial 4.0"), "")

    def test_wishlist_rows_are_well_formed(self):
        rows = sources.wishlist()
        self.assertTrue(rows)
        ids = [r["id"] for r in rows]
        self.assertEqual(len(ids), len(set(ids)))
        for r in rows:
            self.assertIn(r["source"], sources.ADAPTERS, r["id"])
            self.assertTrue(r["asset"] and r["license"] and r["purpose"], r["id"])


if __name__ == "__main__":
    unittest.main()


class FreesoundTest(unittest.TestCase):
    """V-B5 T5: the sound adapter (art/sounds.csv), against a saved-style search response."""
    ROW = {"id": "city_night_crickets", "query_or_url": "crickets night ambience", "license": "CC0-1.0;CC-BY-4.0;CC-BY-3.0",
           "loop": "1"}

    def test_licences(self):
        self.assertEqual(sources.freesound_license("http://creativecommons.org/publicdomain/zero/1.0/"), "CC0-1.0")
        self.assertEqual(sources.freesound_license("https://creativecommons.org/licenses/by/4.0/"), "CC-BY-4.0")
        self.assertEqual(sources.freesound_license("http://creativecommons.org/licenses/by/3.0/"), "CC-BY-3.0")
        self.assertEqual(sources.freesound_license("http://creativecommons.org/licenses/by-nc/4.0/"), "")
        self.assertEqual(sources.freesound_license("http://creativecommons.org/licenses/sampling+/1.0/"), "")

    def test_the_search_asks_for_open_licences_and_a_loop_length(self):
        url = sources.freesound_search_url(self.ROW, "TOKEN")
        q = sources.urllib.parse.parse_qs(sources.urllib.parse.urlparse(url).query)
        self.assertEqual(q["query"], ["crickets night ambience"])
        self.assertIn('license:("Creative Commons 0" OR "Attribution")', q["filter"][0])
        self.assertIn("duration:[15.0 TO 300.0]", q["filter"][0])
        self.assertEqual(q["sort"], ["rating_desc"])

    def test_picks_the_best_open_result_that_fits(self):
        pick = sources.freesound_pick(json.loads(fixture("freesound_search.json")), self.ROW)
        self.assertEqual(pick["sound_id"], 103)  # 101 is NC, 102 is too long for a loop
        self.assertEqual((pick["author"], pick["license"]), ("fieldrec", "CC-BY-4.0"))
        self.assertTrue(pick["preview"].endswith("-hq.ogg"))

    def test_a_cc0_only_row_skips_attribution(self):
        pick = sources.freesound_pick(json.loads(fixture("freesound_search.json")), dict(self.ROW, license="CC0-1.0"))
        self.assertEqual(pick["sound_id"], 104)

    def test_nothing_fits_raises(self):
        with self.assertRaises(ValueError):
            sources.freesound_pick(json.loads(fixture("freesound_search.json")), dict(self.ROW, loop="0"))

    def test_no_key_stops_without_fetching(self):
        old = os.environ.pop("FREESOUND_API_KEY", None)
        try:
            self.assertEqual(sources.main_sounds([]), 2)
        finally:
            if old is not None:
                os.environ["FREESOUND_API_KEY"] = old


class SoundWishlistTest(unittest.TestCase):
    ZONES = {"city_day", "city_night", "market", "smithy", "bakery", "lagoon", "desert_wind", "precinct", "harbour",
             "rain", "sandstorm"}

    def test_the_wishlist(self):
        import csv  # noqa: PLC0415
        sys.path.insert(0, os.path.join(HERE, "..", "art"))
        import license_gate  # noqa: PLC0415
        with open(sources.SOUNDS, newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        with open(os.path.join(HERE, "..", "..", "db", "canon", "fauna.csv"), newline="", encoding="utf-8") as f:
            fauna = {r["id"] for r in csv.DictReader(f)}
        self.assertGreaterEqual(len(rows), 30)
        self.assertEqual(len({r["id"] for r in rows}), len(rows))
        beds = set()
        for r in rows:
            with self.subTest(r["id"]):
                self.assertEqual(r["source"], "freesound")
                self.assertTrue(set(r["license"].split(";")) <= set(license_gate.ALLOWED))
                self.assertIn(r["loop"], {"0", "1"})
                if r["zone"].startswith("cue:"):
                    self.assertEqual(r["loop"], "0")
                    self.assertTrue(set(r["zone"][4:].split(";")) <= fauna, r["zone"])
                else:
                    self.assertIn(r["zone"], self.ZONES)
                    self.assertEqual(r["loop"], "1")
                    beds.add(r["zone"])
        self.assertEqual(beds, self.ZONES)  # every zone has a bed
