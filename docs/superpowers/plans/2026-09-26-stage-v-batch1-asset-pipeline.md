# Stage V batch 1 — the automated asset pipeline, living buildings, the true compass

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (native, chosen by the designer). Steps use checkbox (`- [ ]`) syntax.

**Goal:** Every building in the crescent is generated from its data row as a unique, weathered, trade-readable low-poly mesh, from textures fetched and stylized by script under a licence gate; and the world's compass matches reality (the 3D world no longer mirrors the map).

**Architecture:** Extends the existing headless art pipeline (`tools/art/`, `art/assets.csv`). Plain-Python stages (fetch → stylize → atlas → grammar → gate) are unit-tested without Blender. Blender 5.2 LTS (`~/.local/bin/blender`) turns grammar specs into GLBs; UE's `-run=pythonscript` imports them; `SimStreetBuilder` places one mesh per place and keeps the kit as fallback.

**Tech stack:** Python 3 + Pillow 10 + numpy; Blender 5.2.2 LTS headless; gltfpack 1.3; UE 5.8 Python; C++ (SchizoGame module).

**Spec:** `docs/world-art-plan.md` (§1 look, §2 materials, §3 grammar, §4 typologies, §12–13 Stage V) and `reports/Automated game asset pipeline.md` (the researched pipeline, stages 0–7).

## Global Constraints

- D-023 look: low-poly, flat-shaded, chunky; textures 128–512 px per material cell, pixel-crisp; grime in vertex colour.
- Licence allowlist: `CC0-1.0`, `CC-BY-4.0`, `LicenseRef-Original`, `LicenseRef-QAL-1.0`, `LicenseRef-Fab-Standard`, `LicenseRef-Sonniss`. Any NC / ND / SA / GPL licence hard-fails.
- Poly Haven calls send a unique User-Agent (`schizo-game-asset-pipeline/1.0 (+github.com/CodePhantom-1/schizo-game)`).
- Raw downloads live in `art/source/` (gitignored); only processed/imported output and the manifest are tracked. Binaries go through LFS.
- Every generated building keeps its door opening exactly where `SimStreetBuilder` puts the door slot.
- Determinism: same data row + seed → byte-identical spec JSON.
- DECISIONS.md is append-only; INVENTED choices go in a ledger (D-018).
- No quality cap for disk (designer, 2026-09-26): full-resolution sources are fine; the low-poly theme is the only limit.

## Review Focus

1. A place whose footprint is 1–2 m in a dimension (street shrine, booth): the grammar must still produce a closed, valid mesh with a door, not a degenerate one.
2. A building type added to `building_types.csv` with no grammar preset: must fall back to a generic home of its wealth tier, never crash the batch.
3. A fetched asset whose API licence field is missing or unexpected: the gate must fail it, not default to CC0.
4. UE with the building meshes not imported (Tommy's fresh clone before running the import): the street must fall back to the kit and still register every door slot.
5. The compass flip must not move any gameplay point (door slots, gate, spawn): only sky, far scenery and labels change.

---

### Task 1: The true compass (+Y is south)

UE is left-handed: with +X east and +Y north the player sees the mirror of the map. Keep all coordinates; declare **north = −Y**. Then flip the few things that encode a compass direction.

**Files:**
- Create: `unreal/Source/SchizoGame/Public/SimCompass.h`
- Modify: `unreal/Source/SchizoGame/Private/SimDayNight.cpp` (BodyDir), `SimEnvironment.cpp` (desert/steppe/mountains/mesas, comments), `tools/plot_city.py` (north up = −Y up), `db/canon/city_districts.csv` crescent_frame character text, `docs/city-of-the-moon.md` wording, `DECISIONS.md` (append D-026)
- Test: `unreal/Source/SchizoGame/Private/Tests/SimCompassTest.cpp`

**Interfaces — Produces:** `SimCompass::North` (FVector(0,-1,0)), `SimCompass::East` (FVector(1,0,0)), `SimCompass::SunDir(float Phase, float TiltDeg)` (rises +X, culminates toward south = +Y).

- [ ] Step 1: failing test `Sim.Compass` — sunrise dir X>0.9; noon dir Y>0 (south) and Z>0; `North ^ East` handedness check: from above, turning from North to East is clockwise (i.e. `FVector::CrossProduct(East, North).Z > 0` in UE's left-handed frame → verify by computing and asserting the known value).
- [ ] Step 2: run `tools/ue_test.sh Sim.Compass` → FAIL (header missing).
- [ ] Step 3: implement header; `SimDayNight` uses `SimCompass::SunDir`; environment desert term uses `+Y` for south, steppe `−Y` north, mountains at `Y < 0`, mesas west+south; comments say "north (−Y)".
- [ ] Step 4: test PASS; `tools/ue_smoke.sh` PASS; offscreen shots `aerial,zig,street` show noon sun on the south (+Y) side.
- [ ] Step 5: plot_city flips Y; docs and D-026; commit.

### Task 2: Manifest v2, licence gate, credits

**Files:**
- Create: `tools/art/manifest.py`, `tools/art/license_gate.py`, `tools/tests/test_art_manifest.py`, `docs/credits.md` (generated)
- Modify: `art/assets.csv` (new columns), `.github/workflows/ci.yml` (gate + tests)

**Interfaces — Produces:**
- `manifest.COLUMNS = ["id","file","kind","license","author","url","acquired","sha256","ai","period","source_ref","tag"]`
- `manifest.load(path) -> list[dict]`, `manifest.upsert(path, row) -> None` (keyed by id, sorted, idempotent), `manifest.sha256(path) -> str`
- `license_gate.check(rows) -> list[str]` (problems; empty = pass), `license_gate.credits_md(rows) -> str`
- CLI: `python3 tools/art/license_gate.py [--write-credits]` exits 1 on problems or stale credits.

- [ ] Step 1: tests — allowlisted ids pass; `CC-BY-NC-4.0`, `CC-BY-SA-4.0`, `GPL-3.0`, empty licence each produce a problem naming the id; CC-BY row without author fails; `ai=true` without `source_ref` naming the tool fails; `upsert` twice with the same row leaves one row; credits list CC-BY rows as "Title by Author (URL), CC BY 4.0" and include a QAL section.
- [ ] Step 2: run `python3 -m unittest tools/tests/test_art_manifest.py` → FAIL.
- [ ] Step 3: implement; migrate the 61 existing rows (`original (...)` → `LicenseRef-Original`, CC0 textures/characters → `CC0-1.0` with author/url from the fetchers).
- [ ] Step 4: tests PASS; `license_gate.py --write-credits` then `license_gate.py` → exit 0.
- [ ] Step 5: CI steps; commit.

### Task 3: Source fetchers

**Files:**
- Create: `tools/art/sources.py` (adapters), `tools/art/fetch_assets.py` (CLI), `art/sources.csv` (the wishlist), `tools/tests/test_art_sources.py`, `tools/tests/fixtures/art/*.json`
- Modify: `.gitignore` (nothing new: `art/source/` sub-dirs already ignored per fetcher — add `art/source/*/` blanket)

**Interfaces — Produces:**
- `sources.polyhaven_files(files_json, res="1k") -> dict[map_name, url]` (maps: diffuse, nor_gl, rough, ao, disp; prefers jpg)
- `sources.ambientcg_download(asset_json, res="1K-JPG") -> (url, filename)`
- `sources.kenney_zip_url(html) -> str`
- `sources.fetch(row, dest_root) -> list[manifest row]` — dispatch by `row["source"] in {polyhaven, ambientcg, kenney, git}`; writes `art/source/<id>/...`, returns manifest rows with licence, author, url, acquired (today), sha256.
- `art/sources.csv` columns: `id,source,asset,kind,license,purpose` (license = expected SPDX; the fetcher fails if the API disagrees).

- [ ] Step 1: tests with saved API fixtures (captured once with curl into `tools/tests/fixtures/art/`): polyhaven picks the 1k jpg diffuse + normal + rough; ambientcg picks the 1K-JPG zip; kenney parses the hashed zip href; an expected-licence mismatch raises.
- [ ] Step 2: FAIL; Step 3: implement (urllib only, UA header, retries 3, skip when sha file present); Step 4: PASS.
- [ ] Step 5: wishlist — Poly Haven: `clay_block_wall`, `clay_plaster`, `mud_cracked_dry_riverbed_002`, `reed_roof_04`, `thatch_roof_angled`, `palm_bark`, `sandstone_brick_wall`, `dry_ground_rocks`, `aerial_beach_01`, `rock_face`; ambientCG: `Bricks100`, `Ground087`, `Ground109`, `Wicker010A`, `Bark012`, `Rock035` (already used), `Fabric` madder/indigo weave, `Planks`; Kenney: `nature-kit`; git: KayKit `KayKit-Game-Assets/KayKit-Medieval-Hexagon-Pack-1.0`. Run the fetch for real; manifest rows land; commit (no binaries).

### Task 4: Palette, stylizer, the trim atlas with wear states

**Files:**
- Create: `art/palette.csv` (name,r,g,b,use — ~40 colours: mud, straw, plaster, lime, fired brick, bitumen, reed, palm, cedar, lapis, gold glaze, madder, indigo, saffron, cream, goat-black, copper, patina, basalt, limestone…), `tools/art/stylize.py`, `tools/art/trim_atlas.py`, `tools/tests/test_art_atlas.py`
- Output (generated, gitignored): `art/generated/tex/T_Trim.png` (2048², 8×8 cells of 256 px), `art/generated/tex/trim_cells.json`; review: `art/review/trim_atlas.png` (a 512 px preview, tracked).

**Interfaces — Produces:**
- `stylize.lock(img, palette, size, blur=0.6) -> Image` (blur → BOX downscale → quantize to palette, no dither → RGB)
- `trim_atlas.MATERIALS = ["mudbrick","plaster","whitewash","fired_brick","bitumen","reed","palm","cedar","cone_mosaic","glazed_lapis","ochre_band","packed_earth","stone","textile_madder","textile_indigo","goat_hair"]` and `WEAR = ["fresh","worn","crumbling","cracked"]` → 64 cells, row-major
- `trim_atlas.cell(material, wear) -> (u0, v0, u1, v1)`; `trim_cells.json` = `{"grid": 8, "cells": {"mudbrick/fresh": [col,row], ...}}`
- Every cell tiles seamlessly (edges wrap: left column ≈ right column after the wrap filter).

- [ ] Step 1: tests — every pixel of a stylized image is in the palette; 64 cells all present and unique; `cell()` of the last cell is (0.875,0.875,1,1); a cell is seamless (mean abs diff between its column 0 and column 255 after the wrap step < the mean diff of two random columns); same inputs → identical PNG bytes.
- [ ] Step 2: FAIL; Step 3: implement — CC0 photo cells (mudbrick ← clay_block_wall/Bricks100, plaster ← clay_plaster, cracked ← mud_cracked, reed ← reed_roof, palm ← palm_bark, stone ← sandstone/Rock035, packed_earth ← Ground109, cedar ← Planks) and procedural cells (cone mosaic zigzag/lozenge, glazed lapis bricks with gold band, ochre band, whitewash, textiles, goat-hair stripes, bitumen); wear = procedural overlays (worn: flaking plaster showing courses + runnels; crumbling: eroded base, holes; cracked: salt bloom + crack network), all then palette-locked.
- [ ] Step 4: PASS; generate; eyeball `art/review/trim_atlas.png`; commit.

### Task 5: `door_side` in the data, and the building grammar

**Files:**
- Modify: `tools/city_layout.py` (write `door_side` = `+y`/`-y` per the StreetBuilder rule), `db/canon/places.csv`, `db/canon/schema` doc for places, `unreal/.../SimCityData.h/.cpp` (parse `door_side`), `SimStreetBuilder.cpp` (use it; delete its RowSplit rule), `tools/tests/test_city_layout.py`
- Create: `tools/art/building_grammar.py`, `tools/tests/test_building_grammar.py`

**Interfaces — Produces:**
- places.csv column `door_side` ∈ {`+y`,`-y`} (local frame of the footprint).
- `building_grammar.spec(place: dict, btype: dict, cells: dict) -> dict` with `{"id", "w", "d", "door": {"side","x","width","height"}, "parts": [{"shape":"box"|"prism"|"wedge", "at":[x,y,z], "size":[sx,sy,sz], "yaw", "mat":"mudbrick/worn", "grime":{"base":0..1,"soot":0..1,"hand":0..1}, "jitter":float}], "tris_est": int}` — metres, origin at footprint centre, +z up.
- `building_grammar.door_x(w) = -w/2 + (floor(w)//2 + 0.5)` (matches `BuildRoom`).
- `building_grammar.all_specs(places, btypes, cells) -> list[dict]` (skips open ground, stalls, monuments, beyond_the_gate — same sets as the builder); `python3 tools/art/building_grammar.py --out art/generated/buildings/specs.json`.
- The grammar (world-art-plan §3): plinth by wealth (none/earth/fired brick+bitumen/stone); wall finish by look + wealth (plaster, whitewash, cone mosaic band, ochre bands, recessed-panel buttresses for sacred/elite); wear by age = hash(id) and wealth (poor → crumbling more often); lean 0.5–2° and course wander; roof: palm-beam ends projecting, parapet crenel variety, rooftop reed shelter / drying rack / ladder by typology; openings: lintel type by wealth, high slits, a lattice for elite; trade additions from `building_types.look` keywords (oven/tannur, kiln dome, furnace+chimney+soot, looms frame, vats, awning+counter, pens+reed fence, silo domes, tablet shelves, spear racks, shrine niche, drying racks, bales, jars) — every workshop/trade typology gets ≥1 exterior marker so it reads without a sign.

- [ ] Step 1: tests — `door_side` equals the old C++ rule for every current place (compute the rule in the test from the frame); spec deterministic; 20 home places → 20 distinct part signatures; every part lies inside footprint ±1.5 m (additions may spill to the street side by ≤1.5 m) and above z ≥ 0; the wall on the door side has a gap covering `door.x ± width/2`; each non-home typology gets a part tagged `marker`; a 1×1 m and a 2×2 m footprint give valid specs; an unknown typology falls back to its wealth tier's home; `tris_est ≤ 3000` (elite ≤ 6000).
- [ ] Step 2: FAIL; Step 3: implement; Step 4: PASS; `city_layout.py --check` PASS; C++ builds; `Sim.` tests PASS; commit.

### Task 6: The Blender mesher, vertex grime, contact sheet

**Files:**
- Create: `tools/art/building_mesh.py` (Blender), `tools/art/check_buildings.py` (Blender gate)
- Output: `art/generated/buildings/SM_B_<place_id>.glb`; `art/review/buildings_contact.png` (tracked)

**Interfaces:**
- Consumes: `specs.json` (Task 5), `trim_cells.json` (Task 4).
- Produces GLBs where: UV0 = planar metres / tile (1 m brick, 2 m plaster…); UV1 = the cell's (col,row) as a constant per face (the material wraps UV0 inside the cell); vertex colour RGB = grime (base damp darkening, soot above ovens/furnace/door for smiths, grubby band 0.8–1.4 m on whitewash, AO-ish darkening in reentrant corners), A = wear weight for the drought blend; flat shading; jitter applied per vertex (seeded).
- `check_buildings.py` exits 1 if any GLB is missing, non-manifold beyond openings, over budget, or has its door opening outside `door.x ± 0.1 m`.

- [ ] Step 1: run the check on an empty dir → FAIL (missing). Step 2: implement mesher; run over all specs (`nice -n 19 blender -b --factory-startup -P tools/art/building_mesh.py -- --specs ... --out ...`). Step 3: check PASS. Step 4: contact sheet of 24 buildings (Workbench, vertex colour × atlas texture preview); view it; commit.

### Task 7: UE import and the street wearing it

**Files:**
- Create: `tools/art/ue_import_buildings.py`
- Modify: `SimStreetBuilder.h/.cpp` (per-place mesh with kit fallback), `unreal/Source/SchizoGame/Private/Tests/` (new `Sim.StreetBuildings` test), `tools/art/README.md`
- Output: `unreal/Content/Art/Buildings/SM_B_*.uasset`, `T_Trim.uasset`, `M_Building.uasset` (LFS)

**Interfaces:**
- `M_Building`: BaseColor = `TextureSample(T_Trim, (frac(UV0) + UV1) / 8)` × vertex RGB, lerp toward the cracked row by `DroughtWear` (scalar param, default 0) × vertex A; Roughness 0.95; nearest-ish filter (`TF_Nearest` off; mips on per the research).
- `ASimStreetBuilder::TryBuildingMesh(FName PlaceId) -> UStaticMesh*` (LoadObject `/Game/Art/Buildings/SM_B_<id>`); when found, one `UStaticMeshComponent` at `CurrentFrame`, collision `BlockAll` complex-as-simple; door slot from `door_side` + the same `door_x` formula.

- [ ] Step 1: failing UE test `Sim.StreetBuildings`: after `Build()`, every non-open place with a mesh asset has a component named `Bld_<id>` and its door slot equals the kit path's slot (compute both). Step 2: FAIL. Step 3: import script run (`UnrealEditor-Cmd ... -run=pythonscript -script=tools/art/ue_import_buildings.py`), C++ swap. Step 4: `ue_test.sh Sim` PASS, smoke PASS, offscreen shots (aerial, street, gate, harbor, zig) reviewed — walls read as mudbrick/plaster with wear, trades distinct. Step 5: commit (LFS).

### Task 8: Records

- [ ] `tools/art/README.md` (the pipeline stages and commands), `docs/notes-for-tommy.md` (new pipeline, compass, re-run commands), `HANDOFF.md` (Stage V batch 1), `docs/proposals/invented-ledger-buildings.md` (grammar choices, palette, wear rules), `docs/completion-plan.md` (V1/V2/V3 progress), memory. Commit.

Then: fresh-reviewer bug review of the whole batch → fix pass → push → CI green.

**Next batches (not this plan):** B2 flora/fauna/props from Kenney/KayKit/Quaternius + stylize meshes + HISM scatter CSV; B3 terrain splat/relief from numpy + Poly Haven ground sets; B4 sound (Freesound CC0/BY + Sonniss) and the heritage reference board (Met/Cleveland); B5 local 2D AI (sd.cpp `--circular`) for decals.

---

## Status (2026-09-26): all 8 tasks done and pushed

| Task | Commits | Verified by |
|---|---|---|
| 1 compass | 11b37ed..ad8d2af | `ue_test.sh` 64/64, smoke |
| 2 manifest + licence gate | ..67c8e28 | `test_art_*`, `license_gate.py` in CI |
| 3 fetchers | ..e0cb73b | 20 sources fetched, manifested |
| 4 palette + atlas | ..a0c2951 | `art/review/trim_atlas.png` |
| 5 door_side + grammar | ..8d32b72 | `test_art_building_grammar.py` 13/13 (in CI) |
| 6 mesher + gate | ..41b06dd | `check_buildings.py` 94/94, `art/review/buildings_contact.png` |
| 7 UE import + street | ..d75428c | `Sim.StreetBuildings`, `ue_test.sh` 65/65, `ue_smoke.sh` all ok, in-game shots `art/review/crescent/vb1_*.png` (iGPU) |
| 8 records | bc31aef.. | HANDOFF, notes for Tommy, `docs/proposals/invented-ledger-buildings.md` |

**Rulings made during execution** (each: what — why — cost if wrong):
- T3: the fetch CLI lives in `sources.py` (no `fetch_assets.py`) — one file per job — a rename.
- T5: the plot test's bounds were too loose for yawed parts and domes; fixed the test, not the grammar — a marker may spill < 0.3 m more.
- T5: test renamed `test_art_building_grammar.py` so CI runs it — none.
- T6: reed halls are a C-section vault with a door-wide gap, tents a propped eave, the archive's sealed-door panel dropped, reed platforms 0.4 m — every door slot opens into a walkable room like the kit's (UE MaxStepHeight 0.45 m) — the reed halls look split at the door.
- T6: manifold is proven per part in `building_mesh.py`, not on the GLB (parts overlap by design) — none found.
- T6: vertex RGB stored at half (M_Building x2) so tints > 1 survive 8-bit colour; the hand-band grime was dropped (coarse boxes have no vertices in the band; `ponytail:` note in `grime()`).
- T6: the contact sheet renders with Cycles CPU (Workbench cannot multiply the atlas by vertex colour) — ~1 min.
- T7: `M_Building`'s component masks set on every channel (UE's defaults gave a float4 and the material fell back to default grey; only a real-RHI run shows this) — none now.

**Deferred minors:**
- `SimPropsBuilder`'s doorstep props (0.85 m beside each door) can overlap the grammar's trade markers — resolve in batch 2's scatter (it owns placement).
- The reed hall's doorway gap splits its vault visually; a short entrance porch vault would read better.
- On the Linux iGPU the street faces in shade render near-black (Tommy's palms too) — check sky light on the RTX with Lumen before tuning.

**Machine note:** any rendering UE run on the designer's Linux box must use the iGPU (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`); the RX 5700 XT wedges the kernel driver and crashed the machine on 2026-09-26.
