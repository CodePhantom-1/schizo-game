#!/usr/bin/env python3
"""scatter.py — Stage V batch 2: where every plant, stone and piece of clutter stands.

Reads the canon (places.csv, city_districts.csv, flora.csv) and writes one tracked table of
instances, unreal/Content/Sim/scatter.csv, in UE's frame (cm, +X east, +Y south: places.csv x 100):

  mesh,x_cm,y_cm,yaw_deg,scale,withers,seasons      sorted by (mesh, x_cm, y_cm)

For every flora row x habitat: candidate points over the habitat's ground (density per_100m2 over
its area), rejected by one rule set (`blocked`): inside a building footprint grown by 0.5 m, within
2.5 m of a door slot, in the lagoon (only water plants; rooted reeds to 0.3 m of water, lilies float
deeper), in the channels, the wall, the sea, or a gate's passage. Seeded per flora row and habitat:
the same canon gives a byte-identical file.

Usage: python3 tools/art/scatter.py [--check]   (--check: exit 1 when scatter.csv is stale)
"""
import csv
import io
import math
import pathlib
import random
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
import building_grammar as bg  # noqa: E402
import city_layout  # noqa: E402

CANON = ROOT / "db" / "canon"
OUT = ROOT / "unreal" / "Content" / "Sim" / "scatter.csv"
COLUMNS = ["mesh", "x_cm", "y_cm", "yaw_deg", "scale", "withers", "seasons"]
DOOR_CLEAR_M = 2.5
FOOTPRINT_PAD_M = 0.5
FLOATERS = {"water_lily"}          # float on the lagoon; every other water plant is rooted
ROOTED_DEPTH_R = 4.5               # rooted water plants stand where r >= r0 - 4.5 (<= 0.3 m of water)
GARDEN_WEALTH = {"comfortable", "elite", "sacred"}
OPEN_GROUND = {"market_square", "brickyard", "wharf"}
GATES = {"city_gate", "sea_gate"}
TENT_TYPES = {"migrant_tent", "warchief_tent"}


def read(canon, name):
    with open(pathlib.Path(canon) / name, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def frame(canon):
    """(cx, cy), r0 (lagoon edge), r1 (wall), a0, a1 (the horns, degrees) from crescent_frame."""
    r = next(q for q in read(canon, "city_districts.csv") if q["id"] == "crescent_frame")
    return {"o": (float(r["cx_m"]), float(r["cy_m"])), "r0": float(r["r0_m"]), "r1": float(r["r1_m"]),
            "a0": float(r["a0_deg"]), "a1": float(r["a1_deg"])}


# ---- geometry ----
def to_world(p, lx, ly):
    """A point in a place's local frame (x along its width, y along its depth) -> world metres."""
    a = math.radians(float(p["yaw_deg"]))
    c, s = math.cos(a), math.sin(a)
    return float(p["x_m"]) + c * lx - s * ly, float(p["y_m"]) + s * lx + c * ly


def to_local(p, x, y):
    a = math.radians(float(p["yaw_deg"]))
    c, s = math.cos(a), math.sin(a)
    dx, dy = x - float(p["x_m"]), y - float(p["y_m"])
    return c * dx + s * dy, -s * dx + c * dy


def door_point(p):
    """The door slot in world metres: ASimStreetBuilder::KitDoorLocal in the place's frame
    (grid-rounded footprint, the middle cell of the width, 1 m outside the door wall)."""
    w, d = bg.grid(p["w_m"]), bg.grid(p["d_m"])
    ly = d / 2 + 1 if p["door_side"] == "+y" else -d / 2 - 1
    return to_world(p, bg.door_x(w), ly)


def in_rect(p, x, y, pad=0.0):
    lx, ly = to_local(p, x, y)
    return abs(lx) <= float(p["w_m"]) / 2 + pad and abs(ly) <= float(p["d_m"]) / 2 + pad


def solid(p):
    """A place nothing may stand in: every footprint but the fields beyond the gate and open ground."""
    return p["quarter"] != "beyond_the_gate" and p["typology"] not in bg.OPEN


def polar(fr, x, y):
    ox, oy = fr["o"]
    return math.hypot(x - ox, y - oy), math.degrees(math.atan2(y - oy, x - ox))


def in_arc(fr, ang):
    lo, hi = min(fr["a0"], fr["a1"]), max(fr["a0"], fr["a1"])
    return any(lo <= a <= hi for a in (ang, ang + 360, ang - 360))


class World:
    def __init__(self, canon):
        self.fr = frame(canon)
        self.places = read(canon, "places.csv")
        self.solids = [p for p in self.places if solid(p)]
        self.open = [p for p in self.places if p["typology"] in bg.OPEN and p["quarter"] != "beyond_the_gate"]
        self.doors = [door_point(p) for p in self.places if bg.is_building(p)]
        self.gates = [p for p in self.places if p["typology"] in GATES]

    def blocked(self, x, y, flora, habitat):
        fr = self.fr
        ox, oy = fr["o"]
        r, ang = polar(fr, x, y)
        water_plant = flora["group"] == "water"
        if r < fr["r0"] - 1:  # the lagoon
            if not water_plant:
                return True
            if flora["id"] not in FLOATERS and r < fr["r0"] - ROOTED_DEPTH_R:
                return True
        elif flora["id"] in FLOATERS:
            return True  # lilies only on the water
        if y < oy and abs(x - ox) < 12.0 and r >= fr["r0"] - 1:
            return True  # the channel north to the river
        if x > ox and abs(y - (oy - fr["r0"] * 0.78)) < 16.0 and r >= fr["r0"] - 1:
            return True  # the channel east to the sea
        if x > city_layout.COAST_X - 2:
            return True  # the sea
        if fr["r1"] - 0.8 < r < fr["r1"] + 2.5 and in_arc(fr, ang):
            return True  # the wall
        if any(in_rect(p, x, y, FOOTPRINT_PAD_M) for p in self.solids):
            return True
        if habitat != "open" and any(in_rect(p, x, y, FOOTPRINT_PAD_M) for p in self.open):
            return True  # the open ground is for its own clutter only
        if any(math.hypot(x - dx, y - dy) < DOOR_CLEAR_M for dx, dy in self.doors):
            return True
        for g in self.gates:  # the passage through each gate, and 2 m either side
            lx, ly = to_local(g, x, y)
            if abs(lx) < 2.5 + 2.0 and abs(ly) < float(g["d_m"]) / 2 + 8.0:
                return True
        return False


# ---- habitats: (area m2, sampler(rng) -> (x, y)) regions ----
def _annulus(fr, rin, rout):
    lo, hi = math.radians(min(fr["a0"], fr["a1"])), math.radians(max(fr["a0"], fr["a1"]))
    area = (hi - lo) / 2 * (rout * rout - rin * rin)

    def sample(rng):
        a = rng.uniform(lo, hi)
        r = math.sqrt(rng.uniform(rin * rin, rout * rout))
        return fr["o"][0] + r * math.cos(a), fr["o"][1] + r * math.sin(a)
    return [(area, sample)]


def _rect(p, x0, x1, y0, y1):
    """A local-frame rectangle of place p."""
    def sample(rng):
        return to_world(p, rng.uniform(x0, x1), rng.uniform(y0, y1))
    return (abs(x1 - x0) * abs(y1 - y0), sample)


def _ring(p, pad):
    """The band of width pad around a place's footprint (its four sides)."""
    hw, hd = float(p["w_m"]) / 2, float(p["d_m"]) / 2
    return [_rect(p, -hw - pad, hw + pad, hd, hd + pad), _rect(p, -hw - pad, hw + pad, -hd - pad, -hd),
            _rect(p, -hw - pad, -hw, -hd, hd), _rect(p, hw, hw + pad, -hd, hd)]


def _behind(p, near, far):
    """The strip behind a building (the side away from its door), near..far m from the back wall."""
    hw, hd = float(p["w_m"]) / 2, float(p["d_m"]) / 2
    s = -1 if p["door_side"] == "+y" else 1
    return _rect(p, -hw, hw, s * (hd + near), s * (hd + far))


def _front(p, near, far):
    hw, hd = float(p["w_m"]) / 2, float(p["d_m"]) / 2
    s = 1 if p["door_side"] == "+y" else -1
    return _rect(p, -hw, hw, s * (hd + near), s * (hd + far))


def regions(world, habitat):
    fr, places = world.fr, world.places
    buildings = [p for p in places if bg.is_building(p)]
    if habitat == "shore":
        return _annulus(fr, fr["r0"] - 3, fr["r0"] + 2)
    if habitat == "water":
        return _annulus(fr, fr["r0"] - 12, fr["r0"] - 3)
    if habitat == "wall_foot":
        return _annulus(fr, fr["r1"] - 4, fr["r1"] - 1)
    if habitat == "yard":
        return [_behind(p, 0.5, 3.0) for p in buildings]
    if habitat == "garden":
        return [_behind(p, 0.5, 3.0) for p in buildings if p["wealth"] in GARDEN_WEALTH]
    if habitat == "street_edge":
        return [_front(p, 0.6, 1.2) for p in buildings]
    if habitat == "open":
        hw = lambda p: float(p["w_m"]) / 2 - 1  # noqa: E731
        hd = lambda p: float(p["d_m"]) / 2 - 1  # noqa: E731
        return [_rect(p, -hw(p), hw(p), -hd(p), hd(p)) for p in places if p["typology"] in OPEN_GROUND]
    if habitat == "precinct":
        return [r for p in places if p["typology"] == "ziggurat" for r in _ring(p, 6.0)]
    if habitat == "tombs":
        return [r for p in places if p["quarter"] == "garden_of_tombs" for r in _ring(p, 8.0)]
    if habitat == "camp":
        return [r for p in places if p["typology"] in TENT_TYPES for r in _ring(p, 5.0)]
    return []  # roof: nothing grows there yet


def instances(world, flora, habitat):
    """The flora row's instances in one habitat: (x_m, y_m, mesh, yaw, scale)."""
    rng = random.Random(f"{flora['id']}:{habitat}")
    meshes = flora["meshes"].split(";")
    density = float(flora["per_100m2"]) / 100.0 * (2.0 if habitat == "garden" else 1.0)
    out = []
    for area, sample in regions(world, habitat):
        expect = density * area
        n = int(expect) + (1 if rng.random() < expect - int(expect) else 0)
        for _ in range(n):
            x, y = sample(rng)
            mesh, yaw = rng.choice(meshes), rng.uniform(0, 360)
            scale = rng.uniform(float(flora["scale_min"]), float(flora["scale_max"]))
            if not world.blocked(x, y, flora, habitat):
                out.append((x, y, mesh, yaw, scale))
    return out


def build(canon=CANON):
    """Every instance as a scatter.csv row (dicts of strings), sorted."""
    world = World(canon)
    rows = []
    for f in read(canon, "flora.csv"):
        for h in f["habitats"].split(";"):
            for x, y, mesh, yaw, scale in instances(world, f, h):
                rows.append({"mesh": mesh, "x_cm": str(round(x * 100)), "y_cm": str(round(y * 100)),
                             "yaw_deg": f"{yaw:.1f}", "scale": f"{scale:.2f}", "withers": f["withers"],
                             "seasons": f["seasons"]})
    rows.sort(key=lambda r: (r["mesh"], int(r["x_cm"]), int(r["y_cm"])))
    return rows


def habitat_counts(canon=CANON):
    world = World(canon)
    counts = {}
    for f in read(canon, "flora.csv"):
        for h in f["habitats"].split(";"):
            counts[h] = counts.get(h, 0) + len(instances(world, f, h))
    return counts


def render(rows):
    buf = io.StringIO()
    w = csv.DictWriter(buf, COLUMNS, lineterminator="\n")
    w.writeheader()
    w.writerows(rows)
    return buf.getvalue()


def main(argv):
    text = render(build())
    if "--check" in argv:
        old = OUT.read_text(encoding="utf-8").replace("\r\n", "\n") if OUT.exists() else ""
        if old == text:
            print("scatter.csv is current")
            return 0
        a, b = old.splitlines(), text.splitlines()
        i = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
        print(f"scatter.csv is stale at line {i + 1}: have {a[i] if i < len(a) else '<end>'!r}, "
              f"want {b[i] if i < len(b) else '<end>'!r} — run tools/art/scatter.py")
        return 1
    OUT.write_bytes(text.encode("utf-8"))
    print(f"scatter: {text.count(chr(10)) - 1} instances -> {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
