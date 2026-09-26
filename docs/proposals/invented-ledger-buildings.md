# INVENTED LEDGER — the generated buildings (Stage V batch 1)

Per D-018 and D-023: every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. No CANON row was changed.

The **forms** are attested general practice (`[A]`): mudbrick walls on an earth, fired-brick or stone footing; flat earth roofs on palm beams whose ends show in the wall; parapets; high small windows; whitewash and ochre bands on the better houses; recessed buttress panels on temples; cone mosaic (Uruk) and glazed brick for the grandest; the arched reed house and the mudhif of the southern marshes; the black goat-hair tent of the herders. The **numbers and rules** below are invented. Code: `tools/art/building_grammar.py` (recipe), `tools/art/building_mesh.py` (mesh), `tools/art/trim_atlas.py` (textures).

## Palette and atlas

| # | Choice | Where |
|---|---|---|
| 1 | 64 colours in `art/palette.csv` (earths, straws, limes, fired brick, bitumen, reed, palm, cedar, lapis, gold glaze, madder, indigo, saffron, copper, patina, basalt, limestone). Every texture is locked to it (no dither). | `stylize.py` |
| 2 | 16 materials x 4 wear states (fresh, worn, crumbling, drought-cracked) in one 8x8 atlas of 256 px cells. Photo cells from CC0 Poly Haven / ambientCG; cone mosaic, glazed lapis, ochre band, textiles and goat hair procedural. | `trim_atlas.py` |

## The grammar

| # | Choice | Values |
|---|---|---|
| 3 | Wear by wealth: odds of fresh / worn / crumbling | poor .15/.40/.45 · modest .35/.50/.15 · comfortable .50/.45/.05 · elite .75/.25/0 · civic .45/.45/.10 · sacred .80/.20/0 |
| 4 | Door width / height by wealth | poor 0.8/1.8 · modest 0.9/1.95 · comfortable 1.0/2.05 · elite 1.2/2.2 · civic 1.3/2.2 · sacred 1.4/2.3 m |
| 5 | Storey 2.8 m, wall 0.4 m, roof 0.25 m; mudbrick and plaster walls batter 4 cm | — |
| 6 | The lintel is cedar for comfortable, elite and sacred houses; palm for the rest. Rich doors get fired-brick jambs. | — |
| 7 | Every non-home typology shows its trade outside: bakery ovens, potter's kiln and wheel, furnace chimney and slag for smiths, looms and dye vats, hide frames and a soaking pit for the tannery, the oil press beam, racks of spears and bows, granary domes, a shrine niche with a lamp, a counter under an awning, a boat on its trestle. Homes of any typology fall back to their wealth's generic house. | `Building.markers` |
| 8 | Reed halls: a barrel vault over the depth with a door-wide gap at the door (walkable); ribs every 1.0 m (mudhif) / 1.3 m (reed house); reed houses stand on a platform 0.4 m high (one step). | `Building.reed` |
| 9 | Tents: a goat-hair ridge 2.4 m (3.0 m warchief); the door side propped on poles at the door's height + 0.15 m. | `Building.tent` |
| 10 | Nothing stands in a doorway above a 0.45 m step (UE's MaxStepHeight); the door is at `door_x(w)`, the kit room's door cell. | tested |

## The mesh

| # | Choice | Values |
|---|---|---|
| 11 | Texture tile: plaster, whitewash, packed earth 2 m; stone 1.5 m; everything else 1 m. | `TILE_M` |
| 12 | Grime in vertex colour: damp darkening at the base (35 % x the wear factor: fresh 0.5, worn 1.0, crumbling 1.4, over 0.7 m), soot 55 % near ovens, furnaces and fires. | `grime()` |
| 13 | Drought cracking weight (vertex alpha): earthen surfaces 1, fired brick 0.5, stone 0.3, wood and reed 0.1, cloth 0. `M_Building`'s `DroughtWear` (0 by default) blends toward the cracked cell. | `EARTH` |
| 14 | Budgets: 3,500 triangles per building; 6,000 for elite, sacred and plots over 200 m². | `budget()` |
