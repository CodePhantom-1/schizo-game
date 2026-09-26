#!/usr/bin/env python3
"""mechanics_registry.py — no mechanic goes unplanned or untested.

Reads docs/mechanics-registry.md (one row per mechanic) and checks it:
  - ids are unique; K/C/U statuses and phases are valid
  - every C API call a row cites exists, and every declared C API call is
    cited by at least one row (no kernel function without a mechanic)
  - every canon table is either instanced (each row becomes a mechanic
    instance under a parent mechanic) or listed as lore
Writes docs/mechanics-instances.md (every canon row that must work in play,
e.g. each law, custom, event, talent and rite) and marks which mechanics and
instances have a test tagged `MECH:<id>` anywhere under the test roots.

Usage: python3 tools/mechanics_registry.py [--check]
  --check: exit 1 on a registry error (a stale instances file only warns).
"""
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "docs" / "mechanics-registry.md"
INSTANCES = ROOT / "docs" / "mechanics-instances.md"
CANON = ROOT / "db" / "canon"
TEST_ROOTS = [ROOT / "kernel" / "tests", ROOT / "tools" / "tests",
              ROOT / "unreal" / "Source" / "SchizoGame" / "Private" / "Tests"]

sys.path.insert(0, str(ROOT / "tools"))
import capi_reach  # noqa: E402  (declared C API calls)

ID = re.compile(r"^[A-Z]{2,3}-\d{2}$")
STATUS = {"Y", "P", "N", "-"}
PHASES = {"P0", "P1", "P2", "P3", "P4", "P5", "MQ", "AA"} | {chr(c) for c in range(ord("A"), ord("V"))}
MECH_TAG = re.compile(r"MECH:([A-Z]{2,3}-\d{2}|[a-z_]+:[A-Za-z0-9_]+)")

# table -> (parent mechanic, what "works" means for each of its rows)
INSTANCED = {
    "laws": ("LAW-13", "the crime triggers in play, runs its penalty ladder, and is tested"),
    "customs": ("CUS-01", "the custom has an in-world trigger and a reaction"),
    "events": ("EVT-01", "fires from its triggers with real participants, persistent consequences and staging"),
    "festivals": ("FST-01", "the day plays: gathering, place, market rule, exempt roles, roles for the player"),
    "calendar_days": ("TIM-05", "shown, and read by the systems its omen touches"),
    "months": ("TIM-02", "named in the HUD and the calendar"),
    "seasons": ("TIM-03", "bends schedules, prices, weather and events"),
    "weather": ("TIM-07", "visible, and changes heat, travel, raids and events"),
    "attributes": ("CHR-01", "drives each system it names"),
    "skills": ("CHR-05", "grows by every one of its uses, teachers and texts, and weights its outcomes"),
    "talents": ("CHR-08", "choosable, and its effect is felt and tested"),
    "callings": ("CHR-09", "choosable at its level, and its perks are felt"),
    "ranks": ("CHR-14", "reachable by its ceremony, and every gate it opens is enforced"),
    "arms": ("ARM-01", "equippable and visible; every stat is used"),
    "combat_styles": ("CMB-07", "its fighters fight, flee and surrender by the row"),
    "items": ("FND-02", "has an icon, a mesh, a weight, a way to get it and a use"),
    "recipes": ("CRF-01", "craftable at its station from the screen"),
    "foods": ("EAT-01", "exists as an item or recipe and can be eaten or offered"),
    "work_roles": ("ACT-04", "the job can be worked for its wage at its place"),
    "skill_teachings": ("ACT-10", "the teacher or text teaches it in play"),
    "rite_teachings": ("MAG-02", "the teacher or text teaches the rite in play"),
    "rites": ("MAG-01", "performable through the rite flow, with its effect"),
    "magick_forms": ("MGK-01", "a working system with rites, a codex page and tests (D-025)"),
    "divination_forms": ("DIV-10", "performable, answering with probabilities"),
    "planetary_powers": ("MGK-08", "every power on the list works as a rite power"),
    "deities": ("MAG-06", "favour, domain, cult site, festivals and taboos are live"),
    "factions": ("FAC-01", "standing, tiers, oaths and its verbs work"),
    "treaties": ("FAC-07", "read by the raid politics and shown on the faction screen"),
    "wild_groups": ("RAD-03", "forms, camps, raids and can be met through every camp verb"),
    "wild_places": ("WLA-01", "a walkable, reachable place with its loot and risks"),
    "wild_links": ("WLA-03", "travelled with its time and danger"),
    "wild_encounters": ("WLA-04", "met on the road with every stance"),
    "caravans": ("WLA-05", "departs, arrives, fills the market, can be escorted or robbed"),
    "transport_modes": ("WLA-09", "usable, at its speed, on its links"),
    "building_types": ("AV-10", "every building of the type is enterable and has something to do"),
    "places": ("AV-10", "a door, an owner, an interior and something to do"),
    "city_districts": ("AV-10", "built, and its character shows in play"),
    "people": ("NPC-03", "named, housed, scheduled, can be talked to, and lives a full day"),
    "schedules": ("NPC-02", "the role's hour plays out in the world"),
    "person_schedules": ("NPC-02", "the person's hour plays out in the world"),
    "quests": ("QST-01", "can be received, progressed, completed and failed"),
    "dialogues": ("DLG-01", "spoken by its speaker in its context"),
    "endings": ("END-06", "its unlock is tracked silently (the ending itself is main quest)"),
    "cities": ("WLA-13", "present through its people, goods, gods and rumours"),
}
# Canon that is lore, not mechanics: shown through the codex (DGT-01).
LORE = {"buildings", "calendar", "names", "pantheons", "regions", "story",
        "terrain_schemes", "world_lore"}


def parse_registry(text):
    """Rows of the registry tables: dicts with id, name, k, c, u, phase, calls."""
    rows = []
    for line in text.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 9 or not ID.match(cells[0]):
            continue
        rows.append({"id": cells[0], "name": cells[1], "k": cells[4], "c": cells[5],
                     "u": cells[6], "phase": cells[7], "calls": cells[8].split()})
    return rows


def errors_in(rows, declared):
    errs, seen, cited = [], set(), set()
    for r in rows:
        if r["id"] in seen:
            errs.append(f"duplicate id {r['id']}")
        seen.add(r["id"])
        for col in ("k", "c", "u"):
            if r[col] not in STATUS:
                errs.append(f"{r['id']}: bad {col.upper()} status '{r[col]}'")
        if r["phase"] not in PHASES:
            errs.append(f"{r['id']}: bad phase '{r['phase']}'")
        for call in r["calls"]:
            if call not in declared:
                errs.append(f"{r['id']}: cites unknown C API call '{call}'")
            cited.add(call)
    for call in sorted(declared - cited):
        errs.append(f"C API call sim_world_{call} is cited by no mechanic")
    return errs


def tagged_tests():
    found = set()
    for root in TEST_ROOTS:
        for p in root.rglob("*") if root.exists() else []:
            if p.is_file() and p.suffix in {".cpp", ".h", ".py", ".sh"}:
                found |= set(MECH_TAG.findall(p.read_text(errors="ignore")))
    return found


def canon_tables():
    return sorted(p.stem for p in CANON.glob("*.csv"))


def instances():
    out = []
    for table in sorted(INSTANCED):
        path = CANON / f"{table}.csv"
        if not path.exists():
            continue
        parent, need = INSTANCED[table]
        with path.open(newline="") as f:
            for row in csv.DictReader(f):
                if row.get("id") and row.get("tag") != "OPEN":
                    out.append((f"{table}:{row['id']}", parent, need))
    return out


def render_instances(inst, tested):
    lines = ["# MECHANIC INSTANCES — every canon row that must work in play",
             "",
             "Generated by `tools/mechanics_registry.py` from `db/canon/`. Do not edit by hand.",
             "Each row is one law, custom, event, talent, rite, place... that the game must make work.",
             "It is done when a test tagged `MECH:<instance>` passes. Parent rows are in "
             "[mechanics-registry.md](mechanics-registry.md).",
             "",
             f"**{sum(1 for i in inst if i[0] in tested)} of {len(inst)} instances tested.**",
             ""]
    current = None
    for inst_id, parent, need in inst:
        table = inst_id.split(":")[0]
        if table != current:
            current = table
            lines += ["", f"## {table} (parent {parent})", "", f"Works when: {need}.", "",
                      "| Instance | Tested |", "|---|---|"]
        lines.append(f"| `{inst_id}` | {'yes' if inst_id in tested else 'no'} |")
    return "\n".join(lines) + "\n"


def main(argv):
    check = "--check" in argv
    rows = parse_registry(REGISTRY.read_text())
    declared = {f.replace("sim_world_", "") for _, f in capi_reach.declared(capi_reach.INCLUDE)}
    errs = errors_in(rows, declared)
    for table in canon_tables():
        if table not in INSTANCED and table not in LORE:
            errs.append(f"canon table {table}.csv is neither instanced nor lore")
    tested = tagged_tests()
    inst = instances()
    text = render_instances(inst, tested)
    if check:
        # A warning, not an error: the instances are derived from canon at every
        # run, so coverage holds either way; the file is only the readable view.
        if not INSTANCES.exists() or INSTANCES.read_text() != text:
            print("warning: docs/mechanics-instances.md is stale: run tools/mechanics_registry.py")
    else:
        INSTANCES.write_text(text)
    done = sum(1 for r in rows if r["id"] in tested)
    print(f"{len(rows)} mechanics ({done} tested), {len(inst)} canon instances "
          f"({sum(1 for i in inst if i[0] in tested)} tested), {len(declared)} C API calls all cited"
          if not errs else f"{len(errs)} registry error(s):")
    for e in errs:
        print("  " + e)
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
