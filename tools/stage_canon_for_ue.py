#!/usr/bin/env python3
"""stage_canon_for_ue.py — stages the canon for packaging into the game.

SimRuntime's USimWorldSubsystem reads the canon from Content/Sim/canon at
runtime (sim_world_create). The staged copies are GENERATED — never edited;
source of truth stays db/canon (validated by canon_lint). Regenerate here, at
packaging, or in CI. The staged copy mirrors db/canon exactly: a table removed
from db/canon is removed from the stage too. `--check` writes nothing and
exits 1 when the staged copy has drifted from db/canon (for CI).

Usage: python3 tools/stage_canon_for_ue.py [--check]
"""
import filecmp
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "db" / "canon"        # kernel-readable canon (id-keyed; the UE DataTables in data/ue are a separate artifact)
DST = ROOT / "unreal" / "Content" / "Sim" / "canon"


def main(argv: list[str] | None = None) -> int:
    check = "--check" in (sys.argv[1:] if argv is None else argv)
    if not SRC.exists():
        print(f"{SRC} missing — nothing to stage")
        return 1
    sources = {p.name: p for p in SRC.glob("*.csv")}
    staged = {p.name: p for p in DST.glob("*.csv")} if DST.exists() else {}
    stale = sorted(set(staged) - set(sources))  # tables removed from db/canon
    drift = sorted(n for n, p in sources.items()
                   if n not in staged or not filecmp.cmp(p, staged[n], shallow=False))
    if check:
        for n in drift:
            print(f"  out of date: {n}")
        for n in stale:
            print(f"  stale (not in db/canon): {n}")
        if drift or stale:
            print(f"STAGE STALE — {len(drift) + len(stale)} file(s); run tools/stage_canon_for_ue.py")
            return 1
        print("staged canon is current")
        return 0
    DST.mkdir(parents=True, exist_ok=True)
    for n in sorted(sources):
        shutil.copy2(sources[n], DST / n)
    for n in stale:
        (DST / n).unlink()
    print(f"staged {len(sources)} canon tables -> {DST.relative_to(ROOT)}"
          f" ({len(drift)} changed, {len(stale)} stale removed)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
