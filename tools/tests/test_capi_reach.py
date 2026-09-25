import pathlib, sys, tempfile, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import capi_reach


class Reach(unittest.TestCase):
    def test_declarations_and_references(self):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            (d / "inc").mkdir(); (d / "ue").mkdir()
            (d / "inc" / "CApiX.h").write_text(
                "int sim_world_attack(SimWorld* w);\nint sim_world_frobnicate(const SimWorld* w);\n"
                "// sim_world_in_comment(\n")
            (d / "ue" / "A.cpp").write_text("sim_world_attack(H);\n")
            decls = capi_reach.declared(d / "inc")
            self.assertEqual(sorted(f for _, f in decls), ["sim_world_attack", "sim_world_frobnicate"])
            reached = capi_reach.referenced([d / "ue"])
            self.assertIn("sim_world_attack", reached)
            self.assertNotIn("sim_world_frobnicate", reached)
            self.assertEqual(capi_reach.stage_for("sim_world_attack"), "K")
            self.assertEqual(capi_reach.stage_for("sim_world_frobnicate"), "unassigned")


if __name__ == "__main__":
    unittest.main()
