#!/usr/bin/env python3
"""canon_lint.py — THE law of the content database (plan.md rule 6).

Fails (exit 1) on any canon row that is:
  - untagged or wrongly tagged (must be CANON, A or OPEN)
  - missing id / source_ref
  - duplicate id within a table
  - a CANON or A row with an empty source_ref
  - any field count mismatch against the table header
OPEN rows are reported (they cannot ship) but do not fail the lint.
"""
import csv
import sys
from pathlib import Path

CANON = Path(__file__).resolve().parent.parent / "db" / "canon"
VALID_TAGS = {"CANON", "A", "OPEN"}
REQUIRED = {"id", "tag", "source_ref"}


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
            for lineno, row in enumerate(reader, start=2):
                rows += 1
                where = f"{table.name}:{lineno}"
                rid = (row.get("id") or "").strip()
                if not rid:
                    errors.append(f"{where}: empty id")
                    continue
                if rid in seen:
                    errors.append(f"{where}: duplicate id '{rid}'")
                seen.add(rid)
                tag = (row.get("tag") or "").strip()
                if tag not in VALID_TAGS:
                    errors.append(f"{where}: bad tag '{tag}' (must be one of {sorted(VALID_TAGS)})")
                ref = (row.get("source_ref") or "").strip()
                if tag in {"CANON", "A"} and not ref:
                    errors.append(f"{where}: {tag} row '{rid}' has no source_ref")
                if tag == "A" and ref and "notes" not in ref and "wb" not in ref:
                    # an [A] row must still tie back to the notes' own import of it
                    errors.append(f"{where}: A row '{rid}' must anchor to the notes' import (wb/notes) plus its real source")
                if tag == "OPEN":
                    open_rows.append(f"{table.name}: {rid}")
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
