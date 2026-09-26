# Stage V batch 7 — the other cities and the performance budget (V9 + V10)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or subagent-driven-development). Steps use checkbox (`- [ ]`) syntax. Needs batches 1–6 (the grammar, scatter, terrain atlas, fauna, atmosphere, props). V9 runs with Stage Q7 (journeys) — confirm with the completion plan's owner before building vignette scenes into the main map.

**Goal:** Each city of the notes (`db/canon/cities.csv`, 9 rows) has an **identity kit** — its own palette accent, grammar preset and signature props — and a small vignette scene built from data, so journeys and the codex can show it; and the whole world meets the budget: 60 fps at 1080p medium on Tommy's RTX 3060 and on the RX 5700 XT once its driver is fixed, 30+ fps on the Linux iGPU, measured and recorded.

**Architecture:** V9 reuses every generator: a city identity is a row in a new `db/canon/city_styles.csv` (grammar overrides — wall finish weights, plinth, parapet, band colours; flora mix; signature prop ids; a palette accent family) consumed by `building_grammar.py` (`spec(place, btype, style=...)`) and `scatter.py`; a vignette is a small generated `places`-like table (`tools/city_vignette.py` → `unreal/Content/Sim/vignettes/<city>.csv`) built into a sub-level-free test area far from the crescent (a `-SimVignette=<city>` launch switch spawns it at a fixed offset and points the shot camera at it). V10 is measurement + the standard levers: HISM cull distances, LODs for the building meshes (Blender decimate at 50 % → LOD1, a box impostor → LOD2), merged far-city proxy, and `stat unit` / `stat gpu` / Unreal Insights captures recorded in `docs/performance.md`.

**Spec:** `docs/world-art-plan.md` §11 (the other cities and regions), §12 (budgets), §13 V9, V10; `db/canon/cities.csv`, `regions.csv`; `docs/completion-plan.md` U1 (the performance budget).

## Global Constraints

- The crescent's look must not change when `city_styles.csv` is added: `city_of_the_moon`'s style row reproduces today's grammar exactly (test: `specs.json` byte-identical).
- Each city's identity stays inside the canon: its `features` text in `cities.csv` is the brief (e.g. City of the Dead: "streets filled with skulls; dog and owl engravings ward houses"); INVENTED choices go in `docs/proposals/invented-ledger-cities.md`.
- Budgets (recorded, not guessed): building LOD0 ≤ 3,500 tris (6,000 elite), LOD1 ≤ 50 %, LOD2 ≤ 12 tris; draw calls in the market view ≤ 2,500; GPU frame ≤ 16.6 ms on the RTX 3060 at 1080p medium.
- Rendering on the designer's Linux box: iGPU only (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`) until the kernel/driver fix for the 5700 XT (HANDOFF "Status on this machine").

## Review Focus

1. A city whose style row is missing: it falls back to the crescent's style, never crashes the grammar.
2. LOD transitions must not pop the door opening shut at mid distance (LOD1 keeps the door gap: decimate with the doorway's vertices pinned, or LOD1 starts beyond 40 m where it no longer matters — test with `check_buildings.py` run on LOD1 too).
3. The far-city proxy must not be visible from inside the walls (switch distance ≥ 300 m from the player, never at the street).
4. Performance numbers must name the machine, the view, the time of day and the build configuration (Development vs Shipping), or they don't count.
5. A vignette must not leak into the main game (only spawned with `-SimVignette`; `ue_smoke.sh` still sees 121 places).

---

### Task 1: `city_styles.csv` and the grammar/scatter hooks

**Files:** Create `db/canon/city_styles.csv`, `db/schema/city_styles.md`; modify `tools/art/building_grammar.py` (a `style` dict with defaults = today's constants: `WEAR_ODDS`, finishes by wealth, plinth by wealth, parapet choices, band colours), `tools/art/scatter.py` (flora mix weights by style), tests in `tools/tests/test_art_building_grammar.py` (the crescent's specs byte-identical with its style row; another style changes ≥ 50 % of signatures) and `test_art_scatter.py`.
**Interfaces — Produces:** columns `id,city_id,wall_finish_weights,plinth,parapet,band,accent_palette,flora_weights,signature_props,tag,source_ref` (weights as `name:weight;…`). One row per city in `cities.csv`, each derived from its `features` text (e.g. City of the Dead: whitewash 0, ochre 0.5, `signature_props` skull niches + dog/owl engraving panels; the harbour cities: more fired brick and bitumen, reed quays).
- [ ] Tests → FAIL → implement → PASS → commit.

### Task 2: vignettes

**Files:** Create `tools/city_vignette.py` (per city: 12–25 places around a street and a monument, typologies from the city's trades in the canon; writes `unreal/Content/Sim/vignettes/<city>.csv` in the `places.csv` format + a `style` column), `unreal/Source/SchizoGame/Private/SimVignette.cpp` (+ header; builds the vignette at `(−400000 cm, 400000 cm)` with the street builder's generated-building path and the scatter), `Private/Tests/SimVignetteTest.cpp` (all vignettes load; no place overlaps; the switch is off by default → 121 places in the main build).
- [ ] Steps: generator + tests (`test_city_vignette.py`: deterministic, no overlaps, every typology known) → building meshes for every vignette place through batch 1's pipeline (`building_grammar.py --places unreal/Content/Sim/vignettes/<city>.csv`) → UE import → C++ → one iGPU shot per city (`-SimVignette=<city> -SimShots=... -SimShotViews=vignette`) → `art/review/vignettes/<city>.png` → commit.

### Task 3: LODs and the far proxy

**Files:** Modify `tools/art/building_mesh.py` (export LOD1 by Blender `Decimate` collapse 0.5 with the doorway band's vertices in a pinned vertex group; LOD2 = the footprint box ×height), `tools/art/check_buildings.py` (check LOD1's doorway too), `tools/art/ue_import_buildings.py` (import LODs into the same static mesh: `unreal.StaticMeshEditorSubsystem.set_lods_with_notification` or import the LOD GLBs and `set_lod_from_static_mesh`), `SimStreetBuilder.cpp` (LOD screen sizes 0.3 / 0.08).
- [ ] Tests: `check_buildings.py` passes on LOD0 and LOD1; tris of LOD1 ≤ 55 % of LOD0; `Sim.StreetBuildings` still green; a shot at 300 m shows no holes → commit.

### Task 4: measure and record

**Files:** Create `docs/performance.md`, `tools/perf_capture.sh` (launches `-game` with `-ExecCmds="stat unit,stat gpu"` and `-trace=cpu,gpu,frame` at 4 views × 2 times of day, collects the numbers from the log into a table; iGPU prefix on Linux).
- [ ] Run on the Linux iGPU (designer's machine) and on Tommy's RTX 3060 (Windows — adapt the script's launch line; `.bat` twin `tools/perf_capture.bat`), fill the table, list the top 3 costs per view from Insights, and fix what breaks the budget with the standard levers (cull distances, instance counts, shadow-casting off for grass/flowers, cheaper translucency for rain/dust). Record before/after. Commit.

### Task 5: records and push

- [ ] HANDOFF "Stage V batch 7", notes for Tommy (the vignette switch, the perf script, which levers were pulled), `docs/proposals/invented-ledger-cities.md`, completion-plan progress (V9, V10, U1 numbers). Fresh-context review → fix pass → push → CI green.
