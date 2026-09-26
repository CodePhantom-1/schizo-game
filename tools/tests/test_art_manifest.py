"""Stage V batch 1 Task 2: the asset manifest and its licence gate."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "art"))
import license_gate  # noqa: E402
import manifest  # noqa: E402


def row(**kw):
    r = {c: "" for c in manifest.COLUMNS}
    r.update(id="x", file="art/x.png", kind="texture_map", license="CC0-1.0", author="ambientCG",
             url="https://ambientcg.com/a/X", acquired="2026-09-26", ai="false", tag="A")
    r.update(kw)
    return r


class GateTest(unittest.TestCase):
    def test_allowlisted_licences_pass(self):
        rows = [row(id=f"a{i}", license=lic) for i, lic in enumerate(license_gate.ALLOWED)]
        self.assertEqual(license_gate.check(rows), [])

    def test_forbidden_licences_fail_naming_the_id(self):
        for lic in ("CC-BY-NC-4.0", "CC-BY-SA-4.0", "CC-BY-ND-4.0", "GPL-3.0", "", "CC0"):
            problems = license_gate.check([row(id="bad", license=lic)])
            self.assertTrue(problems and "bad" in problems[0], lic)

    def test_attribution_needs_an_author_and_url(self):
        self.assertTrue(license_gate.check([row(license="CC-BY-4.0", author="")]))
        self.assertTrue(license_gate.check([row(license="CC-BY-4.0", url="")]))

    def test_ai_rows_must_name_the_tool(self):
        self.assertTrue(license_gate.check([row(ai="true", source_ref="")]))
        self.assertEqual(license_gate.check([row(ai="true", source_ref="sd.cpp sd15 prompt: mudbrick")]), [])

    def test_ai_flag_is_boolean(self):
        self.assertTrue(license_gate.check([row(ai="maybe")]))

    def test_duplicate_ids_fail(self):
        self.assertTrue(license_gate.check([row(id="d"), row(id="d")]))

    def test_credits_render_attribution_and_qal(self):
        md = license_gate.credits_md([
            row(id="t", license="CC-BY-4.0", author="Jane", url="https://e.x/t"),
            row(id="q", license="LicenseRef-QAL-1.0", author="Quaternius", url="https://quaternius.com"),
            row(id="c", license="CC0-1.0"),
        ])
        self.assertIn("t by Jane (https://e.x/t), CC BY 4.0", md)
        self.assertIn("Quaternius", md)
        self.assertIn("ambientCG", md)  # CC0 authors are thanked too


class ManifestTest(unittest.TestCase):
    def test_upsert_is_idempotent_and_sorted_by_first_insert(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "assets.csv")
            manifest.upsert(p, [row(id="b"), row(id="a")])
            manifest.upsert(p, [row(id="b", author="new")])
            rows = manifest.load(p)
            self.assertEqual([r["id"] for r in rows], ["b", "a"])
            self.assertEqual(rows[0]["author"], "new")

    def test_upsert_keeps_unknown_fields_blank(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "assets.csv")
            manifest.upsert(p, [{"id": "z", "file": "f", "kind": "k", "license": "CC0-1.0"}])
            self.assertEqual(manifest.load(p)[0]["sha256"], "")

    def test_sha256(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "f")
            open(p, "wb").write(b"abc")
            self.assertEqual(manifest.sha256(p)[:8], "ba7816bf")

    def test_legacy_rows_get_spdx_licences(self):
        self.assertEqual(manifest.legacy(["a", "f", "k", "original (procedural)", "r", "A"])["license"], "LicenseRef-Original")
        self.assertEqual(manifest.legacy(["a", "f", "k", "CC0", "https://ambientcg.com/a/X", "A"])["license"], "CC0-1.0")
        q = manifest.legacy(["a", "f", "k", "CC0", "https://quaternius.com/packs/x.html", "A"])
        self.assertEqual(q["license"], "LicenseRef-QAL-1.0")  # a pack page saying CC0 does not override QAL
        self.assertEqual(license_gate.check([manifest.legacy(["a", "f", "k", "CC-BY-NC", "r", "A"])])[0][:2], "a:")

    def test_the_real_manifest_passes(self):
        self.assertEqual(license_gate.check(manifest.load(manifest.PATH)), [])


if __name__ == "__main__":
    unittest.main()
