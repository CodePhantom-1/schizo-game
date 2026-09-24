#!/usr/bin/env python3
"""coverage_check.py — the notes-coverage audit, automated (game-design.md §12).

Every element of the designer's notes must have a home in the blueprint AND in
the database. Fails (exit 1) if any element's evidence is missing.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# element -> list of (path relative to repo, regex that must match)
ELEMENTS: list[tuple[str, list[tuple[str, str]]]] = [
    ("voice toggle",                       [("docs/endings.md", r"optionally voiced"), ("docs/mechanics.md", r"optionally voiced")]),
    ("prisoner backstory",                 [("docs/game-design.md", r"prisoner"), ("db/canon/story.csv", r"prisoner")]),
    ("the groups; cult and rebels",        [("docs/game-design.md", r"mysterious cult"), ("db/canon/factions.csv", r"brotherhood_of_the_serpent")]),
    ("end arc: needed elsewhere",          [("docs/endings.md", r"needed elsewhere")]),
    ("city-state, business, alliances",    [("docs/game-design.md", r"city-state"), ("docs/endings.md", r"kingship")]),
    ("four endings, one hidden",           [("db/canon/endings.csv", r"ending_2_sumerian"), ("docs/endings.md", r"hidden")]),
    ("The Empire (lore)",                  [("db/canon/factions.csv", r"the_empire"), ("docs/world-bible.md", r"Law Giver")]),
    ("The Neo-Sumerian Rebellion (lore)",  [("db/canon/factions.csv", r"neo_sumerian_rebellion")]),
    ("The Barbarians (lore)",              [("db/canon/factions.csv", r"the_barbarians")]),
    ("The Brotherhood (lore)",             [("docs/world-bible.md", r"Ante-Diluvian")]),
    ("the four ending canons",             [("docs/endings.md", r"council of warlords")]),
    ("the eight cities",                   [("db/canon/cities.csv", r"city_of_jewels"), ("db/canon/cities.csv", r"city_of_the_warrior_spirit")]),
    ("the origin myth of Urash",           [("docs/world-bible.md", r"armada of boats"), ("db/canon/deities.csv", r"water_lord")]),
    ("the pantheon lists",                 [("db/canon/pantheons.csv", r"seven_who_decree")]),
    ("8 deities of medicine (missing)",    [("db/canon/pantheons.csv", r"eight_deities_of_medicine")]),
    ("Gravestone / e-ab-kur-irkalla-ki",   [("db/canon/cities.csv", r"gravestone")]),
    ("the opening (180 years, prisoner)",  [("docs/world-bible.md", r"180 years"), ("db/canon/story.csv", r"prisoner")]),
    ("element-terrain schemes",            [("db/canon/terrain_schemes.csv", r"scheme_a"), ("db/canon/terrain_schemes.csv", r"scheme_b")]),
    ("the directions table",               [("db/canon/regions.csv", r"taurus_d"), ("db/canon/regions.csv", r"tree_of_life")]),
    ("the wider-world requirements",       [("db/canon/world_lore.csv", r"giza")]),
    ("the real Sumerian city lists",       [("db/sources/notes.md", r"Eridu"), ("docs/world-bible.md", r"Shuruppak")]),
    ("the forms of magick",                [("db/canon/magick_forms.csv", r"zisurru"), ("docs/game-design.md", r"Sex magick")]),
    ("the forms of divination",            [("db/canon/divination_forms.csv", r"aeromancy"), ("db/canon/divination_forms.csv", r"pessomancy")]),
    ("the planetary power-lists",          [("db/canon/planetary_powers.csv", r"venus"), ("db/canon/planetary_powers.csv", r"mercury")]),
    ("the Venus entry (commissioned)",     [("db/canon/planetary_powers.csv", r"sacred rites of Inanna"), ("docs/world-bible.md", r"designer's explicit commission")]),
    ("the War Age",                        [("docs/world-bible.md", r"War Age")]),
    ("'degenerate forces'",                [("docs/endings.md", r"degenerate forces")]),
]


def main() -> int:
    cache: dict[tuple[str, str], str] = {}
    failures: list[str] = []
    for element, evidence in ELEMENTS:
        missing = []
        for rel, pattern in evidence:
            path = ROOT / rel
            if path.is_file():
                text = cache.setdefault(rel, path.read_text(encoding="utf-8"))
                if not re.search(pattern, text, re.IGNORECASE):
                    missing.append(f"{rel} !~ /{pattern}/")
            elif path.is_dir():
                hit = any(re.search(pattern, p.read_text(encoding="utf-8"), re.IGNORECASE)
                          for p in path.glob("**/*") if p.is_file())
                if not hit:
                    missing.append(f"{rel}/** !~ /{pattern}/")
            else:
                missing.append(f"{rel} (missing file)")
        if missing:
            failures.append(f"{element}: {', '.join(missing)}")
        else:
            print(f"  ok  {element}")

    print()
    if failures:
        print(f"COVERAGE FAILED — {len(failures)} element(s) without a home:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print(f"COVERAGE PASSED — all {len(ELEMENTS)} notes elements have a blueprint home.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
