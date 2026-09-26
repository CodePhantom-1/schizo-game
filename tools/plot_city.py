#!/usr/bin/env python3
"""plot_city.py — AA2: a top-down map of the City of the Moon from places.csv.

Draws the lagoon, the crescent wall, the sea and every footprint coloured by
its group, with the monuments labelled. Writes art/review/city_plan.png.
Usage: python3 tools/plot_city.py [out.png] [--scatter]
  --scatter: also dot every instance of unreal/Content/Sim/scatter.csv, coloured by its flora group
             (default out: art/review/scatter_plan.png)
"""
import csv
import math
import pathlib
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Polygon, Wedge

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import city_layout  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
COLOURS = {  # by building_types.csv group
    "home": "#c9a57a", "trade": "#d9b44a", "workshop": "#b0693f", "civic": "#8a7f72",
    "sacred": "#e8e2d4", "learning": "#6f8fb8", "monument": "#2f5d9c", "rural": "#9bb070",
}
SCATTER_COLOURS = {  # by flora.csv group
    "tree": "#2e6b2a", "shrub": "#7a9a3a", "grass": "#b8c46a", "water": "#1f4f8f",
    "flower": "#c0392b", "crop": "#d9b44a", "prop": "#5a3a22",
}


def plot_scatter(ax):
    group = {}
    for f in csv.DictReader(open(ROOT / "db/canon/flora.csv", encoding="utf-8")):
        for m in f["meshes"].split(";"):
            group.setdefault(m, f["group"])
    for r in csv.DictReader(open(ROOT / "unreal/Content/Sim/scatter.csv", encoding="utf-8")):
        g = group.get(r["mesh"], "prop")
        ax.plot(int(r["x_cm"]) / 100, int(r["y_cm"]) / 100, "o", ms=2.2 if g == "tree" else 1.3,
                color=SCATTER_COLOURS[g], zorder=5)
    return [plt.Line2D([], [], marker="o", ls="", color=c, label=f"scatter: {g}") for g, c in SCATTER_COLOURS.items()]


def main(out, scatter=False):
    types = {t["id"]: t for t in csv.DictReader(open(ROOT / "db/canon/building_types.csv", encoding="utf-8"))}
    places = list(csv.DictReader(open(ROOT / "db/canon/places.csv", encoding="utf-8")))
    fig, ax = plt.subplots(figsize=(14, 12), dpi=110)
    ax.set_facecolor("#e9dfc9")                       # dry alluvium
    ax.add_patch(plt.Rectangle((city_layout.COAST_X, -400), 600, 900, color="#5b86a8"))            # the sea
    ax.add_patch(Circle(city_layout.O, 105, color="#7aa3b8"))                      # the lagoon
    ax.add_patch(Wedge(city_layout.O, 177, -15, 195, width=4, color="#7a5c3e"))    # the wall
    ax.add_patch(plt.Rectangle((city_layout.O[0] - 4, -330), 8, 330 - 105 + 20, color="#7aa3b8"))  # canal to the river
    for p in places:
        if p["quarter"] == "beyond_the_gate":
            continue
        t = types[p["typology"]]
        ax.add_patch(Polygon(city_layout.corners(p), closed=True, facecolor=COLOURS.get(t["group"], "#999"),
                             edgecolor="#3a2c20", linewidth=0.6))
        if t["group"] in ("monument", "learning") or p["typology"] in ("council_hall", "moon_pool", "necropolis", "mudhif", "caravan_yard", "market_square", "gipar"):
            ax.annotate(p["name"], (float(p["x_m"]), float(p["y_m"])), fontsize=7, ha="center", va="center",
                        color="#1d1712", bbox=dict(boxstyle="round,pad=0.2", fc="#ffffffcc", ec="none"))
    ax.set_xlim(-60, 520)
    ax.set_ylim(330, -170)  # north (-y, D-026) up: the map matches the 3D world
    ax.set_aspect("equal")
    ax.set_title("The City of the Moon — the crescent (metres; north up)", fontsize=12)
    handles = [plt.Line2D([], [], marker="s", ls="", color=c, label=g) for g, c in COLOURS.items()]
    if scatter:
        handles += plot_scatter(ax)
    ax.legend(handles=handles, loc="lower left", fontsize=8)
    out = pathlib.Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, bbox_inches="tight")
    print(f"plot_city: {out}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--scatter"]
    scatter = "--scatter" in sys.argv[1:]
    default = "art/review/scatter_plan.png" if scatter else "art/review/city_plan.png"
    main(args[0] if args else ROOT / default, scatter)
