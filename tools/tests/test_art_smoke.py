"""Stage V batch 5 Task 2: smoke.csv — every soot source of the building grammar, in the world frame, current."""
import csv
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class SmokeTest(unittest.TestCase):
    def test_current(self):
        r = subprocess.run([sys.executable, str(ROOT / "tools" / "art" / "building_grammar.py"), "--check-smoke"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_rows_name_places(self):
        places = {p["id"] for p in csv.DictReader(open(ROOT / "db" / "canon" / "places.csv", encoding="utf-8"))}
        rows = list(csv.DictReader(open(ROOT / "unreal" / "Content" / "Sim" / "smoke.csv", encoding="utf-8")))
        self.assertGreater(len(rows), 20)
        for r in rows:
            self.assertIn(r["place_id"], places)
            self.assertGreaterEqual(int(r["z_cm"]), 0)


if __name__ == "__main__":
    unittest.main()
