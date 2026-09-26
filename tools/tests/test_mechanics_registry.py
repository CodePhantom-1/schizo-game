import pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import mechanics_registry as mr

ROW = "| {id} | Name | rule | src | {k} | Y | N | {ph} | {calls} |"


class Registry(unittest.TestCase):
    def test_parse_and_errors(self):
        text = "\n".join([
            "| ID | Mechanic | Rule | Source | K | C | U | Phase | Calls |",
            ROW.format(id="INV-01", k="Y", ph="P1", calls="give_item"),
            ROW.format(id="INV-01", k="Q", ph="Z9", calls="nope"),
        ])
        rows = mr.parse_registry(text)
        self.assertEqual([r["id"] for r in rows], ["INV-01", "INV-01"])
        errs = mr.errors_in(rows, {"give_item", "craft"})
        self.assertIn("duplicate id INV-01", errs)
        self.assertIn("INV-01: bad K status 'Q'", errs)
        self.assertIn("INV-01: bad phase 'Z9'", errs)
        self.assertIn("INV-01: cites unknown C API call 'nope'", errs)
        self.assertIn("C API call sim_world_craft is cited by no mechanic", errs)

    def test_real_registry_is_clean(self):
        self.assertEqual(mr.main(["--check"]), 0)


if __name__ == "__main__":
    unittest.main()
