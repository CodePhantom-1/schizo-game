#!/usr/bin/env python3
"""stage_canon_for_ue.py — stages the canon for packaging into the game.

SimRuntime's USimWorldSubsystem reads the canon from Content/Sim/canon at
runtime (sim_world_create). The staged copies are GENERATED — never edited;
source of truth stays db/canon (validated by canon_lint). The staged directory
is git-ignored: regenerate here, at packaging, or in CI.

Usage: python3 tools/stage_canon_for_ue.py
"""
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "db" / "canon"        # kernel-readable canon (id-keyed; the UE DataTables in data/ue are a separate artifact)
DST = ROOT / "unreal" / "Content" / "Sim" / "canon"


def main() -> int:
    if not SRC.exists():
        print("data/ue missing — run tools/export_datatables.py first")
        return 1
    DST.mkdir(parents=True, exist_ok=True)
    staged = 0
    for csv_file in SRC.glob("*.csv"):
        shutil.copy2(csv_file, DST / csv_file.name)
        staged += 1
    print(f"staged {staged} canon tables -> {DST.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
