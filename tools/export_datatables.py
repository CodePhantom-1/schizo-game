#!/usr/bin/env python3
"""export_datatables.py — generates UE-ready DataTables from the canon tables.

The parent architecture pattern (§3): `data/` holds engine-ready exports
generated from the source tables — never edited by hand. UE DataTables import
CSV with the row key in a leading `Name` column; every canon column is
preserved (including `tag`, so the engine and codex can filter CANON/A/
INVENTED/OPEN at runtime).

Usage: python3 tools/export_datatables.py   (writes data/ue/*.csv)
"""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CANON = ROOT / "db" / "canon"
OUT = ROOT / "data" / "ue"

# Tables the engine consumes as DataTables at bring-up (the rest join per phase).
TABLES = [
    "cities", "deities", "endings", "events", "factions", "items", "laws",
    "names", "pantheons", "planetary_powers", "places", "ranks", "regions",
    "rite_teachings", "rites", "schedules", "seasons", "skills", "story", "world_lore",
    "festivals", "person_schedules",  # K-2
    "arms", "combat_styles",  # W4-B: combat
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = []
    for table in TABLES:
        src = CANON / f"{table}.csv"
        dst = OUT / f"{table}.csv"
        rows = []
        if src.exists():
            with src.open(newline="", encoding="utf-8") as f:
                rows = [r for r in csv.reader(f) if r]  # blank lines are not rows
        if len(rows) < 2 or rows[0][0] != "id":
            if rows and rows[0][0] != "id":
                print(f"SKIP {table}: first column is not 'id'")
            # Header-only (or vanished) tables export later, when they have
            # content, and a previous export must not linger as live data.
            dst.unlink(missing_ok=True)
            continue
        header = rows[0]
        ue_rows = [["Name"] + header[1:]] + [[r[0]] + r[1:] for r in rows[1:]]
        with dst.open("w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerows(ue_rows)
        manifest.append(f"{table}: {len(ue_rows) - 1} rows")
        print(f"  {dst.relative_to(ROOT)}  ({len(ue_rows) - 1} rows)")
    (OUT / "MANIFEST.md").write_text(
        "# Engine-ready exports (generated — do not edit)\n\n"
        "Run `python3 tools/export_datatables.py` to regenerate from db/canon.\n\n"
        + "\n".join(f"- {m}" for m in manifest)
        + "\n",
        encoding="utf-8",
    )
    print(f"manifest written — {len(manifest)} tables")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
