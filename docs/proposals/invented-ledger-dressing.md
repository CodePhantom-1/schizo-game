# INVENTED LEDGER — the dressing wave (props, sky, post polish)

Per D-018 and D-023: every choice below is authored glue tagged `INVENTED`. It fits the theme and the canon, contradicts no CANON or A row or the notes, and the designer may veto it. A veto is appended to DECISIONS.md and supersedes only that line. No CANON row was changed.

The **objects** are attested general practice (`[A]`): clay jars, reed baskets and matting, low wooden furniture, tripod braziers and waterskins are the universal kit of Mesopotamian daily life (Ur III / Old Babylonian material culture, e.g. Woolley's Ur excavations, standard treatments of Mesopotamian everyday objects). The **numbers and rules** attached to them are invented. Meshes are original procedural builds (Blender headless, `tools/art/props_gen.py`), CC0 ambientCG surface maps where a surface reads (`M_Reed` = Wicker010A, `M_Timber` = Bark012 — both already in `fetch_textures.py`'s set); no other source material is used.

## The prop set (all under 300 tris, D-023 low-fi; tri counts measured)

| # | Choice | Where |
|---|---|---|
| 1 | **SM_PropAmphora** — a 61 cm plain clay storage jar (water/beer), revolved profile incl. the inward lip so the mouth reads open. 260 tris | `tools/art/props_gen.py` `build_amphora` |
| 2 | **SM_PropJarStack** — the granary cluster: one big squat grain jar, a smaller one on its rim, a third beside it. 288 tris | `build_jar_stack` |
| 3 | **SM_PropBasket** — a woven reed basket heaped with hulled barley (reed weave = CC0 Wicker010A). 160 tris | `build_basket` |
| 4 | **SM_PropMatRoll** — an 80 cm rolled reed mat lying on its side. 92 tris | `build_mat_roll` |
| 5 | **SM_PropBench** — a low 90x25 cm timber bench, seat at 26 cm, worn-edge bevel (CC0 Bark012). 264 tris | `build_bench` |
| 6 | **SM_PropDatesTray** — a 45x30 cm wooden tray heaped with dried dates (108 tris) | `build_dates_tray` |
| 7 | **SM_PropBreadTray** — a bakery tray with three barley flatbreads. 156 tris | `build_bread_tray` |
| 8 | **SM_PropBreadLoaves** — two loose flat loaves (cook-shop counter). 64 tris | `build_bread_loaves` |
| 9 | **SM_PropBrazier** — a standing tripod brazier, unlit (timber legs, clay bowl). 176 tris | `build_brazier` |
| 10 | **SM_PropWaterSkin** — a filled hide waterskin hanging from a 1.05 m post. 88 tris | `build_water_skin` |
| 11 | **SM_SkyDome** — a 120 m inverted hemisphere, 16 segments, rings every 15 degrees + one skirt ring below the horizon, capped, normals inward. 224 tris | `build_sky_dome` |

Prop materials (flat D-023 colours, runtime-applied per FBX slot via the engine parameter material — same mechanism as `SimStreetBuilder::LoadKitMeshes`): `M_Clay` (161,92,56), `M_Dates` (82,41,13), `M_Bread` (184,128,72), `M_Hide` (140,107,77), `M_Grain` (199,166,90), `M_SkyDomeSlot` (154,178,204); the kit's four slots keep their street colours.

## Placement rules (deterministic from the street registries; `SimPropsBuilder.cpp`)

Per-place variation is hashed with `FCrc::StrCrc32(place_id)` (the street builder's own method — stable across sessions). Yaw jitter = `(hash % 8) * 7.5 - 26.25` degrees. Props sit beside doorways, never in them: 55 cm off the wall face, 85 cm to the door's +Right side (for south doors that is +X — clear of the roof-access stair, which hugs the west end of the wall).

| # | Rule | Values (cm, UE units) |
|---|---|---|
| 12 | **Houses** (kind `house`): a bench or a rolled mat beside every door; which one = `hash % 3 == 0` ? bench : mat; yaw = outward+90 (wall-parallel) + jitter | 8 houses dressed |
| 13 | **Bakery**: two bread trays flanking the door, 70 cm off the wall, 85 cm either side, wall-parallel; no collision (ankle-height) | |
| 14 | **Granary**: two jar stacks flanking the door, 70 cm off the wall, 80 cm either side | |
| 15 | **Temple**: an offering brazier (110 cm to the door's -Right, 55 off the wall) + two rolled mats stacked (second at z +18, the roll's diameter) at +Right 115, 75 off the wall | |
| 16 | **Smithy**: one brazier by the yard door (fire stand-in; the furnace itself belongs to the smithy wave) | |
| 17 | **Brewery/tavern**: a drinking bench +Right of the door, one beer amphora at -Right 100 | |
| 18 | **Watch post**: the night watch's brazier, +Right 90 | |
| 19 | **Gate**: two braziers just inside the city, at (+60, ±160) off the gate centre — flanking the street, clear of pillars, tablet and PlayerStart | |
| 20 | **Stalls** (ids `stall_*` + `cookshop_place`): one basket every stall at tile-centre (-25, -10, z=8 — the courtyard tile's top); counter goods vary by `hash % 4`: 0 dates tray, 1 bread loaves, 2 amphora, 3 jar stack — at (+25..30, ~5, 8), under the 2 m awning | 5 stalls + cook-shop |
| 21 | **Well** (`street_well_place`): a bench at (0, -115), two amphorae at (85, 30) and (-80, 55), a waterskin post at (105, -75) — all just off the 1 m tile, on the ground (z=0) | |
| 22 | Missing meshes are skipped silently (one summary log line); the street never depends on the props import. `sim.PropsDressing 0` disables the whole pass | |

## The sky (no SkyLight anywhere — see DECISIONS.md / `SimStreetBuilder` for the GPU-capture hang)

| # | Choice | Values |
|---|---|---|
| 23 | **M_SkyDome** (`/Game/Art/Sky`, authored headless by `ue_import_props.py`): Shading Model **Unlit**, TwoSided, Emissive = `Lerp(horizon, zenith, clamp(AbsWorldPosition.Z / DomeRadius))`. Horizon (dusk-warm dust) = **(0.86, 0.58, 0.36)**, zenith (pale dusty blue) = **(0.38, 0.52, 0.68)**, linear. `DomeRadius` is a ScalarParameter, default 12000 cm, set by the actor so the engine-sphere fallback matches the kit dome | `tools/art/ue_import_props.py` `build_sky_material` |
| 24 | **ASimSkyDome**: spawns at the average of the street's place locations (falls back to the world origin), radius 120 m; kit SM_SkyDome preferred, engine Sphere scaled `12000/50 = 240` as pre-import fallback; collision off, casts no shadows, never captures. Day/night tinting of the gradient is deliberately deferred (the wave's scope: the street stops reading as a void) | `SimPropsBuilder.cpp` |

## Post polish (one unbound weight-1 PostProcessVolume, spawned at runtime)

| # | Setting | Value | Why |
|---|---|---|---|
| 25 | `bOverride_WhiteTemp` / `WhiteTemp` | **6000 K** (6500 = neutral) | subtle warm-white balance, the dust-light look |
| 26 | `bOverride_VignetteIntensity` / `VignetteIntensity` | **0.5** (engine default 0.4) | a slight eye-draw to the street |
| 27 | `bOverride_FilmSaturation` / `FilmSaturation` | **1.1** (1.0 = neutral) | mild filmic saturation |
| 28 | `bOverride_FilmContrast` / `FilmContrast` | **0.05** | gentle filmic contrast |

All four are conservative, reversible in one place (`SpawnPostPolish`), and were chosen blind (no compiled editor this wave) — expect a tuning pass once the dressing is first seen in-engine.
