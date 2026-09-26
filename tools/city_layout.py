#!/usr/bin/env python3
"""city_layout.py — AA2 (D-025): the City of the Moon laid out as data.

The crescent of docs/city-of-the-moon.md, built from two canon tables:
city_districts.csv (the nine quarters: arc sectors round the lagoon, or
clusters) and building_types.csv (the catalogue of world-art-plan §4). The
building PROGRAM below says which buildings each quarter holds; the layout
places them — deterministically, no two overlapping — and fills each
quarter's remaining frontage with homes of its wealth. Every existing
places.csv row keeps its id (schedules and people point at them).

Writes db/canon/places.csv with footprint columns: x_m, y_m (centre, metres;
+x east, +y north, the old Moon Gate at the origin), yaw_deg (the building's
width runs along yaw), w_m, d_m, typology, wealth, quarter.

Usage: python3 tools/city_layout.py [--check]   (--check: exit 1 if places.csv would change)
"""
import csv
import math
import pathlib
import random
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CANON = ROOT / "db" / "canon"
PLACES = CANON / "places.csv"

# The crescent: its centre (metres), the lagoon inside radius 105 m, the wall at 175 m
# (the Prophet's citadel to 205 m); the coast runs at about x = 425 m (the wall reaches ~415 m).
COAST_X = 425.0
def _frame():
    with open(CANON / "city_districts.csv", newline="", encoding="utf-8") as fh:
        for r in csv.DictReader(fh):
            if r["id"] == "crescent_frame":
                return (float(r["cx_m"]), float(r["cy_m"])), float(r["r0_m"]), float(r["r1_m"])
    raise SystemExit("city_districts.csv has no crescent_frame row")


O, LAGOON_R, WALL_R = _frame()
GAP_M = 3.0          # lanes between neighbours
STEP_DEG = 0.25      # the scan's angular step

# (id, typology, name) per quarter, in placement order. An id already in
# places.csv keeps its row (only the footprint and quarter change).
PROGRAM = {
    "moon_gate_quarter": [
        ("moon_gate_place", "city_gate", None),
        ("gate_watch_post_place", "watch_post", None),
        ("gate_toll_house_place", "toll_house", "The Toll House"),
        ("caravan_yard_place", "caravan_yard", "The Caravan Yard and Road Inn"),
        ("brewery_tavern_place", "beerhouse", None),
        ("prison_barracks_place", "prison_barracks", "The Quayside Prison Barracks"),
        ("donkey_stables_place", "stable", "The Donkey Stables"),
        ("street_well_place", "well_house", None),
        ("elder_house_1_place", "home_modest", None),
        ("elder_house_2_place", "home_modest", None),
        ("gate_tenement_place", "home_tenement", "The Gate Tenement"),
    ],
    "crescent_market": [
        ("market_square_place", "market_square", None),
        ("stall_grain_place", "market_stall", None),
        ("stall_salt_oil_place", "market_stall", None),
        ("stall_pottery_place", "market_stall", None),
        ("stall_cloth_place", "market_stall", None),
        ("stall_produce_place", "market_stall", None),
        ("stall_dates_place", "market_stall", "The Date Stall"),
        ("stall_spices_place", "market_stall", "The Spice and Herb Stall"),
        ("stall_leather_place", "market_stall", "The Leather Goods Stall"),
        ("stall_bronze_goods_place", "market_stall", "The Bronze Goods Stall"),
        ("cookshop_place", "cookshop", None),
        ("bakery_place", "bakery", None),
        ("weights_house_place", "weights_house", "The House of Weights"),
        ("market_scribe_booth_place", "scribe_booth", "The Scribe's Booth by the Well"),
        ("millers_house_place", "mill", None),
        ("bakers_house_place", "home_modest", None),
        ("market_tenement_place", "home_tenement", "The Market Tenement"),
    ],
    "smiths_lane": [
        ("smithy_place", "foundry", None),
        ("armourer_place", "armourer", "The Armourer's Workshop"),
        ("bowyer_place", "bowyer", "The Bowyer and Fletcher"),
        ("potters_house_place", "potter", None),
        ("weavers_house_place", "dyer_weaver", None),
        ("oil_press_place", "oil_press", "The Oil and Perfume Press"),
        ("jeweller_place", "jeweller", "The Jeweller and Seal-Cutter"),
        ("physicians_house_place", "physician", None),
    ],
    "sacred_mound": [
        ("temple_front_place", "ziggurat", None),
        ("priestess_house_place", "gipar", None),
        ("moon_pool_place", "moon_pool", "The Moon Pool"),
        ("temple_stores_place", "temple_stores", "The Temple Stores"),
        ("temple_kitchens_place", "temple_kitchens", "The Temple Kitchens"),
        ("diviners_house_place", "diviner", "The Diviner's House"),
        ("exorcists_house_place", "exorcist", "The Exorcist's House"),
        ("moon_niche_place", "street_shrine", None),
        ("street_granary_place", "granary", None),
    ],
    "house_of_tablets": [
        ("tablet_school_place", "scribal_school", "The Tablet School"),
        ("tablet_archive_place", "archive", "The Tablet Archive"),
        ("star_terrace_place", "star_terrace", "The Star Terrace"),
        ("scribes_house_place", "home_courtyard", None),
    ],
    "lighthouse_horn": [
        ("great_lighthouse_place", "lighthouse", None),
        ("sea_gate_place", "sea_gate", "The Sea Gate"),
        ("keepers_house_place", "keepers_house", "The Lighthouse Keepers' House"),
        ("enki_shrine_place", "cliff_shrine", "The Shrine of Enki above the Sea"),
    ],
    "lagoon_wharf": [
        ("lighthouse_wharf_place", "wharf", None),
        ("fish_market_place", "fish_market", "The Fish Market"),
        ("customs_house_place", "customs_house", "The Customs House"),
        ("warehouse_1_place", "warehouse", "The Copper Warehouse"),
        ("warehouse_2_place", "warehouse", "The Grain and Oil Warehouse"),
        ("shipwright_place", "boatyard", "The Shipwright's Slip"),
        ("divers_hut_1_place", "divers_hut", "A Sea-Gem Diver's Hut"),
        ("divers_hut_2_place", "divers_hut", "A Sea-Gem Diver's Hut"),
        ("divers_hut_3_place", "divers_hut", "A Sea-Gem Diver's Hut"),
    ],
    "prophets_court": [
        ("council_hall_place", "council_hall", "The Prophet's Council Hall"),
        ("envoys_house_place", "envoys_house", "The Envoys' House"),
        ("rebel_barracks_place", "rebel_barracks", "The Rebel Barracks and Yard"),
        ("imperial_offices_place", "imperial_offices", "The Old Imperial Offices"),
        ("hearing_court_place", "hearing_court", "The Hearing Court"),
    ],
    "reed_quarter": [
        ("mudhif_place", "mudhif", "The Reed Guest Hall"),
        ("whisperers_safehouse_place", "reed_house", "The Whisperers' House"),
        ("floating_shrine_place", "floating_shrine", "The Floating Shrine"),
    ] + [(f"reed_house_{i}_place", "reed_house", "A Reed House on Stilts") for i in range(1, 8)],
    "newcomers_terraces": [
        ("warchief_envoy_tent_place", "warchief_tent", "The Warchief's Envoy Tent"),
        ("migrant_cattle_pen_place", "cattle_pen", "The Migrants' Cattle Pen"),
        ("foreigners_lane_1_place", "home_foreign", "A House in the Foreigners' Lane"),
        ("foreigners_lane_2_place", "home_foreign", "A House in the Foreigners' Lane"),
        ("foreigners_lane_3_place", "home_foreign", "A House in the Foreigners' Lane"),
    ] + [(f"migrant_tent_{i}_place", "migrant_tent", "A Migrant Tent") for i in range(1, 6)],
    "garden_of_tombs": [
        ("garden_of_tombs_place", "necropolis", "The Garden of Tombs"),
        ("tomb_keepers_hut_place", "tomb_keeper", "The Tomb Keeper's Hut"),
        ("tannery_place", "tannery", "The Tannery"),
        ("brickyard_place", "brickyard", "The Brick-Making Yard"),
    ],
    "beyond_the_gate": [
        ("fields_beyond_the_gate_place", "field", None),
        ("grazing_lands_place", "pasture", None),
    ],
}

# Homes that fill a quarter's remaining frontage (typology, how many), by quarter.
FILL = {
    "moon_gate_quarter": ("home_hut", 3),
    "crescent_market": ("home_artisan", 4),
    "smiths_lane": ("home_artisan", 3),
    "sacred_mound": ("home_courtyard", 4),
    "house_of_tablets": ("home_courtyard", 4),
    "lighthouse_horn": ("home_modest", 3),
    "lagoon_wharf": ("home_merchant", 4),
    "prophets_court": ("home_elite", 3),
}

# Where a quarter's first building is pinned (angle deg, radius m), so the
# monuments stand where the design puts them; the rest flow after.
PIN = {
    "moon_gate_place": (185.0, 175.0),        # in the outer wall by the western horn, facing the land road
    "temple_front_place": (90.0, 140.0),      # the northern belly
    "great_lighthouse_place": (-15.0, 215.0), # out on the mole, at the sea
    "sea_gate_place": (8.0, 175.0),           # in the outer wall, facing the quay and the sea
}


def read_csv(path):
    with open(path, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def corners(r):
    x, y, w, d = float(r["x_m"]), float(r["y_m"]), float(r["w_m"]), float(r["d_m"])
    a = math.radians(float(r["yaw_deg"]))
    ux, uy, vx, vy = math.cos(a), math.sin(a), -math.sin(a), math.cos(a)
    return [(x + sx * ux * w / 2 + sy * vx * d / 2, y + sx * uy * w / 2 + sy * vy * d / 2)
            for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]


def overlap(pa, pb):
    """Separating-axis test for two convex quads."""
    for poly in (pa, pb):
        for i in range(4):
            x1, y1 = poly[i]
            x2, y2 = poly[(i + 1) % 4]
            nx, ny = y1 - y2, x2 - x1
            a = [nx * px + ny * py for px, py in pa]
            b = [nx * px + ny * py for px, py in pb]
            if max(a) <= min(b) or max(b) <= min(a):
                return False
    return True


def grown(r, m):
    g = dict(r)
    g["w_m"], g["d_m"] = float(r["w_m"]) + m, float(r["d_m"]) + m
    return g


def band_of(quarter):
    d = {q["id"]: q for q in read_csv(CANON / "city_districts.csv")}.get(quarter)
    if d is None or d["shape"] != "arc":
        return None
    return float(d["r0_m"]), float(d["r1_m"])


def fmt(v):
    return f"{v:.1f}"


def layout():
    types = {t["id"]: t for t in read_csv(CANON / "building_types.csv")}
    quarters = {q["id"]: q for q in read_csv(CANON / "city_districts.csv")}
    placed = []

    def fits(cand, pad=GAP_M):
        cc = corners(grown(cand, pad))
        return all(not overlap(cc, corners(p)) for p in placed if p["quarter"] != "beyond_the_gate")

    def row(pid, typ, quarter, x, y, yaw):
        t = types[typ]
        return {"id": pid, "typology": typ, "quarter": quarter, "wealth": t["wealth"],
                "x_m": fmt(x), "y_m": fmt(y), "yaw_deg": fmt(yaw), "w_m": t["w_m"], "d_m": t["d_m"]}

    def on_arc(pid, typ, quarter, ang, rad):
        a = math.radians(ang)
        return row(pid, typ, quarter, O[0] + rad * math.cos(a), O[1] + rad * math.sin(a), ang - 90.0)

    for qid, items in PROGRAM.items():
        q = quarters[qid]
        fill_typ, fill_n = FILL.get(qid, (None, 0))
        wanted = list(items) + [(f"{qid}_home_{i}_place", fill_typ, None) for i in range(1, fill_n + 1)]
        if q["shape"] == "arc":
            a0, a1, r0, r1 = float(q["a0_deg"]), float(q["a1_deg"]), float(q["r0_m"]), float(q["r1_m"])
            for pid, typ, _ in wanted:
                d = float(types[typ]["d_m"])
                if pid in PIN:
                    ang, rad = PIN[pid]
                    placed.append(on_arc(pid, typ, qid, ang, rad))
                    continue
                # Rows: along the lagoon side, then along the wall side (a wide band has a middle row too).
                radii = [r0 + d / 2 + 1, r1 - d / 2 - 1]
                if r1 - r0 > 60:
                    radii.insert(1, (r0 + r1) / 2)
                done = False
                for rad in radii:
                    ang = a0
                    while ang >= a1 and not done:
                        cand = on_arc(pid, typ, qid, ang, rad)
                        if fits(cand):
                            placed.append(cand)
                            done = True
                        ang -= STEP_DEG
                    if done:
                        break
                if not done:
                    raise SystemExit(f"city_layout: no room in {qid} for {pid} ({typ})")
        else:
            cx, cy = float(q["cx_m"]), float(q["cy_m"])
            rng = random.Random(qid)  # seeded by the quarter: deterministic
            # Clusters keep to their ground: the reed houses stand in the lagoon, the camps and
            # the tombs outside the wall (the Prophet's citadel bulges to 205 m).
            inside_lagoon = qid == "reed_quarter"
            for n, (pid, typ, _) in enumerate(wanted):
                done = False
                for k in range(4000):
                    ring = math.sqrt(k) * 6.0
                    ang = k * 2.39996  # golden angle spiral
                    x, y = cx + ring * math.cos(ang), cy + ring * math.sin(ang)
                    cand = row(pid, typ, qid, x, y, rng.uniform(-25, 25) if typ != "field" else 0.0)
                    far = max(math.hypot(px - O[0], py - O[1]) for px, py in corners(cand))
                    near = min(math.hypot(px - O[0], py - O[1]) for px, py in corners(cand))
                    grounded = far < 100.0 if inside_lagoon else near > 212.0
                    if qid == "beyond_the_gate" or (grounded and fits(cand, 4.0)):
                        placed.append(cand)
                        done = True
                        break
                if not done:
                    raise SystemExit(f"city_layout: no room in {qid} for {pid}")
    return placed


def write(placed, check=False):
    types = {t["id"]: t for t in read_csv(CANON / "building_types.csv")}
    quarters = {q["id"]: q for q in read_csv(CANON / "city_districts.csv")}
    old = {r["id"]: r for r in read_csv(PLACES)}
    names = {pid: nm for items in PROGRAM.values() for pid, _, nm in items}
    buildings = {b["id"] for b in read_csv(CANON / "buildings.csv")}
    head = ["id", "city", "district", "kind", "building_id", "owner_person_id", "name", "description",
            "quarter", "typology", "wealth", "x_m", "y_m", "yaw_deg", "w_m", "d_m", "tag", "source_ref"]
    out = []
    for p in placed:
        t = types[p["typology"]]
        base = old.get(p["id"])
        if base is None:
            base = {
                "id": p["id"], "city": "city_of_the_moon", "kind": t["name"].lower(),
                "building_id": p["typology"] if p["typology"] in buildings else "",
                "owner_person_id": "",
                "name": names.get(p["id"]) or t["name"],
                "description": t["look"][0].upper() + t["look"][1:] + ".",
                "tag": "INVENTED",
                "source_ref": f"INVENTED (D-018/D-025); docs/city-of-the-moon.md; building_types.csv {p['typology']}",
            }
        r = {k: base.get(k, "") for k in head}
        r["district"] = quarters[p["quarter"]]["name"]
        r.update({k: p[k] for k in ("quarter", "typology", "wealth", "x_m", "y_m", "yaw_deg", "w_m", "d_m")})
        out.append(r)
    lines = []
    import io
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=head, lineterminator="\n")
    w.writeheader()
    w.writerows(out)
    text = buf.getvalue()
    if check:
        return PLACES.read_text(encoding="utf-8") == text
    PLACES.write_text(text, encoding="utf-8")
    return True


if __name__ == "__main__":
    placed = layout()
    if "--check" in sys.argv[1:]:
        ok = write(placed, check=True)
        print("places.csv is current" if ok else "places.csv is stale: run tools/city_layout.py")
        sys.exit(0 if ok else 1)
    write(placed)
    print(f"city_layout: {len(placed)} places written to db/canon/places.csv")
