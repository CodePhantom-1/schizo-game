# Stage V batch 3 — the terrain: textured ground, landforms, relief and the countryside scatter

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or subagent-driven-development). Steps use checkbox (`- [ ]`) syntax. Needs batch 2 done (`MPC_World`, `ASimScatter`, `flora.csv`, `scatter_meshes.csv`). This batch edits **Tommy's** `SimEnvironment` — keep his colours, layout and style; add, don't replace.

**Goal:** The land around the crescent reads as a real lower-Mesopotamian landscape at noon and at dusk: textured ground (silt, irrigated earth, cracked mud, salt crust, sand, gravel, reed-mud, beach) under Tommy's hand-picked colours, the landforms of world-art-plan §6 (levees, strip fields with dykes, tells on the horizon, salt flats that spread with the drought), and the countryside's own scatter (date groves on levees, reeds on the banks, seasonal barley, sherds and stones on tells, desert scrub), all driven by the drought and the season.

**Architecture:** `SimEnvironment::TerrainColor` already classifies every triangle (bed, wet mud, street, beach, road, reed bank, crop, desert, far hills). Extract that classification as `ASimEnvironment::GroundKind(X, Y, Z, Nz) -> ESimGround` without changing a colour; write the kind into the terrain's vertex alpha; a new `M_Terrain` multiplies Tommy's vertex colour by a tiling **detail** texture (a luminance-normalised cell per kind from a new ground atlas, world-aligned planar UV, a triplanar blend on slopes), and swaps field and silt cells toward cracked mud and salt crust with `MPC_World.Wither`. Landforms are added as extra terms in `TerrainHeight`. The countryside scatter is procedural in C++ (`ASimScatter::ScatterCountryside`), reading the same `flora.csv` rows through habitats mapped from `GroundKind`.

**Tech stack:** Python 3 + Pillow + numpy (ground atlas); UE 5.8 Python (material); C++.

**Spec:** `docs/world-art-plan.md` §6 (terrain), §7 (flora rules), §9 (the drought, visible), §13 V5 (+ the countryside half of V6).

## Global Constraints

- Tommy's colours are the look: the detail texture's mean luminance is 0.5 per cell and `M_Terrain` does `VertexRGB × Detail × 2` — a flat grey detail must reproduce today's terrain exactly.
- No geometry change inside the walls or on the street apron (`CityDist < 4000 cm` stays flat, as now); gameplay points (door slots, gate, spawn, `SimCityData` places) do not move.
- Terrain grid budget: today's `AxisCoords` density; landforms must read at that resolution (≥ 3 grid cells wide) or go to the scatter.
- Deterministic: `SimHash01`/`Perlin` only, no `FMath::Rand`.
- Rendering on the designer's Linux box: iGPU (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`).

## Review Focus

1. `GroundKind` extraction must be colour-identical: a test samples 2,000 seeded points and compares `TerrainColor` before/after the refactor (store the "before" colours in the test as a hash, generated once from the pre-refactor build).
2. Salt flats and cracked mud spread by `Wither` only on fields and silt kinds, never on the street, beach or water bed.
3. A tell must never rise under a road, canal, river or the city apron (their masks win).
4. Crops change by season (sowing → green shoots, harvest → gold, rains/vintage → bare rows) and never grow in the desert.
5. The countryside scatter must not cover the road west from the Moon Gate (the player's route): clear 6 m either side.

---

### Task 1: the ground atlas (detail cells)

**Files:** Create `tools/art/ground_atlas.py`, `tools/tests/test_art_ground.py`; output `art/generated/tex/T_Ground.png` (4×4 cells of 512 px, gitignored), `art/generated/tex/ground_cells.json`, review `art/review/ground_atlas.png` (tracked).

**Interfaces — Produces:** `ground_atlas.KINDS = ["silt","irrigated","cracked","salt","sand","gravel","reed_mud","beach","road","street","bed","rock","hills","dune","tell","spare"]` (16 cells, row-major, index = `ESimGround` value); each cell a seamless, **luminance-normalised** detail (grey with a faint hue from its source, mean 0.5 ± 0.01, std ≈ 0.12) built from the Poly Haven sets already fetched (`ph_dry_mud_field`, `ph_brown_mud_dry`, `ph_mud_cracked_dry`, `ph_coast_sand`, `ph_dry_ground_rocks`, `ph_rock_boulder_dry`) through `stylize.wrap_blur`/`downscale`, and procedural cells for salt crust (white polygon crust over detail) and tell (sherd speckle).

- [ ] Step 1: tests — 16 cells; each cell mean ∈ [0.49, 0.51]; each tiles (reuse the batch-1 seam test from `tools/tests/test_art_atlas.py`); same inputs → identical PNG bytes; `ground_cells.json` maps every kind.
- [ ] Step 2: FAIL; Step 3: implement (copy the tiling/blur approach of `trim_atlas.py`; normalise with `cell = cell / cell.mean() * 0.5`, clip to [0,1]); Step 4: PASS; eyeball the review sheet; commit.

### Task 2: `GroundKind` and `M_Terrain`

**Files:** Modify `unreal/Source/SchizoGame/Public/SimEnvironment.h`, `Private/SimEnvironment.cpp` (`TerrainColor` → `GroundKind` + colour table; vertex alpha); create `tools/art/ue_make_terrain_material.py`, `Private/Tests/SimTerrainTest.cpp`.

**Interfaces — Produces:**
- `enum class ESimGround : uint8 { Silt, Irrigated, Cracked, Salt, Sand, Gravel, ReedMud, Beach, Road, Street, Bed, Rock, Hills, Dune, Tell, Spare }` (order = the atlas).
- `static ESimGround ASimEnvironment::GroundKind(float X, float Y, float Z, float Nz, uint32 Seed)`; `TerrainColor` becomes `switch (GroundKind(...))` over the same colours it returns today.
- Terrain vertex colour A = `(uint8)Kind / 15.0`.
- `/Game/Art/Terrain/M_Terrain`: `Cell = round(VertexColor.A × 15)`; `UV = frac(WorldPosition.xy / 400 cm)` (triplanar XZ/YZ blend by the normal on slopes > 30°); `Detail = T_Ground((UV + (Cell % 4, floor(Cell / 4))) / 4)`; `Dry = MPC_World.Wither × (Cell ∈ {Silt, Irrigated})` → lerp the cell to `Cracked`/`Salt` (salt where a low-frequency world-space noise > 0.6) and the colour toward the cracked colour; BaseColor = `VertexRGB × Detail × 2`; Roughness 0.95. Set every ComponentMask channel explicitly.
- `SimEnvironment::LoadMaterials` uses `M_Terrain` when present, else Tommy's `FlatMat` (fallback for a fresh clone).

- [ ] Step 1: failing test `Sim.Terrain.GroundKind` — 2,000 seeded points: `TerrainColor` hash equals the stored pre-refactor hash (compute it on the unmodified code first and paste it into the test — that run is Step 0); kinds of 6 hand-picked points (lagoon bed, beach, the road west, a field, the desert south, the street) equal the expected enum.
- [ ] Step 2: FAIL (no `GroundKind`); Step 3: refactor + vertex alpha + material script; run the script headless; Step 4: PASS; `ue_test.sh` all green; smoke ok; iGPU shots aerial + a new `-SimShotViews` entry `fields` (add a view over the western fields to the shot list in `SimGameMode`'s shot code if absent); commit.

### Task 3: landforms

**Files:** Modify `SimEnvironment.cpp` (`TerrainHeight` terms), `Private/Tests/SimTerrainTest.cpp`.

**Interfaces — Produces** (all inside `TerrainHeight`, after the existing terms, masked so the city apron, road, canals, river and sea win — apply each landform, then re-apply those masks):
- **Levees:** `+70 cm × (1 − smoothstep(300, 1200, |d|))` beside the river and each canal (`d` = distance to the channel edge), so palms can stand high and dry.
- **Strip fields:** in the field zone (`X ∈ (−68000, −2500)` today), low dykes (+25 cm, 60 cm wide) every 2,200 cm along X and furrows (−6 cm) every 90 cm — at the grid's resolution only the dykes read; furrows go to the material (a stripe in `M_Terrain` for Irrigated cells, 90 cm period along X).
- **Tells:** 5 mounds at seeded positions 30–90 km from the city on the landward horizon, radius 250–600 m, height 8–18 m, flat-topped (`H += Tall × smoothstep(R, 0.6R, d)`), kind `Tell`.
- **Salt pans:** no geometry; `Salt` kind where the field zone's noise > 0.75 (the material spreads it further with `Wither`).
- **Erosion gullies:** 3–6 shallow (−80 cm) meandering gullies down the dune slopes south (Perlin-warped lines).

- [ ] Step 1: tests — the city apron is still flat (sample 500 points with `CityDist < 4000` → heights unchanged vs the stored hash); the road west still ≤ 20 cm; a canal centre is still ≤ −250; each tell's centre is ≥ 800 cm above its surroundings and its kind is `Tell`; no tell within 5 km of the road or the river.
- [ ] Step 2–4: FAIL → implement → PASS; shots (aerial, fields, and one from the gate looking west at dusk); commit.

### Task 4: the countryside scatter

**Files:** Modify `SimScatter.h/.cpp` (batch 2), `db/canon/flora.csv` (new habitats), `Private/Tests/SimScatterTest.cpp`.

**Interfaces — Produces:**
- New `flora.csv` habitats: `levee`, `field`, `bank`, `desert`, `tell_top`; new rows: `barley` (crop, field, seasons sowing;harvest, meshes `crops_wheat_a` (sowing) / `crops_wheat_b` (harvest) — add both Kenney files `crops_wheatStageA/B.glb` to `scatter_meshes.csv` and run batch 2's stylizer), `bare_rows` (crop, field, seasons rains;vintage, mesh `crops_dirt_row` = `crops_dirtRow.glb`), `sherds` (prop, tell_top, a code-built `sherd_scatter` mesh in `flora_gen.py`: 12 tiny flat red-brown shards), and extend `date_palm` to `levee`, `phragmites` to `bank`, `desert_grass`/`saltbush` to `desert`.
- `void ASimScatter::ScatterCountryside(const FSimFloraTable&)`: a jittered grid (cell 400 cm) over the terrain window, `GroundKind` → habitat (`Irrigated`→field, levee band → levee, `ReedMud`→bank, `Dune`/`Sand`→desert, `Tell`→tell_top), density from `per_100m2`, `SimHash01` for every random draw; skip within 600 cm of the road west (Review Focus 5) and inside any Tommy palm's 300 cm (he places palms with `FSimMeshKit::Palm`; keep a list of their positions in `BuildCountryside` and expose it).
- Instance budget for the countryside: 60,000 (HISM, cull 250 m for crops and grass).

- [ ] Step 1: tests — instance counts per habitat > 0; nothing within 600 cm of the road centreline west of the gate; barley visible only in sowing/harvest (`ApplyWorldState`), bare rows only in rains/vintage; deterministic (two builds → same first 100 transforms).
- [ ] Step 2–4: FAIL → implement → PASS; `ue_test.sh`, smoke; shots: fields at sowing and at harvest (`sim.AdvanceDays` to change season), and `sim.Drought 4` (salt and cracked mud spreading, palms browning). Commit.

### Task 5: records and push

- [ ] `docs/notes-for-tommy.md` (what changed in his `SimEnvironment`: `GroundKind`, `M_Terrain`, the landform terms — and that his colours are untouched), HANDOFF "Stage V batch 3", `docs/proposals/invented-ledger-terrain.md` (every number above), `tools/art/README.md` (ground atlas + terrain material commands), completion-plan progress. Fresh-context review against the Review Focus → fix pass → push → CI green.

## Status (2026-09-26): all 5 tasks done (on Tommy's Windows box)

| Task | Commits | Verified by |
|---|---|---|
| 1 ground atlas | f831c8f | `test_art_ground.py` 6/6 (mean 0.5 per cell, tiling, deterministic), `art/review/ground_atlas.png` |
| 2 GroundKind + M_Terrain | b54f08e | `Sim.Terrain.GroundKind` (2,000-sample colour hash pinned from the pre-refactor build; 6 hand-picked kinds), rendered shots `vb3_*_fields.png` |
| 3 landforms | 629fa53 | `Sim.Terrain.Landforms` (apron heights pinned before the change; road ≤ 20 cm; canals ≤ −250 cm; 5 tells ≥ 8 m proud, kind Tell, 500 m + r clear of road and river) |
| 4 countryside scatter | 8c42988 | `Sim.Scatter.Countryside` (every habitat > 0, road clear 6 m, crops by season, deterministic), `ue_test.sh` 68/68, `ue_smoke.sh` all ok, shots `vb3_sowing/harvest/drought4_*` |
| 5 records | (this) | HANDOFF, notes for Tommy, `invented-ledger-terrain.md`, completion-plan, `tools/art/README.md` "Terrain", roadmap |

The review was done inline against the Review Focus list (each item has a test or a shot), not by a separate fresh-context agent.

**Rulings made during execution** (each: what — why — cost if wrong):
- T2: `GroundKind` is a separate classifier that mirrors `TerrainColor`'s regions; `TerrainColor`'s code is untouched (not rewritten as a switch) — colour identity by construction, and Task 3's new kinds (Salt, Tell) change no colour — two branch trees to keep in step (the colour-hash test catches colour drift, not kind drift).
- T2: `M_Terrain` keeps Tommy's `M_Flat` roughness 0.92 / specular 0.25 (not the plan's 0.95) and his world noise — a flat detail must reproduce today's terrain exactly — none.
- T2: `M_Flat`'s "blocky" noise was never blocky: its snapped position goes to a Noise pin named `Position`, which does not exist (it is `World Position`), so the noise reads the raw world position; `M_Terrain` reproduces what renders — Tommy may want the blocky look switched on (every flat surface changes).
- T2: `T_Ground` imports as linear (not sRGB) so a 0.5 cell stays 0.5 and x 2 leaves the colour — none.
- T2: every material link in the new scripts is checked (a missing pin raises): it caught UE's If pins (`A > B`, `A == B`, `A < B`) and the Noise pin (`World Position`) — none.
- T3: tells 3–4.5 km out (the plan says 30–90 km) and 500 m + radius clear of road and river (plan: 5 km) — the terrain window is 15 km and its grid 15 m near the city growing 8%/step (~216 m cells at 3 km); 30–90 km is off the map — tells sit nearer than planned.
- T3: each tell stands on a plateau at the mean base height of its foot, then rises its full height — on the northern steppe the dune ridges swing ±7 m and a tell on a trough read lower than its surroundings — the mound's foot can show a small step on a ridge.
- T3: no field dykes in the geometry (60 cm dykes on a 15 m grid would alias into random bumps; the global constraint sends sub-3-cell features elsewhere); the furrows are in `M_Terrain` — the fields read flat from the side.
- T3: levees are +70 cm over ~45 m (not ~9 m) so they read at the grid — softer banks.
- T3: landforms go before the apron, lagoon, road, canal, river and sea terms (not after them with the masks re-applied) — the same result with no duplicated masks — none.
- T4: barley is two rows, `barley_shoots` (sowing) and `barley_ripe` (harvest) — the plan's one row wanted a different mesh per season — none.
- T4: countryside density multipliers against `per_100m2` (the table's numbers are the city's): desert × 0.2, levee × 0.5, tell top × 0.05 — at table density the dunes and the 250–600 m tells blew the 60,000 budget (it truncated spatially) — tune in `HabitatDensity`.
- T4: whole-row colours for Kenney's wheat (shoots `leaf_1`, ripe `reed_3`; its ripe `_defaultMat` read as terracotta); `dirtDark` → `mud_2` — none.
- T4: `ue_import_scatter.py` now only adds missing `MPC_World` parameters — re-setting the list gave them new ids and broke `M_Terrain` (default material in game) — none.
- T4: `-SimQuitAfter=<s>` (core ticker, runs while the menu pauses the world) ends the Windows smoke menu run cleanly; `-SimShotAdvanceDays=<n>` shoots other seasons — none.

**Deferred minors:**
- Kenney's `crops_wheatStageA` is 720 triangles; ~7,500 barley instances under a 250 m cull is heavy — an LOD or a lighter code-built barley in batch 7 (performance).
- The levees and gullies are subtle at the terrain's resolution; the tells are far out of the standard shot views (add a `tells` shot view if wanted).
- The countryside grid covers x −900..600 m, y −600..700 m plus the tells; beyond that the land is Tommy's hand-placed palms and tufts only.
- Blender 4.5 (the plan asks for 5.2) and Windows: run the Python tools with `PYTHONUTF8=1`; several write CRLF on Windows (normalise before committing).
- One rendered run stalled at engine start ("Presizing…") for 20 minutes and was killed; the next ran normally.
