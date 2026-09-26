"""The building grammar (world-art-plan §3-4): a places.csv row + its building_types.csv row
-> a list of parts, the recipe the Blender mesher (building_mesh.py) turns into one GLB.

  python3 tools/art/building_grammar.py [--out art/generated/buildings/specs.json]

Plain Python and deterministic (seeded by the place id). Frame: metres, origin at the footprint
centre, x along the width, y along the depth, z up; the door opens on `door_side` (places.csv,
the same field the street builder reads) at door_x(w), so every doorway matches its door slot.

A part: {"shape": box|prism|dome|vault|hull|gable, "at": [x, y, z] (bottom centre),
"size": [...], "yaw": deg, "mat": "<material>/<wear>" (a trim-atlas cell), "tag", "tint": [r,g,b],
"jitter": m, "batter": m (boxes: the top pulled in on both long faces)}. Sizes: box/gable/vault/
hull [sx, sy, sz] (vault: a C-section arch spanning y, open underneath; gable: ridge along x, its
"open" (+1/-1) side's slope ends at "eave" m high); prism [r_base, r_top, h]; dome [rx, ry, h]. Building-level "soot" sources darken
nearby vertices (ovens, furnaces, kilns).
"""
import csv
import io
import json
import math
import os
import random
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# The street builder's categories (SimStreetBuilder.cpp): these are not rooms.
OPEN = {"market_square", "well_house", "street_shrine", "moon_pool", "necropolis", "brickyard",
        "cattle_pen", "wharf", "fish_market", "floating_shrine", "field", "pasture"}
STALLS = {"market_stall", "cookshop", "scribe_booth"}
MONUMENTS = {"city_gate", "ziggurat", "lighthouse", "sea_gate"}  # SimEnvironment builds these
REED = {"reed_house", "divers_hut", "mudhif"}
TENTS = {"migrant_tent", "warchief_tent"}
COURTYARD = {"home_courtyard", "home_merchant", "home_elite", "home_tenement", "caravan_yard", "gipar",
             "council_hall"}

STOREY_H = 2.8
WALL_T = 0.4
ROOF_T = 0.25
DOOR_H = {"poor": 1.8, "modest": 1.95, "comfortable": 2.05, "elite": 2.2, "civic": 2.2, "sacred": 2.3}
DOOR_W = {"poor": 0.8, "modest": 0.9, "comfortable": 1.0, "elite": 1.2, "civic": 1.3, "sacred": 1.4}
# (fresh, worn, crumbling) odds by wealth; "cracked" is the drought blend M_Building applies.
WEAR_ODDS = {"poor": (0.15, 0.4, 0.45), "modest": (0.35, 0.5, 0.15), "comfortable": (0.5, 0.45, 0.05),
             "elite": (0.75, 0.25, 0.0), "civic": (0.45, 0.45, 0.1), "sacred": (0.8, 0.2, 0.0)}
TRIS = {"box": 12, "gable": 20, "prism": 32, "dome": 64, "vault": 68, "hull": 56}
GREEN = [0.45, 0.62, 0.32]
CLAY = [1.05, 0.95, 0.85]


def grid(v):
    return max(1, int(math.floor(float(v) + 0.5)))


def door_x(w):
    """The door's centre on its wall: SimStreetBuilder::BuildRoom's middle cell of the grid run."""
    n = grid(w)
    return -n / 2 + (n // 2 + 0.5)


def is_building(p):
    return p.get("quarter") != "beyond_the_gate" and p["typology"] not in OPEN | STALLS | MONUMENTS


class Building:
    def __init__(self, place, btype):
        self.place, self.t = place, btype
        self.id = place["id"]
        self.rng = random.Random(self.id)
        self.w, self.d = float(place["w_m"]), float(place["d_m"])
        self.wealth = place.get("wealth") or btype.get("wealth") or "modest"
        self.look = (btype.get("look") or "").lower()
        self.sign = 1 if place.get("door_side") == "+y" else -1
        self.parts, self.soot = [], []
        odds = WEAR_ODDS.get(self.wealth, WEAR_ODDS["modest"])
        r = self.rng.random()
        self.wear = "fresh" if r < odds[0] else ("worn" if r < odds[0] + odds[1] else "crumbling")
        dw = min(DOOR_W.get(self.wealth, 0.9), max(0.4, self.w - 0.6))
        self.door = {"side": "+y" if self.sign > 0 else "-y", "x": door_x(self.w), "width": dw,
                     "height": DOOR_H.get(self.wealth, 1.95)}

    # ---- helpers ----
    def m(self, mat, wear=None):
        return f"{mat}/{wear or self.wear}"

    def add(self, shape, at, size, mat, tag, yaw=0.0, tint=None, jitter=0.03, batter=0.0):
        p = {"shape": shape, "at": [round(v, 3) for v in at], "size": [round(v, 3) for v in size],
             "yaw": round(yaw, 2), "mat": mat, "tag": tag, "tint": tint or [1.0, 1.0, 1.0],
             "jitter": jitter, "batter": batter}
        self.parts.append(p)
        return p

    def front(self, x, out=0.75):
        """A point in the street in front of the door wall (out metres beyond the footprint)."""
        return x, self.sign * (self.d / 2 + out)

    def rand(self, a, b):
        return self.rng.uniform(a, b)

    # ---- walls ----
    def ring(self, x0, x1, y0, y1, z, h, t, mat, tag, door=None, batter=0.0):
        """Four walls on the rectangle [x0,x1]x[y0,y1]; `door` (x, width, height, sign) cuts the
        +y or -y wall and fills above the opening."""
        w, d = x1 - x0, y1 - y0
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        for s in (-1, 1):
            y = cy + s * (d / 2 - t / 2)
            if door and door[3] == s:
                dx, dw, dh = door[0], door[1], door[2]
                for a, b in ((x0, dx - dw / 2), (dx + dw / 2, x1)):
                    if b - a > 0.05:
                        self.add("box", [(a + b) / 2, y, z], [b - a, t, h], mat, tag, batter=batter)
                if h > dh:
                    self.add("box", [dx, y, z + dh], [dw, t, h - dh], mat, tag, jitter=0.02)
            else:
                self.add("box", [cx, y, z], [w, t, h], mat, tag, batter=batter)
        for s in (-1, 1):
            if d - 2 * t > 0.05:
                self.add("box", [cx + s * (w / 2 - t / 2), cy, z], [t, d - 2 * t, h], mat, tag, batter=batter)

    def windows(self, x0, x1, y0, y1, z, h, sill, skip_side=None):
        """High slit windows (dark insets) on the long walls, and one on each end wall."""
        tint = [0.22, 0.19, 0.17]
        mat = "bitumen/fresh"
        for s in (-1, 1):
            if skip_side == s:
                continue
            n = max(1, int((x1 - x0) / 3.2))
            for i in range(n):
                x = x0 + (x1 - x0) * (i + 0.5) / n
                if abs(x - self.door["x"]) < self.door["width"] and s == self.sign:
                    continue
                self.add("box", [x, (y0 + y1) / 2 + s * ((y1 - y0) / 2 + 0.01), z + sill], [0.34, 0.06, 0.3],
                         mat, "window", tint=tint, jitter=0.0)
        for s in (-1, 1):
            if y1 - y0 > 2.5:
                self.add("box", [(x0 + x1) / 2 + s * ((x1 - x0) / 2 + 0.01), (y0 + y1) / 2, z + sill],
                         [0.06, 0.34, 0.3], mat, "window", tint=tint, jitter=0.0)

    def beams(self, x0, x1, y, z, out_sign):
        """Palm-log roof beams poking out of the wall under the roof line [A]."""
        step = self.rand(0.7, 0.95)
        x = x0 + step / 2
        while x < x1 - 0.2:
            if not (out_sign == self.sign and abs(x - self.door["x"]) < self.door["width"] / 2 + 0.1):
                self.add("box", [x, y + out_sign * 0.15, z], [0.14, 0.3 + WALL_T, 0.14],
                         self.m("palm", "worn"), "beam", jitter=0.02)
            x += step

    def roof(self, x0, x1, y0, y1, z, parapet):
        w, d = x1 - x0, y1 - y0
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        self.add("box", [cx, cy, z], [w + 0.1, d + 0.1, ROOF_T], self.m("packed_earth"), "roof", jitter=0.02)
        zt = z + ROOF_T
        if parapet == "none":
            return zt
        ph = 0.45 if parapet == "crenel" else self.rand(0.3, 0.5)
        mat = self.parts[-2]["mat"] if len(self.parts) > 1 and self.parts[-2]["tag"] == "wall" else self.m("mudbrick")
        self.ring(x0 - 0.05, x1 + 0.05, y0 - 0.05, y1 + 0.05, zt, ph, 0.2, mat, "parapet")
        if parapet == "crenel":
            # Stepped merlons along the front and back [A: Mesopotamian stepped crenellation].
            for s in (-1, 1):
                x = x0 + 0.25
                while x < x1 - 0.2:
                    self.add("box", [x, cy + s * (d / 2 - 0.05), zt + ph], [0.4, 0.2, 0.3], mat, "crenel", jitter=0.01)
                    self.add("box", [x, cy + s * (d / 2 - 0.05), zt + ph + 0.3], [0.2, 0.2, 0.2], mat, "crenel", jitter=0.01)
                    x += 0.8
        return zt

    # ---- the whole house ----
    def finish(self):
        """Wall material and ornament from look + wealth (world-art-plan §2-3)."""
        r = self.rng.random()
        w = self.wealth
        if "whitewash" in self.look or w == "sacred":
            wall = "whitewash"
        elif w == "poor":
            wall = "mudbrick" if r < 0.6 else "plaster"
        elif w == "modest":
            wall = "plaster" if r < 0.7 else "mudbrick"
        elif w == "comfortable":
            wall = "plaster"
        elif w == "elite":
            wall = "whitewash" if r < 0.7 else "fired_brick"
        else:  # civic
            wall = "mudbrick" if r < 0.5 else "plaster"
        band = None
        if "cone mosaic" in self.look or (w == "elite" and self.rng.random() < 0.6):
            band = "cone_mosaic"
        elif "glazed" in self.look or "lapis" in self.look:
            band = "glazed_lapis"
        elif w in ("comfortable", "modest") and self.rng.random() < 0.45:
            band = "ochre_band"
        plinth = {"poor": None, "modest": "packed_earth", "comfortable": "fired_brick", "elite": "stone",
                  "civic": "fired_brick", "sacred": "stone"}.get(w)
        if "fired-brick plinth" in self.look:
            plinth = "fired_brick"
        buttress = w == "sacred" or (w == "civic" and self.rng.random() < 0.5) or "recessed" in self.look
        parapet = "crenel" if w in ("sacred", "elite") or (w == "civic" and buttress) else (
            "none" if w == "poor" and self.rng.random() < 0.5 else "plain")
        return wall, band, plinth, buttress, parapet

    def house(self):
        hw, hd = self.w / 2, self.d / 2
        wall, band, plinth, buttress, parapet = self.finish()
        wm = self.m(wall)
        d = self.door
        door = (d["x"], d["width"], d["height"], self.sign)
        storeys = max(1, int(self.t.get("storeys") or 1))
        h = STOREY_H
        ph = 0.0
        if plinth:
            ph = {"packed_earth": 0.3, "fired_brick": 0.45, "stone": 0.5}[plinth]
            self.ring(-hw, hw, -hd, hd, 0.0, ph, WALL_T + 0.12, self.m(plinth, "worn" if plinth != "stone" else "fresh"),
                      "plinth", door=(d["x"], d["width"] + 0.12, 1.0, self.sign))
            if plinth == "stone":
                self.ring(-hw, hw, -hd, hd, ph, 0.06, WALL_T + 0.08, "bitumen/fresh", "plinth",
                          door=(d["x"], d["width"] + 0.1, 1.0, self.sign))
        court = self.t["id"] in COURTYARD and self.w >= 8 and self.d >= 7
        batter = 0.04 if wall in ("mudbrick", "plaster") else 0.0
        self.ring(-hw + 0.06, hw - 0.06, -hd + 0.06, hd - 0.06, 0.0, h, WALL_T, wm, "wall", door=door, batter=batter)
        self.windows(-hw, hw, -hd, hd, 0.0, h, 1.9)
        # The lintel: palm log for most, cedar for the rich [A].
        lin = self.m("cedar", "fresh") if self.wealth in ("elite", "sacred", "comfortable") else self.m("palm")
        self.add("box", [d["x"], self.sign * (hd - 0.26), d["height"]], [d["width"] + 0.4, WALL_T + 0.16, 0.2], lin, "lintel", jitter=0.01)
        if self.wealth in ("elite", "sacred", "civic"):
            # A recessed double door frame (rich), in fired brick.
            for s in (-1, 1):
                self.add("box", [d["x"] + s * (d["width"] / 2 + 0.18), self.sign * (hd + 0.02), 0.0],
                         [0.2, 0.2, d["height"] + 0.3], self.m("fired_brick"), "jamb", jitter=0.01)
        if band:
            z0 = d["height"] + 0.25
            bh = min(0.5, h - z0 - 0.05)
            if bh > 0.15:
                self.add("box", [0.0, self.sign * (hd + 0.0), z0], [self.w - 0.1, 0.14, bh], self.m(band, "fresh" if band != "ochre_band" else self.wear), "band", jitter=0.0)
        if buttress:
            step = self.rand(1.1, 1.4)
            x = -hw + 0.4
            while x < hw - 0.3:
                if abs(x - d["x"]) > d["width"] / 2 + 0.35:
                    for s in (-1, 1):
                        self.add("box", [x, s * (hd + 0.05), 0.0], [0.34, 0.2, h + 0.1], wm, "buttress", jitter=0.01)
                x += step
        if court:
            self.courtyard(wm, h, parapet)
            top = h
        else:
            self.beams(-hw, hw, self.sign * hd, h - 0.35, self.sign)
            self.beams(-hw, hw, -self.sign * hd, h - 0.35, -self.sign)
            top = self.roof(-hw, hw, -hd, hd, h, parapet)
            self.rooftop(-hw, hw, -hd, hd, top, parapet)
        if storeys >= 2:
            self.upper(wm, h, parapet, court)
        return h

    def courtyard(self, wm, h, parapet):
        """Rooms round an open court (the Ur courtyard house, [A]): a roofed band at the back and one
        side, the court open to the sky with its palm, well, hearth or oven."""
        hw, hd = self.w / 2, self.d / 2
        back = -self.sign
        rd = min(4.0, self.d * 0.42)
        sw = min(3.2, self.w * 0.32)
        side = 1 if self.rng.random() < 0.5 else -1
        yb0, yb1 = sorted((back * hd, back * (hd - rd)))
        xs0, xs1 = sorted((side * hw, side * (hw - sw)))
        # Inner walls where the rooms meet the court (a doorway into each).
        iy = back * (hd - rd)
        self.add("box", [(-side * 0.8) + 0.0, iy, 0.0], [self.w - sw - 2.2, 0.3, h], wm, "inner", jitter=0.02)
        self.add("box", [side * (hw - sw), (yb0 + yb1) / 2 * 0 + (-back) * (rd / 2 - 0.1) * 0, 0.0],
                 [0.3, max(0.4, self.d - rd - 2.4), h], wm, "inner", jitter=0.02)
        self.roof(-hw, hw, yb0, yb1, h, "none")
        self.roof(xs0, xs1, -hd, hd, h, "none")
        # Parapet round the whole outside, beams on the outer faces.
        zt = h + ROOF_T
        if parapet != "none":
            self.ring(-hw - 0.05, hw + 0.05, -hd - 0.05, hd + 0.05, zt, 0.4, 0.2, wm, "parapet")
        self.beams(-hw, hw, back * hd, h - 0.35, back)
        # The court floor and its life.
        cx = -side * (sw / 2)
        cy = self.sign * (rd / 2) * 0.9
        self.add("box", [cx, cy, 0.0], [self.w - sw - 0.9, self.d - rd - 0.9, 0.06], self.m("packed_earth"), "floor", jitter=0.0)
        kind = self.t["id"]
        if kind in ("home_courtyard", "home_elite", "gipar", "council_hall"):
            self.palm(cx + self.rand(-0.8, 0.8), cy + self.rand(-0.5, 0.5), self.rand(4.5, 6.5))
        if kind in ("home_courtyard", "home_merchant", "home_elite"):
            self.add("prism", [cx - 1.2, cy - 0.6, 0.0], [0.55, 0.5, 0.75], self.m("fired_brick", "worn"), "well", jitter=0.01)
        if kind in ("home_tenement", "caravan_yard"):
            self.oven(cx + 1.0, cy, 0.0)
        if kind == "caravan_yard":
            for i in range(3):
                self.add("box", [cx - 2 + i * 1.6, cy + 1.2, 0.0], [1.2, 0.5, 0.5], self.m("stone", "worn"), "marker")  # troughs
        # Tenements: many doors round one court (dark doorways on the inner wall).
        if kind == "home_tenement":
            for i in range(4):
                x = -hw + sw + 1.0 + i * (self.w - sw - 2) / 4
                self.add("box", [x * (1 if side < 0 else -1) * 0 + x, iy - back * 0.16, 0.0], [0.8, 0.06, 1.8],
                         "bitumen/fresh", "window", tint=[0.2, 0.18, 0.16], jitter=0.0)

    def upper(self, wm, h, parapet, court):
        """A second storey over the back rooms, a roof terrace in front (and a cedar gallery for merchants)."""
        hw, hd = self.w / 2, self.d / 2
        back = -self.sign
        d2 = min(self.d * self.rand(0.45, 0.6), 5.0 if court else self.d)
        w2 = self.w * self.rand(0.6, 0.95)
        x2 = self.rand(-(hw - w2 / 2), hw - w2 / 2)
        y1 = back * hd
        y0 = back * (hd - d2)
        ya, yb = sorted((y0, y1))
        z = h + ROOF_T
        h2 = STOREY_H * 0.92
        self.ring(x2 - w2 / 2 + 0.05, x2 + w2 / 2 - 0.05, ya + 0.05, yb - 0.05, z, h2, 0.35, wm, "wall_upper")
        self.windows(x2 - w2 / 2, x2 + w2 / 2, ya, yb, z, h2, 1.6)
        self.beams(x2 - w2 / 2, x2 + w2 / 2, back * hd, z + h2 - 0.35, back)
        top = self.roof(x2 - w2 / 2, x2 + w2 / 2, ya, yb, z + h2, parapet)
        # A doorway from the terrace into the upper rooms.
        self.add("box", [x2, y0 - back * 0.02 * 0 + (-back) * 0.18 * 0 + y0, z], [0.8, 0.06, 1.8], "bitumen/fresh", "window",
                 tint=[0.2, 0.18, 0.16], jitter=0.0)
        if self.t["id"] in ("home_merchant", "home_elite", "council_hall", "gipar", "caravan_yard"):
            # The cedar gallery along the upper storey's court/terrace face, on posts [A].
            gy = y0 + (-back) * 0.45
            self.add("box", [x2, gy, z + 0.02], [w2 - 0.2, 0.9, 0.12], self.m("cedar", "fresh"), "gallery", jitter=0.01)
            self.add("box", [x2, gy + (-back) * 0.4, z + 0.14], [w2 - 0.2, 0.08, 0.9], self.m("cedar", "fresh"), "gallery", jitter=0.01)
            for i in range(max(2, int(w2 / 1.6))):
                px = x2 - w2 / 2 + 0.3 + i * (w2 - 0.6) / max(1, int(w2 / 1.6) - 1)
                self.add("prism", [px, gy + (-back) * 0.4, 0.0], [0.09, 0.08, z], self.m("cedar", "fresh"), "gallery", jitter=0.0)
        if self.rng.random() < 0.4:
            self.shelter(x2, (ya + yb) / 2, top, min(w2, 3.0) * 0.8, min(d2, 3.0) * 0.8)

    def rooftop(self, x0, x1, y0, y1, z, parapet):
        """Rooftop life: reed sun-shelters, drying racks, the ladder hatch (people sleep up here in summer)."""
        r = self.rng.random()
        w, d = x1 - x0, y1 - y0
        if w < 3 or d < 3:
            return
        cx, cy = (x0 + x1) / 2 + self.rand(-w / 5, w / 5), (y0 + y1) / 2 + self.rand(-d / 5, d / 5)
        if r < 0.4:
            self.shelter(cx, cy, z, min(3.0, w * 0.5), min(2.6, d * 0.5))
        elif r < 0.62:
            self.rack(cx, cy, z)
        if self.rng.random() < 0.5:
            self.add("box", [x0 + 0.9, y0 + 0.9, z], [0.7, 0.7, 0.04], "bitumen/fresh", "hatch", tint=[0.2, 0.18, 0.16], jitter=0.0)
            self.add("box", [x0 + 0.9, y0 + 0.9, z], [0.5, 0.08, 1.4], self.m("palm", "worn"), "ladder", yaw=15, jitter=0.0)

    def shelter(self, cx, cy, z, sx, sy):
        for ix in (-1, 1):
            for iy in (-1, 1):
                self.add("prism", [cx + ix * sx / 2, cy + iy * sy / 2, z], [0.06, 0.05, 1.8], self.m("palm", "worn"), "shelter", jitter=0.01)
        self.add("box", [cx, cy, z + 1.8], [sx + 0.3, sy + 0.3, 0.1], self.m("reed"), "shelter", jitter=0.04)

    def rack(self, cx, cy, z, mat=None):
        for s in (-1, 1):
            self.add("prism", [cx + s * 0.9, cy, z], [0.05, 0.04, 1.6], self.m("palm", "worn"), "rack", jitter=0.01)
        self.add("box", [cx, cy, z + 1.5], [2.0, 0.06, 0.06], self.m("palm", "worn"), "rack", jitter=0.0)
        self.add("box", [cx, cy, z + 0.55], [1.7, 0.04, 0.95], mat or self.m(self.rng.choice(["textile_madder", "textile_indigo", "reed"]), "worn"),
                 "rack", jitter=0.03)

    # ---- trade markers (world-art-plan §4.2: every trade reads without a sign) ----
    def oven(self, x, y, z, big=False):
        r = 0.9 if big else 0.55
        self.add("dome", [x, y, z], [r, r, r * 1.4], self.m("mudbrick", "worn"), "marker", jitter=0.03)
        self.soot.append([x, y, z + r * 1.4, 1.6])

    def jars(self, x, y, n):
        for i in range(n):
            a = i * 2.4
            self.add("prism", [x + 0.45 * math.cos(a) * (i > 0), y + 0.45 * math.sin(a) * (i > 0), 0.0],
                     [self.rand(0.22, 0.3), self.rand(0.12, 0.18), self.rand(0.55, 0.95)], self.m("plaster", "worn"), "marker",
                     tint=CLAY, jitter=0.01)

    def bales(self, x, y, n):
        for i in range(n):
            self.add("box", [x + (i % 2) * 0.8 - 0.4, y, 0.6 * (i // 2)], [0.75, 0.6, 0.6],
                     self.m(self.rng.choice(["reed", "textile_madder", "goat_hair"]), "worn"), "marker", yaw=self.rand(-10, 10))

    def awning(self, mat=None):
        d = self.door
        x, y = self.front(d["x"], 1.1)
        cloth = mat or self.m(self.rng.choice(["textile_madder", "textile_indigo", "reed"]), "fresh")
        for s in (-1, 1):
            self.add("prism", [x + s * 1.2, y + self.sign * 0.35, 0.0], [0.06, 0.05, 2.3], self.m("palm", "worn"), "marker", jitter=0.01)
        self.add("box", [x, self.sign * (self.d / 2 + 0.75), 2.3], [2.8, 1.6, 0.06], cloth, "marker", jitter=0.03)

    def pole_rack(self, x, y, n, head):
        """Spears / bows / shafts leaning on a rail."""
        self.add("box", [x, y, 0.9], [1.8, 0.08, 0.08], self.m("palm", "worn"), "marker", jitter=0.0)
        for i in range(n):
            px = x - 0.8 + i * 1.6 / max(1, n - 1)
            self.add("prism", [px, y + 0.1, 0.0], [0.03, 0.02, 2.1], self.m("palm", "fresh"), "marker", jitter=0.0)
            if head:
                self.add("prism", [px, y + 0.1, 2.1], [0.06, 0.0, 0.25], "bitumen/fresh", "marker", tint=[1.6, 1.1, 0.7], jitter=0.0)

    def frame(self, x, y, cloth, h=1.8):
        """A hide stretcher, a loom, or a skein line."""
        for s in (-1, 1):
            self.add("prism", [x + s * 0.7, y, 0.0], [0.05, 0.04, h], self.m("palm", "worn"), "marker", jitter=0.01)
        self.add("box", [x, y, h - 0.08], [1.6, 0.07, 0.07], self.m("palm", "worn"), "marker", jitter=0.0)
        self.add("box", [x, y, 0.35], [1.25, 0.04, h - 0.55], cloth, "marker", jitter=0.02)

    def palm(self, x, y, h):
        self.add("prism", [x, y, 0.0], [0.2, 0.13, h], self.m("palm", "fresh"), "tree", jitter=0.05)
        self.add("dome", [x, y, h - 0.4], [1.8, 1.8, 0.9], self.m("reed", "fresh"), "tree", tint=GREEN, jitter=0.12)

    def markers(self):
        t, d = self.t["id"], self.door
        fx = lambda off: self.front(d["x"] + off)  # noqa: E731
        other = -1 if d["x"] > 0 else 1  # the side of the door with more wall to use
        side_x = d["x"] + other * min(1.8, self.w / 2 - 0.5)
        sx, sy = self.front(side_x, 0.6)
        if t in ("bakery", "temple_kitchens"):
            self.oven(sx, sy, 0.0)
            self.oven(sx + other * 1.2, sy, 0.0)
            self.bales(*fx(other * 2.6), 1)
        elif t == "cookshop":
            self.oven(sx, sy, 0.0)
        elif t == "beerhouse":
            self.jars(sx, sy, 4)
            self.add("box", [d["x"] - other * 1.4, sy, 0.0], [1.4, 0.4, 0.45], self.m("palm", "worn"), "marker")  # bench
            self.awning()
        elif t == "mill":
            for i in range(3):
                self.add("box", [sx + other * i * 0.7, sy, 0.0], [0.55, 0.35, 0.25], self.m("stone", "worn"), "marker")  # querns
            self.bales(sx, sy + self.sign * 0.2, 1)
        elif t in ("foundry", "armourer"):
            hw, hd = self.w / 2, self.d / 2
            self.add("prism", [hw * 0.5 * other, -self.sign * hd * 0.4, 0.0], [0.45, 0.32, STOREY_H + 1.6],
                     self.m("fired_brick", "crumbling"), "marker", jitter=0.02)  # the furnace chimney
            self.soot.append([hw * 0.5 * other, -self.sign * hd * 0.4, STOREY_H + 1.6, 2.5])
            self.soot.append([d["x"], self.sign * hd, d["height"], 1.8])
            self.add("dome", [sx, sy, 0.0], [0.8, 0.8, 0.5], "bitumen/worn", "marker", tint=[0.8, 0.7, 0.62], jitter=0.08)  # slag heap
            if t == "armourer":
                self.frame(sx - other * 1.6, sy + self.sign * 0.1, self.m("goat_hair", "worn"), 1.5)  # hide shields drying
        elif t == "bowyer":
            self.pole_rack(sx, sy, 6, False)
        elif t in ("watch_post", "rebel_barracks", "prison_barracks"):
            self.pole_rack(sx, sy, 5, True)
            if t == "rebel_barracks":
                self.banner(*fx(-other * 2.0))
            if t == "watch_post":
                self.add("box", [0.0, 0.0, STOREY_H + ROOF_T], [2.4, 2.4, 1.6], self.m("mudbrick"), "marker")  # lookout
        elif t == "potter":
            self.oven(sx, sy, 0.0, big=True)
            self.jars(*fx(-other * 1.5), 5)
            self.add("prism", [d["x"] - other * 2.6, sy, 0.0], [0.35, 0.35, 0.55], self.m("palm", "worn"), "marker")  # kick wheel
        elif t == "dyer_weaver":
            self.frame(sx, sy, self.m("textile_indigo", "fresh"))
            for i in range(2):
                self.add("prism", [d["x"] - other * (1.4 + i), sy, 0.0], [0.4, 0.42, 0.6], self.m("bitumen", "worn"),
                         "marker", tint=[0.6, 0.7, 1.4] if i else [1.5, 0.6, 0.5])  # dye vats
        elif t == "tannery":
            self.frame(sx, sy, self.m("goat_hair", "worn"))
            self.frame(d["x"] - other * (d["width"] / 2 + 1.0), sy, self.m("textile_madder", "crumbling"))
            self.add("box", [d["x"] - other * 0.4, sy, 0.0], [1.6, 1.0, 0.05], "bitumen/fresh", "marker", tint=[0.5, 0.4, 0.3])  # soaking pit
        elif t == "oil_press":
            self.add("box", [sx, sy - self.sign * 0.3, 1.0], [2.6, 0.22, 0.22], self.m("cedar", "worn"), "marker")  # the beam press
            self.add("prism", [sx, sy, 0.0], [0.2, 0.2, 1.0], self.m("stone", "worn"), "marker")
            self.jars(*fx(-other * 1.6), 3)
        elif t == "jeweller":
            self.awning(self.m("textile_indigo", "fresh"))
            self.add("box", [d["x"] - other * 1.4, sy, 0.0], [1.2, 0.5, 0.85], self.m("cedar", "fresh"), "marker")  # the counter
        elif t == "physician":
            self.rack(sx, sy, 0.0, self.m("reed", "fresh"))  # herbs drying
            self.jars(*fx(-other * 1.5), 3)
        elif t in ("temple_stores", "granary"):
            hw, hd = self.w / 2, self.d / 2
            for i in range(2 if t == "temple_stores" else 3):
                x = -hw + (i + 0.7) * self.w / 3.2
                self.add("dome", [x, -self.sign * hd * 0.3, STOREY_H + ROOF_T], [1.3, 1.3, 1.9], self.m("plaster"), "marker", jitter=0.03)
            self.bales(sx, sy, 2)
        elif t in ("diviner", "exorcist", "cliff_shrine", "tomb_keeper"):
            # The shrine niche by the door with its lamps; the diviner's liver-model table, the exorcist's
            # amulet rack, the keeper's lamp.
            self.add("box", [d["x"] + other * (d["width"] / 2 + 0.6), self.sign * (self.d / 2 + 0.05), 1.0], [0.5, 0.12, 0.7],
                     "bitumen/fresh", "marker", tint=[0.25, 0.2, 0.18], jitter=0.0)
            self.add("prism", [d["x"] + other * (d["width"] / 2 + 0.6), self.sign * (self.d / 2 + 0.12), 1.0], [0.08, 0.05, 0.22],
                     self.m("plaster", "fresh"), "marker", tint=[1.3, 1.1, 0.8], jitter=0.0)
            if t == "diviner":
                self.add("box", [sx, sy, 0.0], [0.8, 0.6, 0.8], self.m("cedar", "worn"), "marker")
                self.add("dome", [sx, sy, 0.8], [0.18, 0.18, 0.1], self.m("plaster", "fresh"), "marker", tint=CLAY)
            if t == "exorcist":
                self.frame(sx, sy, self.m("reed", "worn"), 1.4)
            if t in ("cliff_shrine",):
                self.add("box", [sx, sy, 0.0], [0.9, 0.7, 0.9], self.m("stone", "worn"), "marker")  # offering table
        elif t in ("scribal_school", "archive"):
            self.add("box", [sx, sy, 0.0], [1.4, 0.8, 0.6], self.m("plaster", "worn"), "marker", tint=CLAY)  # the clay bin
            for i in range(5):
                self.add("box", [sx - 0.5 + i * 0.25, sy, 0.62], [0.12, 0.18, 0.04], self.m("plaster", "fresh"), "marker", tint=CLAY, jitter=0.0)
        elif t == "star_terrace":
            hw, hd = self.w / 2, self.d / 2
            z = STOREY_H + ROOF_T
            for k in range(3):
                s = 1 - 0.25 * (k + 1)
                self.add("box", [0.0, 0.0, z], [self.w * s, self.d * s, 0.9], self.m("mudbrick", "fresh"), "marker")
                z += 0.9
            self.add("prism", [0.0, 0.0, z], [0.08, 0.02, 1.8], self.m("stone", "fresh"), "marker")  # the gnomon
        elif t == "weights_house":
            self.add("prism", [sx, sy, 0.0], [0.07, 0.06, 1.9], self.m("cedar", "fresh"), "marker")
            self.add("box", [sx, sy, 1.85], [1.4, 0.07, 0.07], self.m("cedar", "fresh"), "marker")
            for s in (-1, 1):
                self.add("prism", [sx + s * 0.62, sy, 1.2], [0.2, 0.2, 0.05], self.m("fired_brick", "fresh"), "marker", tint=[1.2, 0.95, 0.6])
            self.add("box", [d["x"] - other * 1.4, sy, 0.0], [0.4, 0.3, 0.25], self.m("stone", "fresh"), "marker")  # a duck weight
        elif t in ("toll_house", "customs_house"):
            self.awning()
            self.add("box", [d["x"] - other * 1.5, sy, 0.0], [1.3, 0.7, 0.8], self.m("cedar", "worn"), "marker")  # the table
            self.bales(sx, sy, 3)
        elif t == "warehouse":
            self.jars(sx, sy, 5)
            self.bales(*fx(-other * 1.8), 4)
        elif t == "stable":
            hw = self.w / 2
            x0 = side_x - other * 0.2
            for i in range(6):
                self.add("prism", [x0 + other * i * 0.55, sy + self.sign * 0.6, 0.0], [0.05, 0.04, 1.2], self.m("reed", "worn"), "marker", jitter=0.02)
            self.add("box", [x0 + other * 1.4, sy + self.sign * 0.6, 0.9], [3.0, 0.06, 0.08], self.m("palm", "worn"), "marker")
            self.bales(*fx(-other * 1.5), 2)
        elif t == "boatyard":
            hx = d["x"] + other * (d["width"] / 2 + 2.3)  # the boat on its trestle, clear of the door
            self.add("hull", [hx, sy, 0.3], [3.6, 0.9, 0.5], self.m("reed", "worn"), "marker", yaw=self.rand(-5, 5))
            self.add("box", [hx, sy, 0.0], [3.2, 0.2, 0.3], self.m("palm", "worn"), "marker")
            self.add("prism", [d["x"] - other * 1.2, sy, 0.0], [0.28, 0.3, 0.5], "bitumen/fresh", "marker")  # bitumen pots
        elif t == "keepers_house":
            self.bales(sx, sy, 3)
            self.add("prism", [d["x"] - other * 1.2, sy, 0.0], [0.28, 0.3, 0.5], "bitumen/fresh", "marker")
        elif t in ("council_hall", "envoys_house", "hearing_court", "imperial_offices"):
            if t == "council_hall":
                # A portico of painted palm columns [A].
                for i in range(4):
                    px = d["x"] + (i - 1.5) * 1.7
                    self.add("prism", [px, self.sign * (self.d / 2 + 1.0), 0.0], [0.24, 0.2, 3.2], self.m("palm", "fresh"), "marker")
                    self.add("prism", [px, self.sign * (self.d / 2 + 1.0), 2.4], [0.26, 0.26, 0.3], self.m("ochre_band", "fresh"), "marker")
                self.add("box", [d["x"], self.sign * (self.d / 2 + 1.0), 3.2], [7.0, 1.2, 0.2], self.m("cedar", "fresh"), "marker")
            elif t == "envoys_house":
                self.banner(sx, sy)
                self.banner(*fx(-other * 2.2))
            elif t == "hearing_court":
                self.add("box", [sx, sy, 0.0], [2.0, 0.7, 0.9], self.m("cedar", "worn"), "marker")  # the judges' bench
                for i in range(5):
                    self.add("box", [d["x"] + other * (d["width"] / 2 + 1.0 + i * 0.12), self.sign * (self.d / 2 + 0.05), 1.7], [0.05, 0.05, 0.5],
                             self.m("cedar", "worn"), "marker", jitter=0.0)  # the barred window
            else:  # imperial offices, half looted: rubble and a fallen beam
                self.add("dome", [sx, sy, 0.0], [1.1, 0.9, 0.5], self.m("mudbrick", "crumbling"), "marker", jitter=0.15)
                self.add("box", [sx - other * 1.5, sy, 0.1], [2.4, 0.18, 0.18], self.m("cedar", "crumbling"), "marker", yaw=25)
        elif t in ("home_foreign",):
            self.add("box", [d["x"] + other * 1.5, self.sign * (self.d / 2 + 0.06), 0.9], [1.3, 0.05, 1.5],
                     self.m(self.rng.choice(["textile_madder", "textile_indigo"]), "fresh"), "marker", jitter=0.02)  # carpets hung out
        elif t == "home_artisan":
            self.awning()
            self.add("box", [d["x"] - other * 1.4, sy, 0.0], [1.2, 0.5, 0.85], self.m("palm", "worn"), "marker")  # shop counter
        elif t == "home_hut":
            self.add("box", [sx, sy, 0.0], [0.55, 0.35, 0.22], self.m("stone", "worn"), "marker")  # the quern outside
        elif t in ("home_modest", "home_courtyard", "home_merchant", "home_elite", "home_tenement", "caravan_yard", "gipar"):
            if self.rng.random() < 0.5:
                self.jars(sx, sy, self.rng.randint(1, 3))
        if not any(p["tag"] == "marker" for p in self.parts) and self.t.get("group") != "home":
            # Every trade reads without a sign: a fallback of goods by the door.
            self.jars(sx, sy, 2)
            self.bales(*fx(-other * 1.5), 1)

    def banner(self, x, y):
        self.add("prism", [x, y, 0.0], [0.05, 0.04, 3.6], self.m("palm", "fresh"), "marker")
        self.add("box", [x + 0.4, y, 2.2], [0.7, 0.04, 1.3], self.m(self.rng.choice(["textile_madder", "textile_indigo"]), "fresh"), "marker")

    # ---- reed halls and tents ----
    def reed(self):
        """The arched reed house and the mudhif guest hall: bundled-reed arches, a barrel vault [A]."""
        hw, hd = self.w / 2, self.d / 2
        t = self.t["id"]
        z = 0.0
        if t == "reed_house":
            # A reed platform on stilts at the lagoon's edge.
            for ix in (-1, 1):
                for iy in (-1, 1):
                    self.add("prism", [ix * (hw - 0.3), iy * (hd - 0.3), 0.0], [0.1, 0.1, 0.3], self.m("palm", "worn"), "stilt", jitter=0.01)
            # Low enough to step onto (UE's MaxStepHeight is 0.45 m): the door slot is on the ground.
            self.add("box", [0.0, 0.0, 0.28], [self.w, self.d, 0.12], self.m("reed", "worn"), "platform", jitter=0.0)
            z = 0.4
        h = min(self.d * (0.95 if t == "mudhif" else 0.8), 5.5)
        # The barrel vault spans the depth; the doorway is a gap in it, door-wide, at door_x, so the
        # door slot opens into the hall (the mesher's vault is a C-section shell, open underneath).
        d = self.door
        g0, g1 = d["x"] - d["width"] / 2, d["x"] + d["width"] / 2
        for a, b in ((-hw + 0.2, g0), (g1, hw - 0.2)):
            if b - a > 0.1:
                self.add("vault", [(a + b) / 2, 0.0, z], [b - a, self.d - 0.4, h], self.m("reed"), "vault", jitter=0.05)
        step = 1.0 if t == "mudhif" else 1.3
        x = -hw + 0.5
        while x <= hw - 0.4:
            if not g0 - 0.15 < x < g1 + 0.15:
                self.add("vault", [x, 0.0, z], [0.22, self.d - 0.26, h + 0.08], self.m("reed", "worn"), "rib", tint=[0.8, 0.72, 0.6], jitter=0.02)
            x += step
        if t == "mudhif":
            # The two great bundled columns flanking the door.
            for s in (-1, 1):
                self.add("prism", [d["x"] + s * (d["width"] / 2 + 0.55), self.sign * (hd - 0.3), 0.0], [0.4, 0.28, h * 0.6 + 0.9],
                         self.m("reed", "worn"), "marker", jitter=0.03)
        if t in ("reed_house", "divers_hut"):
            sx, sy = self.front(d["x"] + (1 if d["x"] <= 0 else -1) * (d["width"] / 2 + 1.55), 0.9)  # clear of the door
            self.add("hull", [sx, sy, 0.0], [2.8, 0.6, 0.35], self.m("reed", "worn"), "marker", yaw=self.rand(-8, 8))  # the canoe
            self.add("prism", [sx, sy - self.sign * 0.5, 0.0], [0.28, 0.32, 0.4], self.m("reed", "fresh"), "marker")  # basket of shells
            self.frame(-sx * 0.3, sy, self.m("goat_hair", "crumbling"), 1.5)  # nets drying
        return h

    def tent(self):
        """The black goat-hair tent [A]: a low ridge on poles, one side rolled up for the door."""
        t = self.t["id"]
        h = 3.0 if t == "warchief_tent" else 2.4
        # The door side's cloth is propped on poles at head height: the gable's "open" leg ends at "eave".
        g = self.add("gable", [0.0, 0.0, 0.0], [self.w - 0.4, self.d - 0.4, h], self.m("goat_hair"), "tent", jitter=0.06)
        g["open"], g["eave"] = self.sign, max(2.0, self.door["height"] + 0.15)
        d = self.door
        for x in (-self.w / 2 + 0.4, 0.0, self.w / 2 - 0.4):
            self.add("prism", [x, 0.0, 0.0], [0.05, 0.04, h + 0.25], self.m("palm", "worn"), "pole", jitter=0.0)
            fx = x if abs(x - d["x"]) > d["width"] / 2 + 0.2 else d["x"] + d["width"] / 2 + 0.3
            self.add("prism", [fx, self.sign * (self.d / 2 - 0.25), 0.0], [0.05, 0.04, 2.05], self.m("palm", "worn"), "pole", jitter=0.0)
        sx, sy = self.front(d["x"] + 1.4, 0.8)
        self.add("dome", [sx, sy, 0.0], [0.35, 0.35, 0.18], self.m("mudbrick", "crumbling"), "marker", tint=[0.6, 0.5, 0.4])  # dung fire
        self.soot.append([sx, sy, 0.3, 1.0])
        if t == "warchief_tent":
            self.banner(*self.front(d["x"] - 1.8, 0.8))
            self.add("box", [d["x"] - 0.6, self.sign * (self.d / 2 + 0.6), 0.0], [1.6, 1.0, 0.04], self.m("textile_madder", "worn"), "marker")  # felt


def spec(place, btype):
    b = Building(place, btype)
    tid = btype["id"]
    if tid in REED:
        b.reed()
    elif tid in TENTS:
        b.tent()
    else:
        b.house()
    b.markers()
    tris = sum(TRIS[p["shape"]] for p in b.parts)
    return {"id": b.id, "typology": place["typology"], "wealth": b.wealth, "w": b.w, "d": b.d,
            "wear": b.wear, "door": b.door, "parts": b.parts, "soot": b.soot, "tris_est": tris}


def all_specs(places, types):
    out = []
    for p in places:
        if not is_building(p):
            continue
        t = types.get(p["typology"])
        if t is None:
            raise KeyError(f"{p['id']}: typology {p['typology']!r} is not in building_types.csv")
        out.append(spec(p, t))
    return out


def signature(s):
    """What makes two buildings look alike: their parts' shapes, materials and rounded sizes."""
    return tuple(sorted((p["shape"], p["mat"], p["tag"], tuple(round(v, 1) for v in p["size"])) for p in s["parts"]))


SMOKE_CSV = os.path.join(REPO, "unreal", "Content", "Sim", "smoke.csv")


def smoke_rows(specs, places):
    """V-B5: every soot source (ovens, kilns, chimneys, fires) in world cm, plus a roof hearth for every
    home without one: place_id,x_cm,y_cm,z_cm (UE frame: the spec frame rotated by the yaw, + the centre)."""
    by_id = {p["id"]: p for p in places}
    out = []
    for sp in specs:
        p = by_id[sp["id"]]
        cx, cy, yaw = float(p["x_m"]), float(p["y_m"]), math.radians(float(p["yaw_deg"]))
        c, s = math.cos(yaw), math.sin(yaw)
        points = [(x, y, z) for x, y, z, _ in sp["soot"]]
        if not points and sp["typology"].startswith("home_"):
            sign = 1 if sp["door"]["side"] == "+y" else -1
            points = [(0.0, -sign * sp["d"] / 4, STOREY_H + 0.2)]  # the hearth's smoke hole, over the back room
        for x, y, z in points:
            out.append({"place_id": sp["id"], "x_cm": str(round((cx + c * x - s * y) * 100)),
                        "y_cm": str(round((cy + s * x + c * y) * 100)), "z_cm": str(round(z * 100))})
    out.sort(key=lambda r: (r["place_id"], int(r["x_cm"]), int(r["y_cm"]), int(r["z_cm"])))
    return out


def smoke_text(rows):
    buf = io.StringIO()
    w = csv.DictWriter(buf, ["place_id", "x_cm", "y_cm", "z_cm"], lineterminator="\n")
    w.writeheader()
    w.writerows(rows)
    return buf.getvalue()


def main(argv):
    out = argv[argv.index("--out") + 1] if "--out" in argv else os.path.join(REPO, "art", "generated", "buildings", "specs.json")
    canon = os.path.join(REPO, "db", "canon")
    places = list(csv.DictReader(open(os.path.join(canon, "places.csv"), newline="", encoding="utf-8")))
    types = {t["id"]: t for t in csv.DictReader(open(os.path.join(canon, "building_types.csv"), newline="", encoding="utf-8"))}
    specs = all_specs(places, types)
    smoke = smoke_text(smoke_rows(specs, places))
    if "--check-smoke" in argv:  # CI: the tracked smoke.csv matches the grammar
        have = open(SMOKE_CSV, encoding="utf-8", newline="").read().replace("\r\n", "\n") if os.path.exists(SMOKE_CSV) else ""
        print("smoke.csv is current" if have == smoke else "smoke.csv is stale: run tools/art/building_grammar.py")
        sys.exit(0 if have == smoke else 1)
    with open(SMOKE_CSV, "wb") as f:
        f.write(smoke.encode("utf-8"))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        json.dump(specs, f, indent=1, sort_keys=True)
    print(f"building_grammar: {len(specs)} buildings, {sum(s['tris_est'] for s in specs)} tris est -> {out}; "
          f"{smoke.count(chr(10)) - 1} smoke sources -> {os.path.relpath(SMOKE_CSV, REPO)}")


if __name__ == "__main__":
    main(sys.argv[1:])
