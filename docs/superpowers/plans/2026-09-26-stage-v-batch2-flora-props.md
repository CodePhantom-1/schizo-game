# Stage V batch 2 — flora and street life: the fetched packs and code-built plants, scattered from data

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or subagent-driven-development). Steps use checkbox (`- [ ]`) syntax. Batch 1 (`2026-09-26-stage-v-batch1-asset-pipeline.md`) is done; read its Status section first — its rulings and machine notes carry over.

**Goal:** The crescent stops being bare sand: date palms, reeds, tamarisk, bushes, grasses, lilies, spring flowers, stones and household clutter stand where their habitat and their owners' trades put them, drawn from the fetched CC0 packs (Kenney nature kit, KayKit) and a few code-built Mesopotamian plants, all palette-locked, all placed by a tested rule set, withering with the drought and flowering in season.

**Architecture:** Plain-Python placement (`tools/art/scatter.py`) reads the canon (`places.csv`, `city_districts.csv`, the new `flora.csv`) and writes one tracked table of instances (`unreal/Content/Sim/scatter.csv`), checked current in CI like `places.csv`. Blender stylizes the pack meshes and builds the code-made plants into GLBs (one per mesh id) with the colour in vertex colour. UE imports them to `/Game/Art/Scatter` with one material `M_Scatter`, and a new `ASimScatter` actor stands one HISM per mesh, z from `ASimEnvironment::TerrainHeight`, withering and season from the kernel through a material parameter collection.

**Tech stack:** Python 3 + Pillow; Blender 5.2 LTS headless; UE 5.8 Python (Interchange glTF); C++ (SchizoGame module, HISM).

**Spec:** `docs/world-art-plan.md` §5 (props and clutter), §7 (flora), §10 (accuracy guard rails), §12 (how it gets built), §13 V4/V6; `reports/Automated game asset pipeline.md` stages 3–6.

## Global Constraints

- D-023 look: low-poly, flat-shaded, chunky; colour in vertex colour, every colour snapped to `art/palette.csv` (64 colours).
- Licence allowlist as batch 1 (`tools/art/license_gate.py`): every new source row in `art/assets.csv`; CI fails otherwise.
- §10 guard rails — nothing from the packs that contradicts the period: **no barrels, crates, cacti, pines, oaks, mushrooms, windmills, water-wheels, chickens, coins, glass.** The allowed list is explicit (Task 2); anything not on it stays out.
- Every door slot stays clear: no instance within 2.5 m of a door slot (`ASimStreetBuilder::KitDoorLocal`, mirrored in Python as `door_point()`), none inside a building footprint grown by 0.5 m.
- Determinism: same canon → byte-identical `scatter.csv` (seeded `random.Random`, sorted output).
- Budgets: flora mesh ≤ 1,500 tris, prop ≤ 600; the whole city ≤ 40,000 instances; HISM with cull distances (grass 60 m, bushes 150 m, palms never).
- Rendering runs on the designer's Linux box use the iGPU: `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`. Tommy (Windows, RTX 3060) needs no prefix.
- Paths: Blender `$BLENDER` (Linux `~/.local/bin/blender`), UE `$UE_ROOT` (Linux `~/UnrealEngine`). UE python scripts need an **absolute** `-script=` path.

## Review Focus

1. A place added to `places.csv` after the scatter was generated: CI's `scatter.py --check` must fail (stale), never let the game stand a palm inside a new house.
2. A fresh clone before `ue_import_scatter.py`: `ASimScatter` must stand nothing, log one line, and not crash or spam 60 warnings.
3. Drought stage 0 vs 4: stage 0 must look exactly as authored (Wither = 0), stage 4 must visibly brown every leafy mesh but not stones, pots or sacks (vertex alpha masks leaves only).
4. A mesh row whose source file is missing on disk (a pack updated upstream): `scatter_mesh.py` fails that row by name and exits 1; it never exports an empty GLB.
5. Instances on the lagoon side: nothing below the waterline except lilies and reeds (`habitat` decides; reeds stand in ≤ 0.3 m of water, lilies float).

---

### Task 1: `flora.csv` — the species the city grows, with habitat and season

**Files:**
- Create: `db/canon/flora.csv`, `db/schema/flora.md`
- Test: `tools/tests/test_art_flora.py`

**Interfaces — Produces:** `flora.csv` columns `id,name,group,habitats,seasons,withers,meshes,per_100m2,scale_min,scale_max,tag,source_ref` where
- `group` ∈ {tree, shrub, grass, water, flower, crop, prop}
- `habitats` `;`-list ∈ {shore, water, yard, garden, street_edge, open, wall_foot, roof, precinct, tombs, camp}
- `seasons` `;`-list of `seasons.csv` ids, or `all`
- `withers` 1 when the drought browns it (leaves), else 0
- `meshes` `;`-list of mesh ids from `art/scatter_meshes.csv` (Task 2)

Rows (tag `A` for the plants, `INVENTED` for the numbers; `source_ref` cites world-art-plan §7/§5):

```
id,name,group,habitats,seasons,withers,meshes,per_100m2,scale_min,scale_max,tag,source_ref
date_palm,Date palm,tree,yard;garden;shore;street_edge,all,1,palm_tall;palm_short;palm_bend;palm_detailed,0.35,0.85,1.25,A,world-art-plan §7 (trees); INVENTED density
palm_offshoot,Date-palm offshoot,shrub,garden;yard,all,1,palm_short,0.25,0.35,0.55,A,world-art-plan §7 (seedling offshoots)
dead_palm,Dead palm trunk,tree,yard;tombs,all,0,palm_dead,0.04,0.8,1.1,A,world-art-plan §7 (dead and dry)
tamarisk,Tamarisk,shrub,shore;yard;tombs,all,1,tamarisk,0.25,0.8,1.3,A,world-art-plan §7 (trees)
pomegranate,Pomegranate bush,shrub,garden,all,1,bush_large;bush,0.4,0.8,1.1,A,world-art-plan §7 (gardens)
phragmites,Giant reed,water,shore;water,all,1,reed_clump,6.0,0.8,1.4,A,world-art-plan §7 (water and marsh)
cattail,Cattail,water,shore;water,all,1,cattail,2.5,0.8,1.2,A,world-art-plan §7 (water and marsh)
water_lily,Water lily,water,water,rains;sowing;harvest,0,lily_large;lily_small;kk_waterlily_a;kk_waterlily_b,3.0,0.8,1.3,A,world-art-plan §7 (quiet water)
desert_grass,Desert grass,grass,wall_foot;tombs;camp;street_edge,all,1,grass;grass_large;grass_leafs,3.0,0.7,1.2,A,world-art-plan §7 (wild and desert)
saltbush,Saltbush,shrub,tombs;camp;wall_foot,all,1,saltbush;bush_small,1.0,0.7,1.2,A,world-art-plan §7 (wild and desert)
poppy,Spring poppy,flower,tombs;camp;wall_foot;garden,rains;sowing,0,flower_red_a;flower_red_b;flower_red_c,2.0,0.8,1.2,A,world-art-plan §7 (spring wildflowers)
anemone,Spring anemone,flower,tombs;garden,rains;sowing,0,flower_purple_a;flower_yellow_a,1.0,0.8,1.2,A,world-art-plan §7 (spring wildflowers)
stones,Loose stones,prop,wall_foot;street_edge;tombs;camp,all,0,stone_small_a;stone_small_b;rock_small_a;kk_rock_c,1.5,0.6,1.4,A,world-art-plan §6 (scattered stones)
jars,Storage jars,prop,yard;street_edge,all,0,pot_large;pot_small,0.6,0.9,1.2,A,world-art-plan §5 (household)
sacks,Grain sacks,prop,yard;open,all,0,kk_sack,0.4,0.9,1.1,A,world-art-plan §5 (food as objects)
water_buckets,Leather buckets,prop,yard;shore,all,0,kk_bucket_water,0.2,0.9,1.1,A,world-art-plan §5 (household)
roof_ladder,Roof ladder,prop,yard,all,0,kk_ladder,0.1,1.0,1.0,A,world-art-plan §5; wb (sleeping on the roof)
spear_rack,Spear rack,prop,camp,all,0,kk_weaponrack,0.2,1.0,1.0,A,world-art-plan §5
log_pile,Palm-log pile,prop,yard;open,all,0,log;log_stack,0.15,0.8,1.1,A,world-art-plan §5 (fuel)
```

- [ ] Step 1: write `tools/tests/test_art_flora.py`: every row's `habitats` ⊆ the habitat set; `seasons` = `all` or ⊆ `seasons.csv` ids; `withers` ∈ {0,1}; `per_100m2 > 0`; `scale_min ≤ scale_max`; every `meshes` id exists in `art/scatter_meshes.csv` (import it with `csv`); no mesh id comes from the §10 banned list (`BANNED = ("barrel","crate","cactus","pine","oak","mushroom","windmill","watermill","chicken")` — substring test on the source file name).
- [ ] Step 2: `python3 -m unittest tools/tests/test_art_flora.py` → FAIL (no table).
- [ ] Step 3: write the table and `db/schema/flora.md` (one paragraph per column, like `db/schema/places.md`).
- [ ] Step 4: PASS after Task 2's mesh table exists (write Task 2's CSV in this task's commit if needed); `python3 tools/canon_lint.py` → exit 0; `python3 tools/stage_canon_for_ue.py` (stages `flora.csv` into `unreal/Content/Sim/canon/`), `--check` → 0.
- [ ] Step 5: commit.

### Task 2: the stylizer and the code-built plants

**Files:**
- Create: `art/scatter_meshes.csv`, `tools/art/scatter_mesh.py` (Blender), `tools/art/flora_gen.py` (Blender, imported by `scatter_mesh.py`), `tools/art/check_scatter.py` (Blender gate)
- Modify: `art/assets.csv` via `manifest.upsert` (one row per exported mesh: licence of its source; `LicenseRef-Original` for code-built)
- Output: `art/generated/scatter/SM_F_<id>.glb` (gitignored), `art/review/scatter_contact.png` (tracked)

**Interfaces:**
- `art/scatter_meshes.csv` columns `id,source,file,leafy,max_tris` — `source` is an `art/assets.csv` id (e.g. `kenney_nature_kit`, `kaykit_medieval_hexagon`) or `code`; `file` the path under `art/source/<source>/` (or the `flora_gen` function name for `code`); `leafy` 1 when green parts should wither.
- The allowed rows (and only these):

```
id,source,file,leafy,max_tris
palm_tall,kenney_nature_kit,Models/GLTF format/tree_palmTall.glb,1,1500
palm_short,kenney_nature_kit,Models/GLTF format/tree_palmShort.glb,1,1500
palm_bend,kenney_nature_kit,Models/GLTF format/tree_palmBend.glb,1,1500
palm_detailed,kenney_nature_kit,Models/GLTF format/tree_palmDetailedTall.glb,1,1500
bush,kenney_nature_kit,Models/GLTF format/plant_bush.glb,1,800
bush_large,kenney_nature_kit,Models/GLTF format/plant_bushLarge.glb,1,800
bush_small,kenney_nature_kit,Models/GLTF format/plant_bushSmall.glb,1,800
grass,kenney_nature_kit,Models/GLTF format/grass.glb,1,300
grass_large,kenney_nature_kit,Models/GLTF format/grass_large.glb,1,300
grass_leafs,kenney_nature_kit,Models/GLTF format/grass_leafs.glb,1,300
lily_large,kenney_nature_kit,Models/GLTF format/lily_large.glb,0,300
lily_small,kenney_nature_kit,Models/GLTF format/lily_small.glb,0,300
flower_red_a,kenney_nature_kit,Models/GLTF format/flower_redA.glb,0,300
flower_red_b,kenney_nature_kit,Models/GLTF format/flower_redB.glb,0,300
flower_red_c,kenney_nature_kit,Models/GLTF format/flower_redC.glb,0,300
flower_purple_a,kenney_nature_kit,Models/GLTF format/flower_purpleA.glb,0,300
flower_yellow_a,kenney_nature_kit,Models/GLTF format/flower_yellowA.glb,0,300
stone_small_a,kenney_nature_kit,Models/GLTF format/stone_smallA.glb,0,300
stone_small_b,kenney_nature_kit,Models/GLTF format/stone_smallB.glb,0,300
rock_small_a,kenney_nature_kit,Models/GLTF format/rock_smallA.glb,0,300
pot_large,kenney_nature_kit,Models/GLTF format/pot_large.glb,0,600
pot_small,kenney_nature_kit,Models/GLTF format/pot_small.glb,0,600
log,kenney_nature_kit,Models/GLTF format/log.glb,0,600
log_stack,kenney_nature_kit,Models/GLTF format/log_stack.glb,0,600
kk_sack,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/props/sack.gltf,0,600
kk_bucket_water,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/props/bucket_water.gltf,0,600
kk_ladder,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/props/ladder.gltf,0,600
kk_weaponrack,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/props/weaponrack.gltf,0,600
kk_rock_c,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/nature/rock_single_C.gltf,0,600
kk_waterlily_a,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/nature/waterlily_A.gltf,0,300
kk_waterlily_b,kaykit_medieval_hexagon,addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/nature/waterlily_B.gltf,0,300
reed_clump,code,reed_clump,1,1200
cattail,code,cattail,1,800
tamarisk,code,tamarisk,1,1500
saltbush,code,saltbush,1,800
palm_dead,code,palm_dead,0,600
```

  (Before writing it, `find art/source/kaykit_medieval_hexagon -name "sack.gltf"` etc. — confirm each path; fix the path, never the id.)
- `flora_gen.<name>() -> bpy.types.Object` for `reed_clump` (14–20 tapered blades 1.8–3.2 m, a few feathery plumes, seeded), `cattail` (8–12 blades + 3–5 brown spikes), `tamarisk` (a short trunk + 5–7 feathery grey-green lobed crowns), `saltbush` (a low dome of 3 grey-green lumps), `palm_dead` (a leaning grey trunk, 3 broken frond stubs). Colours are palette names from `art/palette.csv` (look them up by name; never hard-code RGB).
- `scatter_mesh.py`: per row → import (`bpy.ops.import_scene.gltf`), join to one object, apply transforms, set origin to the bottom centre, scale so the source's metres stay metres, recolour: for each face take its material's base colour (or image average for textured KayKit materials — `image.pixels` mean), snap to the nearest palette colour (`stylize.load_palette()` + `stylize.quantize(arr, palette)` from `tools/art/stylize.py` on a 1×1 array; Blender's bundled Python has numpy but not Pillow — import only those two functions, or copy the 10-line nearest-colour loop if the Pillow import at the top of `stylize.py` fails inside Blender), write it to a CORNER float colour attribute `Col`; A = 1 on faces whose snapped colour is green-dominant (g > r and g > b) when `leafy`, else 0; drop materials, one slot `M_Scatter`; flat shading; export `SM_F_<id>.glb` with `export_vertex_color="ACTIVE"`, `export_materials="PLACEHOLDER"`. Upsert the manifest row.
- `check_scatter.py`: exits 1 when a GLB is missing, over `max_tris`, has zero faces, or its bottom is not at z ≈ 0 (±2 cm).

- [ ] Step 1: run `check_scatter.py` on an empty dir → FAIL (missing).
- [ ] Step 2: implement `flora_gen.py` + `scatter_mesh.py`; a missing source file prints `FAIL <id>: missing <path>` and the run exits 1 (Review Focus 4 — test it by pointing one row at a bad path in a temp copy of the CSV via `--meshes`).
- [ ] Step 3: `nice -n 19 $BLENDER -b --factory-startup -P tools/art/scatter_mesh.py -- --sheet` → all rows exported; `check_scatter.py` → pass; `python3 tools/art/license_gate.py --write-credits && python3 tools/art/license_gate.py` → 0.
- [ ] Step 4: view `art/review/scatter_contact.png` (Cycles CPU, like `building_mesh.contact_sheet` — reuse it): palms read as date palms (not coconut-green: the palette snaps them to dusty greens), no medieval-European look.
- [ ] Step 5: commit (`art/scatter_meshes.csv`, the scripts, the contact sheet, `art/assets.csv`, `docs/credits.md`).

### Task 3: `scatter.py` — where everything stands

**Files:**
- Create: `tools/art/scatter.py`, `tools/tests/test_art_scatter.py`
- Output (tracked): `unreal/Content/Sim/scatter.csv`
- Modify: `.github/workflows/ci.yml` (`python3 tools/art/scatter.py --check` in the art step)

**Interfaces — Produces:**
- `scatter.csv` columns `mesh,x_cm,y_cm,yaw_deg,scale,withers,seasons` (UE frame: cm, +X east, +Y south — the same numbers `places.csv` × 100), sorted by `(mesh, x_cm, y_cm)`.
- `scatter.door_point(place) -> (x_m, y_m)`: the door slot in world metres — local `(door_x(w), ±(d/2 + 1))` rotated by `yaw_deg` and offset by the centre (same as `KitDoorLocal` + `CurrentFrame`; `door_x` from `building_grammar`).
- `scatter.habitat_points(habitat, frame, places, rng) -> list[(x_m, y_m)]` candidate generators:
  - `shore`: the lagoon edge ring `r ∈ [r0 − 3, r0 + 2]` over the crescent's angles (`a0..a1` from `crescent_frame`), `water`: `r ∈ [r0 − 12, r0 − 3]` (inside the lagoon; lilies and reeds only).
  - `yard`: behind each building (the side away from its door), within 3 m of the back wall; `garden`: the same, only for `comfortable`/`elite`/`sacred` places, denser.
  - `street_edge`: against the front wall, 0.6–1.2 m out, ≥ 2.5 m from the door.
  - `wall_foot`: `r ∈ [r1 − 4, r1 − 1]`; `open`: inside open-ground footprints (market square, brickyard, wharf) — props only; `precinct`: around the ziggurat's temenos (`ziggurat` place); `tombs`: inside the `garden_of_tombs` quarter's footprints' surroundings; `camp`: around `migrant_tent`/`warchief_tent` places.
- `scatter.build(canon_dir) -> list[dict]` — for each flora row × habitat: candidates → keep with probability from `per_100m2` over the habitat's area → reject by the exclusion rules → emit one row per instance with a mesh chosen by the rng, `yaw` uniform, `scale` uniform in range.
- Exclusion (one function, `blocked(x, y, places, frame) -> bool`): inside any building footprint grown by 0.5 m (reuse `city_layout.corners` / point-in-polygon); within 2.5 m of any `door_point`; in the lagoon except for `water`-habitat rows; within 2 m of the gates' passages (`city_gate`, `sea_gate`).
- CLI: `python3 tools/art/scatter.py [--check]` — writes (or compares and exits 1 when stale, printing the first differing line).

- [ ] Step 1: tests — deterministic (two builds → identical rows); no instance inside a grown footprint; none within 2.5 m of any door point; no non-`water` instance inside `r < r0 − 1` (the lagoon); every row's mesh ∈ `scatter_meshes.csv`; total ≤ 40,000; each habitat in `flora.csv` yields > 0 instances; `door_point` for `caravan_yard_place` equals the value computed by hand from its row (write the number in the test); adding a fake place to a copied `places.csv` in a temp canon dir makes the old output stale (`--check` logic returns non-equal) (Review Focus 1).
- [ ] Step 2: FAIL; Step 3: implement; Step 4: PASS; generate; `--check` → 0; CI step added.
- [ ] Step 5: plot it — extend `tools/plot_city.py` with `--scatter` (dots coloured by group over the plan) → `art/review/scatter_plan.png`; eyeball: palms in yards and gardens, reeds ringing the lagoon, streets and doors clear. Commit.

### Task 4: UE import and `M_Scatter`

**Files:**
- Create: `tools/art/ue_import_scatter.py`
- Output: `unreal/Content/Art/Scatter/SM_F_*.uasset`, `M_Scatter.uasset`, `MPC_World.uasset` (LFS)

**Interfaces — Produces:**
- `MPC_World` (Material Parameter Collection) scalars `Wither` (0..1) and `Bloom` (0..1).
- `M_Scatter`: BaseColor = `lerp(VertexColor.rgb, StrawColour, MPC_World.Wither × VertexColor.a)` with `StrawColour` = the palette's `reed_2` entry (dry straw; the palette names are numbered families — `mud_0..7`, `reed_0..4`, `leaf_0..2`… see `art/palette.csv`); Roughness 0.9; **two-sided**; World Position Offset wind = `sin(Time × 1.7 + WorldPosition.x × 0.01) × 3 cm × saturate(LocalZ / 300 cm)` on leafy verts only (× VertexColor.a) — keep it off stones and props.
- Each mesh: Interchange import (the batch-1 staging pattern in `ue_import_buildings.py`), slot 0 = `M_Scatter`; collision: palms and props `BlockAll` simple box (auto convex off, one box), grass/flowers/lilies/reeds none.

- [ ] Step 1: write the script (reuse `import_file`, the staging/rename loop and the material helper style from `ue_import_buildings.py` — set every `ComponentMask` channel explicitly, the batch-1 lesson).
- [ ] Step 2: run it headless (`UnrealEditor-Cmd ... -run=pythonscript -script="$PWD/tools/art/ue_import_scatter.py"`); the log file `unreal/Saved/Logs/SchizoGame.log` says `ue_import_scatter: N/N meshes`.
- [ ] Step 3: a real-RHI check that the materials compile: one `-game -RenderOffscreen` shot run (iGPU on Linux) and `grep "Failed to compile"` → nothing. Commit (LFS).

### Task 5: `ASimScatter` — the city wears it, withering and flowering with the kernel

**Files:**
- Create: `unreal/Source/SchizoGame/Public/SimScatter.h`, `Private/SimScatter.cpp`, `Private/Tests/SimScatterTest.cpp`
- Modify: `SimGameMode.cpp` (build the scatter after the street), `SimPropsBuilder.cpp` (skip the doorstep dressing for a place whose `ASimStreetBuilder::TryBuildingMesh` is non-null — the grammar's trade markers replace it; batch 1's deferred minor)

**Interfaces — Produces:**
- `static FSimScatterResult ASimScatter::BuildScatter(UWorld*)` → `{ int32 NumInstances; int32 NumMeshes; int32 NumMissingMeshes; }` — reads `Content/Sim/scatter.csv` (`SimCityData::SplitCsv`), one `UHierarchicalInstancedStaticMeshComponent` per (mesh, seasons) pair, instance z = `ASimEnvironment::TerrainHeight(x, y)` (water rows: the water height — `SimEnvironment.cpp` has `constexpr float WaterZ = -120.f` in its anonymous namespace; expose it as `static float ASimEnvironment::WaterHeight()` and use that).
- Missing mesh asset → count it, skip its rows, one summary `UE_LOG` line total (Review Focus 2).
- Cull distances per group: grass/flowers 6000 cm, shrubs/props 15000 cm, trees none.
- `void ASimScatter::ApplyWorldState(int32 DroughtStage, const FString& Season)`: sets `MPC_World.Wither = clamp(DroughtStage / 4, 0, 1)`; sets visibility of the HISMs whose `seasons` column does not include `Season` to hidden. Called once after build and whenever the sim day changes (poll `USimWorldSubsystem::GetSimDayFor` in `Tick`, 1 Hz).

- [ ] Step 1: failing test `Sim.Scatter` (pattern: `SimStreetBuildingsTest.cpp` — a fresh game world, `BuildQuarter` then `BuildScatter`): instance count == scatter.csv rows whose mesh exists; no instance within 250 cm (2D) of any `GetAllDoorSlots` location; `ApplyWorldState(0, "rains")` → Wither 0 and poppies visible; `ApplyWorldState(4, "harvest")` → Wither 1 and poppies hidden; with `bUseScatterMeshes = false` (a static test switch like `bUseBuildingMeshes`) → 0 instances and no crash.
- [ ] Step 2: `tools/ue_test.sh Sim.Scatter` → FAIL (no class). Step 3: implement. Step 4: PASS; `tools/ue_test.sh` all `Sim.*` green; `tools/ue_smoke.sh` all ok (the menu boot needs 30 smooth frames — watch it; a first boot after import builds the DDC once).
- [ ] Step 5: shots (`-SimShotViews=aerial,street,harbor,gate,zig`, iGPU on Linux) at 12.00 and 17.50: the street has palms, shade, clutter; the lagoon a reed ring; doors clear. Then `sim.Drought 4` + a shot: leaves browned. Copy the chosen shots to `art/review/crescent/vb2_*.png`. Commit.

### Task 6: records and push

- [ ] `tools/art/README.md` (a "Scatter" section: the commands of Tasks 2–4), `docs/notes-for-tommy.md` (the new actor, `MPC_World`, how to regenerate), `HANDOFF.md` ("Stage V batch 2"), `docs/proposals/invented-ledger-flora.md` (densities, habitats, the allowed pack list and why each banned item is out), `docs/completion-plan.md` progress line. Fresh-context review of the batch (Review Focus above) → fix pass → push → CI green.

## Status (2026-09-26): all 6 tasks done (on Tommy's Windows box; not yet pushed)

| Task | Commits | Verified by |
|---|---|---|
| 0 Windows fixes | ce966d8, ac8d2c0 | `sources.py` tree hashes equal the designer's manifest on Windows |
| 1 flora.csv | ed56f40 | `test_art_flora.py`, `canon_lint`, `stage_canon_for_ue --check`, `mechanics_registry --check` |
| 2 scatter mesher | 560651b | `check_scatter.py` 36/36, `license_gate.py`, `art/review/scatter_contact.png`; a missing source fails its row and exits 1 (Review Focus 4) |
| 3 scatter.py | 29d9584 | `test_art_scatter.py` 8/8 (Review Focus 1, 5), `scatter.py --check` in CI, `art/review/scatter_plan.png` |
| 4 UE import | 2416148 | `ue_import_scatter: 36/36`; a rendered run has no `Failed to compile` |
| 5 ASimScatter | 7ad8519 | `Sim.Scatter` (Review Focus 2, 3), `ue_test.sh` 66/66, `ue_smoke.sh` all ok, shots `art/review/crescent/vb2_*.png` (RTX 3060) |
| 6 records | (this) | HANDOFF, notes for Tommy, `invented-ledger-flora.md`, completion-plan progress, `tools/art/README.md` "Scatter" |

Machine: Windows 11, RTX 3060, UE 5.8.3 (launcher build), **Blender 4.5** (the plan asks for 5.2 LTS; every script ran on 4.5), Python 3.12. The review was done inline against the Review Focus list (each item has a test or a shot), not by a separate fresh-context agent.

**Rulings made during execution** (each: what — why — cost if wrong):
- T0: `sources.py` writes `/` paths and skips `.git/` with either separator — on Windows the manifest got `\` paths and the tree hash took in git's internals — none (the hashes now match Linux).
- T0: `.superpowers/` added to `.gitignore` — the roadmap says the ledger is gitignored; it was not — none.
- T1: `flora` is `LORE` in `mechanics_registry.py` — art data, no mechanic reads it (like `terrain_schemes`) — a mechanic row later if flora gains gameplay (foraging, dates to pick).
- T2: `size_m` column in `scatter_meshes.csv` (the real longest side) — the packs are toy scale (a Kenney palm is 1.4 m, a KayKit sack 16 cm); "source metres" would give dollhouse props — sizes are judgement; tune in the csv.
- T2: Kenney's named materials map to the palette by hand (`MATERIAL_MAP`) — their cartoon colours (mint leaves, 112,229,214) snap to glaze blue by plain nearest colour — a new Kenney material falls back to the nearest snap.
- T2: KayKit faces sample the atlas at each face's UV centre, not the image mean — KayKit packs every colour in one atlas — none.
- T2: KayKit lilies are pinned to `leaf_0` (`ROW_COLOUR`) — their atlas green snaps to lagoon water — none.
- T2: `building_mesh.py` loads `trim_cells.json` only if it exists — importing it for `contact_sheet()` failed on a clone without the atlas — none.
- T3: yard and garden densities raised (date palm 0.35 → 3.0 per 100 m2, jars 0.6 → 2.5, offshoot, pomegranate, sacks, buckets, ladders, logs, dead palm) — at the plan's numbers the 2.5 m yard strips gave one palm per ~11 houses — more draw calls (still 1,053 instances, far under 40,000).
- T3: rooted water plants only where the lagoon is ≤ 0.3 m deep (r ≥ r0 − 4.5 m on `SimEnvironment`'s slope); lilies float anywhere in the water band — Review Focus 5 — a reed ring narrower than the shore band.
- T3: the gate passage is the gate's local |x| < 4.5 m, |y| < d/2 + 8 m — the plan gives no passage width — a bare apron at each gate.
- T3: open ground (market square, brickyard, wharf...) takes only `open`-habitat props; every other footprint is solid — the plan's "inside any building footprint" would have kept props out of the market — none.
- T4: `MPC_World` is reused if it exists, not recreated — `M_Scatter` references it, so a second import could not delete it — none.
- T4: collision via `EditorStaticMeshLibrary` when `StaticMeshEditorSubsystem` is missing (the headless commandlet) — deprecated API — a later UE may drop it; the subsystem path is tried first.
- T4: the material-compile check ran with Task 5's shots — nothing references `M_Scatter` until `ASimScatter` places it — none.
- T5: `M_Scatter` connects VertexColor's RGB pin by its real name `""` and decodes it with a 2.2 power; its connect helper now raises on a failed link — `"RGB"` silently connected nothing (black palms), and the mesh build stores vertex colour sRGB-encoded (`StaticMeshBuilder.cpp`, `ToFColor(true)`) — none.
- T5: lilies float on `ASimEnvironment::WaterHeight()`; reeds root on the terrain (every mesh id with `lily` floats) — `scatter.csv` has no habitat column — a new floating mesh must have `lily` in its id.
- T5: `ASimScatter` follows the drought as well as the day (1 Hz) — `sim.Drought` changes it mid-day — none.
- T5: `-SimShotDrought=<stage>` sets the kernel's drought before a shot tour — an `-ExecCmds` `sim.Drought` runs before the kernel world exists and does nothing — none.
- T5: `ue_test.sh`/`ue_smoke.sh` pick `Win64/*.exe` and add `-stdout -FullStdOutLogOutput` when `UE_ROOT` is a Windows install — the roadmap says to run them in Git Bash, but they hard-coded Linux — none.

**Deferred minors:**
- `ue_import_buildings.py` (batch 1) has the same `"RGB"` pin bug: `M_Building`'s tint and grime never reach the buildings (the multiply's B stays 1), and its vertex colour also needs the sRGB decode. Fixing it changes every building's look — do it on purpose, with shots.
- The scatter palms read darker than `SimEnvironment`'s hand-tinted palms (`leaf_1` is a dark entry, and shaded faces are dark generally — batch 1's open sky-light note). Revisit with batch 5's atmosphere.
- Some Kenney faces have flipped normals (a palm's fronds, a log end); `M_Scatter` is two-sided so the game hides it; the contact sheet shows them dark.
- `palm_dead` reads thin at contact-sheet scale.
- Several repo tools open files without an encoding: on Windows run the Python tools with `PYTHONUTF8=1` (e.g. `mechanics_registry.py` rewrote an em dash otherwise).
