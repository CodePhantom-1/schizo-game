"""star_dome.py — the night sky (Stage V batch 5 Task 3): the Yale Bright Star Catalogue into the game's sky.

    python3 tools/art/star_dome.py            # writes unreal/Content/Sim/stars.csv
    python3 tools/art/star_dome.py --check    # exit 1 when the file is stale

Reads art/source/bsc5/catalog (public domain; tools/art/sources.py bsc5) and writes one row per sky quad, which
ASimAtmosphere draws as instanced, camera-following, emissive cards 900 m out:
  star    every star brighter than magnitude 5.0: size and brightness by magnitude, colour by B-V
  zodiac  the twelve ecliptic constellations' figures: the minimum spanning tree of each one's stars brighter than
          4.5 (INVENTED — the figures are drawn by distance, not from any tablet); one thin segment per edge
  milky   the Milky Way: soft blobs scattered about the galactic plane (pole RA 192.86, Dec 27.13), thickest
          toward the centre in Sagittarius, the Great Rift darker (procedural, seeded)
Positions are unit vectors in UE world space (SimCompass: +X east, -Y north, +Z up) at sidereal angle 0; the game
turns the dome about POLE by SIDEREAL_SIGN x sidereal(day, hour) (see rotate), tilted to latitude 31 N.
"""
import math
import os
import random
import sys
from collections import namedtuple

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CATALOGUE = os.path.join(REPO, "art", "source", "bsc5", "catalog")
OUT = os.path.join(REPO, "unreal", "Content", "Sim", "stars.csv")

LATITUDE = 31.0  # the city's latitude (degrees north)
MAG_LIMIT = 5.0
ZODIAC = ("Ari", "Tau", "Gem", "Cnc", "Leo", "Vir", "Lib", "Sco", "Sgr", "Cap", "Aqr", "Psc")
ZODIAC_MAG = 4.5
OBLIQUITY = 23.44
GAL_POLE = (192.859, 27.128)  # RA, Dec of the north galactic pole (J2000)
GAL_L_NCP = 122.932  # galactic longitude of the north celestial pole
MILKY_BLOBS = 2600
FIELDS = ("kind", "hr", "con", "ra", "dec", "x", "y", "z", "x2", "y2", "z2", "size", "r", "g", "b")

_PHI = math.radians(LATITUDE)
POLE = (0.0, -math.cos(_PHI), math.sin(_PHI))  # the celestial pole in world space: 31 degrees up in the north
SIDEREAL_SIGN = 1.0  # UE's world is a mirror of the real sky's frame: the turn about POLE is +angle there

Star = namedtuple("Star", "hr con ra dec mag bv glon glat")


def parse(path):
    """The catalogue's fixed columns (ReadMe, V/50): every star with a J2000 place and a V magnitude."""
    stars = []
    with open(path, encoding="latin-1") as f:
        for line in f:
            if len(line) < 107 or not line[75:77].strip() or not line[102:107].strip():
                continue  # the 14 non-stellar entries (novae, clusters) have no position
            ra = 15.0 * (int(line[75:77]) + int(line[77:79]) / 60.0 + float(line[79:83]) / 3600.0)
            dec = int(line[84:86]) + int(line[86:88]) / 60.0 + int(line[88:90]) / 3600.0
            if line[83] == "-":
                dec = -dec
            bv = line[109:114].strip()
            stars.append(Star(int(line[0:4]), line[11:14].strip(), ra, dec, float(line[102:107]),
                              float(bv) if bv else 0.6, float(line[90:96]), float(line[96:102])))
    return stars


def world_dir(ra, dec, lst):
    """The unit vector toward (ra, dec) at local sidereal angle lst (degrees), in UE world space."""
    h, d = math.radians(lst - ra), math.radians(dec)
    along, west, up = math.cos(d) * math.cos(h), math.cos(d) * math.sin(h), math.sin(d)
    # The equatorial frame (the meridian's equator point M, west W, the pole P) in east/north/up at latitude phi.
    e = -west
    n = -math.sin(_PHI) * along + math.cos(_PHI) * up
    u = math.cos(_PHI) * along + math.sin(_PHI) * up
    return (e, -n, u)  # SimCompass: north is -Y


def rotate(v, axis, angle):
    """Rodrigues' rotation (what FQuat(axis, angle) does to a vector)."""
    c, s = math.cos(angle), math.sin(angle)
    kx, ky, kz = axis
    x, y, z = v
    dot = kx * x + ky * y + kz * z
    cx, cy, cz = ky * z - kz * y, kz * x - kx * z, kx * y - ky * x
    return (x * c + cx * s + kx * dot * (1 - c), y * c + cy * s + ky * dot * (1 - c), z * c + cz * s + kz * dot * (1 - c))


def sidereal(day, hour):
    """The sky's turn in degrees (the plan's formula: a sidereal day is 4 minutes short of a solar one)."""
    return (day * 360.9856 + hour * 15.041) % 360.0


def separation(ra1, dec1, ra2, dec2):
    a, b = (math.radians(x) for x in (dec1, dec2))
    c = math.sin(a) * math.sin(b) + math.cos(a) * math.cos(b) * math.cos(math.radians(ra1 - ra2))
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def ecliptic_latitude(ra, dec):
    e, a, d = math.radians(OBLIQUITY), math.radians(ra), math.radians(dec)
    return math.degrees(math.asin(math.sin(d) * math.cos(e) - math.cos(d) * math.sin(e) * math.sin(a)))


def galactic_latitude(ra, dec):
    ag, dg = (math.radians(x) for x in GAL_POLE)
    a, d = math.radians(ra), math.radians(dec)
    return math.degrees(math.asin(math.sin(d) * math.sin(dg) + math.cos(d) * math.cos(dg) * math.cos(a - ag)))


def galactic_to_equatorial(l, b):
    ag, dg = (math.radians(x) for x in GAL_POLE)
    b, dl = math.radians(b), math.radians(GAL_L_NCP - l)
    dec = math.asin(math.sin(dg) * math.sin(b) + math.cos(dg) * math.cos(b) * math.cos(dl))
    ra = ag + math.atan2(math.cos(b) * math.sin(dl), math.cos(dg) * math.sin(b) - math.sin(dg) * math.cos(b) * math.cos(dl))
    return math.degrees(ra) % 360.0, math.degrees(dec)


def bv_colour(bv):
    """B-V to a star's colour (Ballesteros' temperature, Helland's blackbody fit), half-way to white."""
    bv = max(-0.4, min(2.0, bv))
    t = 4600.0 * (1.0 / (0.92 * bv + 1.7) + 1.0 / (0.92 * bv + 0.62)) / 100.0
    r = 255.0 if t <= 66 else 329.698727446 * (t - 60) ** -0.1332047592
    g = 99.4708025861 * math.log(t) - 161.1195681661 if t <= 66 else 288.1221695283 * (t - 60) ** -0.0755148492
    b = 255.0 if t >= 66 else (0.0 if t <= 19 else 138.5177312231 * math.log(t - 10) - 305.0447927307)
    return tuple(0.5 + 0.5 * max(0.0, min(255.0, c)) / 255.0 for c in (r, g, b))


def spanning_tree(stars):
    """Prim's minimum spanning tree over angular distance: a constellation's figure, drawn by nearness."""
    if len(stars) < 2:
        return []
    inside, edges = {0}, []
    while len(inside) < len(stars):
        i, j = min(((i, j) for i in inside for j in range(len(stars)) if j not in inside),
                   key=lambda e: separation(stars[e[0]].ra, stars[e[0]].dec, stars[e[1]].ra, stars[e[1]].dec))
        inside.add(j)
        edges.append((stars[i], stars[j]))
    return edges


def row(kind, hr, con, ra, dec, a, b, size, rgb):
    return {"kind": kind, "hr": hr, "con": con, "ra": ra, "dec": dec, "x": a[0], "y": a[1], "z": a[2],
            "x2": b[0], "y2": b[1], "z2": b[2], "size": size, "r": rgb[0], "g": rgb[1], "b": rgb[2]}


def build(stars):
    out = []
    for s in sorted((s for s in stars if s.mag < MAG_LIMIT), key=lambda s: s.hr):
        # Size: 2.8 m (0.18 degrees at 900 m, two pixels) at magnitude 5, three times that at 0.
        # Brightness: a gentler ramp than the eye's 2.512 per magnitude, or the faint half would vanish.
        size = 280.0 * 10 ** (0.1 * (MAG_LIMIT - s.mag))
        bright = min(2.5, 10 ** (-0.2 * (s.mag - 1.0)))
        out.append(row("star", s.hr, s.con, s.ra, s.dec, world_dir(s.ra, s.dec, 0.0), (0.0, 0.0, 0.0), size,
                       tuple(c * bright for c in bv_colour(s.bv))))
    for con in ZODIAC:
        members = sorted((s for s in stars if s.con == con and s.mag < ZODIAC_MAG), key=lambda s: s.mag)
        if len(members) < 4:  # Cancer is faint: its four brightest
            members = sorted((s for s in stars if s.con == con), key=lambda s: s.mag)[:4]
        for a, b in spanning_tree(members):
            out.append(row("zodiac", a.hr, con, a.ra, a.dec, world_dir(a.ra, a.dec, 0.0), world_dir(b.ra, b.dec, 0.0),
                           60.0, (0.07, 0.06, 0.035)))
    rng = random.Random(1987)
    phase = [rng.uniform(0, 2 * math.pi) for _ in range(3)]
    while sum(1 for r in out if r["kind"] == "milky") < MILKY_BLOBS:
        l = rng.uniform(0.0, 360.0)
        dl = min(l, 360.0 - l)
        core = math.exp(-(dl / 60.0) ** 2)  # brighter toward the centre in Sagittarius
        if rng.random() > 0.35 + 0.65 * core:
            continue
        b = rng.gauss(0.0, 3.5 + 5.0 * math.exp(-(dl / 25.0) ** 2))  # the bulge is thicker
        clump = 0.6 + 0.4 * math.sin(math.radians(l) * 7 + phase[0]) * math.sin(math.radians(l) * 3 + phase[1])
        glow = 0.018 * (0.4 + 0.6 * core) * clump
        if 10.0 < l < 60.0 and abs(b) < 2.5:
            glow *= 0.3  # the Great Rift
        ra, dec = galactic_to_equatorial(l, b)
        out.append(row("milky", 0, "", ra, dec, world_dir(ra, dec, 0.0), (0.0, 0.0, 0.0), rng.uniform(9000.0, 16000.0),
                       (glow * 0.85, glow * 0.9, glow)))
    return out


def fmt(v):
    return f"{v:.4f}" if isinstance(v, float) else str(v)


def render(rows):
    return ",".join(FIELDS) + "\n" + "".join(",".join(fmt(r[k]) for k in FIELDS) + "\n" for r in rows)


def read_csv(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            vals = line.rstrip("\n").split(",")
            r = dict(zip(FIELDS, vals))
            for k in FIELDS[3:]:
                r[k] = float(r[k])
            r["hr"] = int(r["hr"])
            rows.append(r)
    return rows


def main(argv):
    if not os.path.exists(CATALOGUE):
        print(f"star_dome: {CATALOGUE} missing: python3 tools/art/sources.py bsc5")
        return 1
    text = render(build(parse(CATALOGUE)))
    if "--check" in argv:
        same = os.path.exists(OUT) and open(OUT, encoding="utf-8").read() == text
        print(f"star_dome: {OUT} {'up to date' if same else 'STALE: run tools/art/star_dome.py'}")
        return 0 if same else 1
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    kinds = {}
    for r in read_csv(OUT):
        kinds[r["kind"]] = kinds.get(r["kind"], 0) + 1
    print(f"star_dome: {kinds} -> {OUT} ({2 * sum(kinds.values())} triangles)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
