# INVENTED LEDGER — the terrain: ground detail, landforms and the countryside (Stage V batch 3)

Per D-018 and D-023: every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. No CANON row was changed.

The **forms** are attested for the lower Mesopotamian alluvium (`[A]`, world-art-plan §6): river and canal levees with date groves on them, strip fields with furrows, tells (the mounds of older settlements) on the horizon, salt crusting the fields as irrigation fails, cracked mud in the drought, reed banks, dune scrub. The **numbers** below are invented. Tommy's terrain colours are unchanged (a pinned hash proves it); only the detail and the relief are added. Code: `tools/art/ground_atlas.py`, `tools/art/ue_make_terrain_material.py`, `ASimEnvironment` (`GroundKind`, `TerrainHeight`), `ASimScatter::ScatterCountryside`.

## Ground detail (M_Terrain)

| # | Choice | Values |
|---|---|---|
| 1 | 16 ground kinds, one tiling detail cell each (512 px, 4 m of ground per tile), luminance mean 0.5 so Tommy's colour is kept: silt, irrigated, cracked, salt, sand, gravel, reed-mud, beach, road, street, bed, rock, hills, dune, tell, spare | `ground_atlas.py` |
| 2 | Kinds by region, in `TerrainColor`'s order: lagoon bed · wet mud and reed banks = reed-mud · the city = street · beach · the road west · fields: irrigated (crop hash < 0.52), silt (< 0.76), cracked (the rest), salt where a field noise > 0.75 · far hills (rock if steep) · steep open ground = gravel · desert > 0.6 dune, > 0.25 sand, else silt · tells | `GroundKind` |
| 3 | Slopes past 30° blend to side projections (triplanar) | `M_Terrain` |
| 4 | Furrows on irrigated fields: a stripe every 90 cm along X, up to 12% darker | `M_Terrain` |
| 5 | The drought (`MPC_World.Wither` = stage / 4) turns silt and irrigated ground to cracked mud, or to salt crust where a low world noise > 0.6, and leans their colour to the cracked field colour (184,152,110) or the salt bloom (232,228,214); street, road, beach and bed never change | `M_Terrain` |
| 6 | Tommy's world noise (0.86–1.08, scale 0.0045) and his roughness 0.92 / specular 0.25 kept exactly | `M_Terrain` |

## Landforms

| # | Choice | Values |
|---|---|---|
| 7 | Levees: +70 cm, falling to 0 over ~45 m, beside the river (from 52 m off its centre) and each canal (from its 5.2 m edge) | `LeveeWeight` |
| 8 | Tells: five, 3–4.5 km out between south and north through west, 250–600 m across, 8–18 m high above a plateau at the mean ground of their foot, flat-topped (full height inside 0.6 of the radius); 500 m + radius clear of the road west, the river, the fields and the sea, and of each other | `TellList` |
| 9 | Erosion gullies: five, 80 cm deep, ~50 m wide, meandering (Perlin) down the dunes south | `TerrainHeight` |
| 10 | No field dykes in the geometry: 60 cm dykes cannot read on the 15 m terrain grid (they would alias into random bumps); the furrows are in the material | ruling |
| 11 | Every landform is applied before the city apron, lagoon, road, canal, river and sea terms, so those always win | `TerrainHeight` |

## The countryside scatter

| # | Choice | Values |
|---|---|---|
| 12 | A jittered 4 m grid over x −900..600 m, y −600..700 m round the crescent, plus each tell's disc; habitat from the ground kind: tell → tell_top, a levee crest (weight > 0.5) → levee, irrigated → field, reed-mud → bank, dune or sand → desert | `ScatterCountryside` |
| 13 | New rows: barley shoots (sowing) and ripe barley (harvest), 6 per 100 m² each; bare furrow rows (rains, vintage), 3; potsherds on the tells, 4. Date palms extend to levees, giant reed to banks, desert grass and saltbush to the desert | `flora.csv` |
| 14 | Countryside density against the table (the table's numbers are the city's): desert × 0.2 (open scrub), levee × 0.5 (groves), tell top × 0.05 (sherd patches on a 250–600 m mound) | `HabitatDensity` |
| 15 | Kept clear: 6 m either side of the road west from the Moon Gate; 3 m round every palm Tommy's countryside plants | `ScatterCountryside` |
| 16 | Budget 60,000 instances (today ~44,500); crops and grass culled at 250 m, trees never | `ASimScatter` |
| 17 | Barley colours: shoots `leaf_1`, ripe `reed_3` (Kenney's ripe wheat material would read as terracotta); dirt rows `mud_4`/`mud_2` | `scatter_mesh.py` |
