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
