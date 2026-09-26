import pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import canon_lint


class ItemColumns(unittest.TestCase):
    """FND-02: every item row carries its physical columns. MECH:FND-02"""

    def test_bad_values_are_errors(self):
        row = {"id": "x", "ui_category": "snack", "weight_g": "-1", "stack_max": "0",
               "spoil_days": "abc", "flags": "document|nonsense"}
        errs = canon_lint.item_row_errors("items.csv:2", row)
        self.assertEqual(len(errs), 5)

    def test_good_row_passes(self):
        row = {"id": "bread", "ui_category": "food", "weight_g": "250", "stack_max": "20",
               "spoil_days": "3", "flags": ""}
        self.assertEqual(canon_lint.item_row_errors("items.csv:2", row), [])

    def test_real_items_pass(self):
        self.assertEqual(canon_lint.main(), 0)


if __name__ == "__main__":
    unittest.main()
