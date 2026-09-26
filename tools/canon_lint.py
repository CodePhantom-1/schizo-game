#!/usr/bin/env python3
"""canon_lint.py — THE law of the content database (plan.md rule 6).

Fails (exit 1) on any canon row that is:
  - untagged or wrongly tagged (must be CANON, A, INVENTED or OPEN)
  - missing id / source_ref, or an id with surrounding whitespace
  - duplicate id within a table
  - a CANON or A row with an empty source_ref
  - any field count mismatch against the table header
  - an items.csv row without its physical columns (FND-02: ui_category,
    weight_g, stack_max, spoil_days, flags)
OPEN rows are reported (they cannot ship) but do not fail the lint.
"""
import csv
import sys
from pathlib import Path

CANON = Path(__file__).resolve().parent.parent / "db" / "canon"
VALID_TAGS = {"CANON", "A", "INVENTED", "OPEN"}
REQUIRED = {"id", "tag", "source_ref"}
UI_CATEGORIES = {"food", "drink", "ingredient", "material", "tool", "weapon", "ammunition",
                 "armour", "shield", "clothing", "ritual", "document", "trade_good",
                 "station_part", "animal", "medicine", "treasure"}
ITEM_FLAGS = {"bound", "document", "floats", "fixture", "animal", "container"}


def item_row_errors(where: str, row: dict) -> list[str]:
    """FND-02: the physical columns every item row must carry."""
    errs = []
    rid = row.get("id", "")
    if row.get("ui_category") not in UI_CATEGORIES:
        errs.append(f"{where}: item '{rid}' ui_category '{row.get('ui_category')}' not in {sorted(UI_CATEGORIES)}")
    for col, lo in (("weight_g", 0), ("stack_max", 1), ("spoil_days", 0)):
        v = row.get(col) or ""
        if not v.isdigit() or int(v) < lo:
            errs.append(f"{where}: item '{rid}' {col} '{v}' must be an integer >= {lo}")
    for flag in filter(None, (row.get("flags") or "").split("|")):
        if flag.split(":")[0] not in ITEM_FLAGS:
            errs.append(f"{where}: item '{rid}' unknown flag '{flag}'")
    return errs


def main() -> int:
    errors: list[str] = []
    open_rows: list[str] = []
    tables = sorted(CANON.glob("*.csv"))
    if not tables:
        print(f"FAIL: no tables found in {CANON}")
        return 1

    for table in tables:
        with table.open(newline="", encoding="utf-8") as fh:
            reader = csv.DictReader(fh)
            header = reader.fieldnames or []
            missing = REQUIRED - set(header)
            if missing:
                errors.append(f"{table.name}: header missing required columns {sorted(missing)}")
                continue
            seen: set[str] = set()
            rows = 0
            for row in reader:
                rows += 1
                where = f"{table.name}:{reader.line_num}"
                # DictReader parks surplus fields under the None key and fills
                # missing ones with None: either way the row's width is wrong
                # (typically an unquoted comma) and every later column shifts.
                if None in row:
                    errors.append(f"{where}: {len(header) + len(row[None])} fields, header has {len(header)} (unquoted comma?)")
                elif any(v is None for v in row.values()):
                    width = sum(v is not None for v in row.values())
                    errors.append(f"{where}: {width} fields, header has {len(header)}")
                raw_id = row.get("id") or ""
                rid = raw_id.strip()
                if not rid:
                    errors.append(f"{where}: empty id")
                    continue
                if raw_id != rid:
                    errors.append(f"{where}: id '{raw_id}' has surrounding whitespace (the kernel matches ids verbatim)")
                if rid in seen:
                    errors.append(f"{where}: duplicate id '{rid}'")
                seen.add(rid)
                tag = (row.get("tag") or "").strip()
                if tag not in VALID_TAGS:
                    errors.append(f"{where}: bad tag '{tag}' (must be one of {sorted(VALID_TAGS)})")
                ref = (row.get("source_ref") or "").strip()
                if tag in {"CANON", "A", "INVENTED"} and not ref:
                    errors.append(f"{where}: {tag} row '{rid}' has no source_ref")
                if tag == "OPEN":
                    open_rows.append(f"{table.name}: {rid}")
                elif table.name == "items.csv":
                    errors.extend(item_row_errors(where, row))
            print(f"{table.name:<24} {rows:>3} rows  header ok")

    print()
    if open_rows:
        print(f"OPEN rows (cannot ship): {len(open_rows)}")
        for r in open_rows:
            print(f"  - {r}")
    if errors:
        print(f"\nLINT FAILED — {len(errors)} error(s):")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("LINT PASSED — every row tagged, sourced and unique.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
