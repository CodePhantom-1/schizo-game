#!/usr/bin/env python3
"""codex_gen.py — generates build/codex.md from the canon tables.

The in-game codex and the content database are the same tables (parent
architecture §3.3): this generator is the first consumer. Tagged rows carry
their tag into the codex so the player always sees what is canon.
"""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CANON = ROOT / "db" / "canon"
OUT = ROOT / "build" / "codex.md"

TITLE = {
    "cities": "The Cities", "customs": "Customs", "deities": "The Gods",
    "divination_forms": "Divination", "endings": "The Endings",
    "factions": "The Powers", "magick_forms": "The Forms of Magick",
    "pantheons": "The Pantheon Lists", "planetary_powers": "The Planetary Powers",
    "ranks": "Ranks", "regions": "The Four Directions",
    "story": "The Story", "terrain_schemes": "Element-Terrain Schemes",
    "world_lore": "The Wider World", "people": "Personages",
    "buildings": "Works", "rites": "Rites", "quests": "Quests", "dialogues": "Voices",
}


def main() -> int:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# The Codex (generated — do not edit; run tools/codex_gen.py)", ""]
    for table in sorted(CANON.glob("*.csv")):
        name = table.stem
        if name not in TITLE:
            continue  # technical tables (items, laws, ...) join the codex in later phases
        with table.open(newline="", encoding="utf-8") as fh:
            rows = [r for r in csv.DictReader(fh) if (r.get("id") or "").strip()]
        if not rows:
            continue
        lines.append(f"## {TITLE[name]}")
        lines.append("")
        for r in rows:
            tag = (r.get("tag") or "").strip()
            label = r.get("name") or r.get("id")
            if name == "dialogues" and r.get("speaker"):
                label = f"{r['speaker']} ({r['id']})"
            detail = r.get("canon") or r.get("domain_and_role") or r.get("effects") or r.get("method") or r.get("epithet_and_role") or r.get("canon_summary") or r.get("player_beat") or r.get("basis") or r.get("text") or ""
            mark = "" if tag == "CANON" else f" `[{tag}]`"
            lines.append(f"- **{label}**{mark} — {detail}" if detail else f"- **{label}**{mark}")
        lines.append("")
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(lines)} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
