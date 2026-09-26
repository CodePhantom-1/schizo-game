# INVENTED LEDGER — the city's plants and clutter (Stage V batch 2)

Per D-018 and D-023: every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. No CANON row was changed.

The **species** are attested for the southern Mesopotamian alluvium and marsh (`[A]`, world-art-plan §7): the date palm and its offshoots, tamarisk, the pomegranate of the gardens, the giant reed (*Phragmites*) and cattail (*Typha*) of the marsh, water lilies on quiet water, desert grasses and saltbush on the dry ground, and the spring poppies and anemones after the rains. The **clutter** is the household of world-art-plan §5: storage jars, grain sacks, leather buckets, roof ladders, spear racks, palm-log fuel. The **numbers and rules** below are invented. Code: `db/canon/flora.csv` (data), `tools/art/scatter.py` (placement), `tools/art/scatter_mesh.py` + `tools/art/flora_gen.py` (meshes), `ASimScatter` (game).

## What grows where

| # | Choice | Values |
|---|---|---|
| 1 | Densities per 100 m2 of habitat | date palm 3.0 · offshoot 1.0 · dead palm 0.15 · tamarisk 0.25 · pomegranate 1.5 · giant reed 6.0 · cattail 2.5 · water lily 3.0 · desert grass 3.0 · saltbush 1.0 · poppy 2.0 · anemone 1.0 · stones 1.5 · jars 2.5 · sacks 1.0 · buckets 0.8 · roof ladder 0.6 · spear rack 0.2 · palm logs 0.6. Gardens (behind comfortable, elite and sacred houses) grow at twice the yard density. |
| 2 | Habitats | shore: 3 m inside to 2 m outside the lagoon edge · water: 3–12 m inside it · yard/garden: 0.5–3 m behind a building (the side away from its door) · street_edge: 0.6–1.2 m in front of it · wall_foot: 1–4 m inside the wall · open: inside the market square, brickyard and wharf, 1 m in from the edge · precinct: 6 m round the ziggurat · tombs: 8 m round each place of the Garden of Tombs quarter · camp: 5 m round each tent |
| 3 | Rooted water plants (reeds, cattails) stand where the lagoon is at most 0.3 m deep (4.5 m inside its edge on the terrain's slope); lilies float anywhere in the water band | — |
| 4 | Seasons: poppies and anemones show in the rains and sowing, lilies in the rains, sowing and harvest; the rest all year | — |
| 5 | Withering: leaves of palms, bushes, grasses, reeds, tamarisk and saltbush brown to dry straw (palette `reed_2`) as the drought rises (stage / 4); flowers, lilies, stones and clutter do not | — |
| 6 | Kept clear: 2.5 m round every door slot, 0.5 m round every footprint, the gate passages (4.5 m either side of the gate's axis, 8 m beyond its depth), the wall, the channels and the sea | — |

## The meshes

| # | Choice | Values |
|---|---|---|
| 7 | Real sizes (longest side): tall palm 10 m, bent 9 m, detailed 12 m, short 5 m; bushes 0.9–1.8 m; grasses 0.4–0.8 m; flowers 0.35–0.5 m; lily pads 0.6–1.2 m; stones 0.5–0.9 m; jars 0.55–0.7 m; sack 0.8 m; bucket 0.4 m; ladder 3.2 m; spear rack 1.8 m; log 2.5 m, log pile 2.0 m | `art/scatter_meshes.csv` `size_m` |
| 8 | Code-built plants: reed clump (14–20 blades 1.8–3.2 m, 3–5 plumes), cattail (8–12 blades, 3–5 brown spikes), tamarisk (a 1.4 m trunk under 5–7 grey-green crowns), saltbush (three grey-green lumps), dead palm (a 6.5 m grey trunk leaning 6–12°, three broken frond stubs) | `flora_gen.py` |
| 9 | Kenney's cartoon colours mapped to the palette by hand: leaves `leaf_1`, dark leaves `leaf_0`, grass `leaf_2`, bark `palm_2`, pots terracotta `ochre_1`/`ochre_0`, log ends `reed_3`, stone `stone_2`, red flowers `madder_1`, purple `indigo_1`, yellow `saffron_1`, white `cream_0` | `MATERIAL_MAP` |
| 10 | Collision: trees, the tamarisk and props block with one box; grass, flowers, lilies, reeds and bushes do not | `ue_import_scatter.py` |
| 11 | Cull distances: grass and flowers 60 m, shrubs, water plants and props 150 m, trees never; grass, flowers and water plants cast no shadow | `ASimScatter` |
| 12 | Wind: leaves sway 3 cm at 300 cm up, phase by world x; stones and props never move | `M_Scatter` |

## Out, and why (world-art-plan §10)

Only the meshes listed in `art/scatter_meshes.csv` are used; `test_art_flora.py` fails on any file whose name holds a banned word. From the packs, left out on purpose: **barrels and crates** (jars, baskets and sacks held the goods), **cacti** (New World), **pines, oaks and mushrooms** (a European forest, not the alluvium), **windmills and water-wheels** (not of the period), **chickens** (not yet in Mesopotamia), coins and glass (not in daily use). Kenney's "palm" is a coconut form; its fronds are snapped to dusty palette greens and its fruit reads as date clusters.
