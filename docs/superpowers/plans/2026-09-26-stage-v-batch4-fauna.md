# Stage V batch 4 — the fauna: herds, city animals, birds and the night's predators

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or subagent-driven-development). Steps use checkbox (`- [ ]`) syntax. Needs batches 2–3 (habitats, `GroundKind`, `ASimScatter`'s data loader, `MPC_World`). Read batch 1's Status section for machine notes.

**Goal:** A walk from the Moon Gate to the marsh meets herds (sheep, goats, cattle, donkeys) whose numbers follow the kernel's herd count, dogs and cats in the streets, geese and ducks at the lagoon, doves over the precinct, herons and flamingos in the shallows, vultures over the Garden of Tombs, and at night a jackal near the tombs — animated, reacting to the player, and period-correct (world-art-plan §8 and §10).

**Architecture:** A canon table `fauna.csv` lists species with habitat, activity hours, group size and model. Models: Quaternius animal packs first (the same source family as our humans, `LicenseRef-QAL-1.0`, already on the licence allowlist), fetched like `tools/art/fetch_characters.py` does; period variants (zebu hump, fat-tailed sheep, saluki) are Blender edits of the base meshes; birds are code-built low-poly meshes with the wing flap in `M_Scatter`-style world-position offset (no skeleton). In UE a `USimFaunaSubsystem` (tickable world subsystem, like `USimPropsBuilder`) spawns and drives the animals with a tiny state machine (idle / graze / walk / flee / sleep) on the terrain height — no navmesh — and birds as boids flocks in instanced meshes.

**Tech stack:** Python 3 (fetch, tables, tests); Blender 5.2 headless (variants, birds); UE 5.8 Python (Interchange skeletal import, like `ue_import_characters.py`); C++.

**Spec:** `docs/world-art-plan.md` §8 (the fauna, behaviour rules), §10 (no chickens; no camels in the city; horses only with the eastern peoples and the elite), §13 V7; kernel C API `sim_world_herd_head` (`kernel/include/sim/CApiWild.h`), the time of day (`USimWorldSubsystem`), `SimDayNight`.

## Global Constraints

- §10: **no chickens, no camels in the city, no horses in ordinary city use** (a horse only at the `envoys_house`/`warchief_tent` places). The test in Task 1 enforces it on the table.
- Licences: every model row in `art/assets.csv`; Quaternius is `LicenseRef-QAL-1.0` — fine for the private repo, **not** for a public one (docs/notes-for-tommy.md says so; keep saying it).
- Budget: ≤ 150 skeletal animals live at once near the player (distance-culled to 120 m, re-spawned deterministically by their habitat seed), ≤ 600 birds as instances; animation via the imported clips, no anim blueprints authored by hand (use `UAnimSequence` played with `PlayAnimation(..., bLooping)` on a `USkeletalMeshComponent`, the pattern `SimNpc` uses — check `SimNpc.cpp`).
- Determinism: spawn positions and group sizes from `SimHash01` of (species, habitat cell, sim day).
- Rendering on the designer's Linux box: iGPU (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`).

## Review Focus

1. A raid that takes the herd (the kernel's `herd_head` drops): the next day's pasture shows fewer animals — never the old count.
2. The player walking into a flock: birds lift and scatter within 1 s and resettle after ≥ 20 s away; herds never walk through walls or into the lagoon (clamp to habitat polygon; the lagoon is `r < r0`).
3. Night: diurnal animals asleep or penned (no sheep wandering the street at midnight); jackals only 20:00–05:00, only near the tombs and the camps.
4. A fresh clone before the animal import: the subsystem spawns nothing and logs one line (no crash, no per-animal warnings).
5. A species row whose model is missing: skipped with one warning naming it; the others still spawn.

---

### Task 1: `fauna.csv`

**Files:** Create `db/canon/fauna.csv`, `db/schema/fauna.md`, `tools/tests/test_art_fauna.py`.

**Interfaces — Produces:** columns `id,name,group,habitats,hours,group_min,group_max,per_place,herd_share,model,tag,source_ref`:
- `group` ∈ {herd, work, city, bird_flock, bird_wader, bird_raptor, wild_night, water, small}
- `habitats` `;`-list of batch 2/3 habitats plus place typologies prefixed `@` (e.g. `@cattle_pen;@pasture;@stable`)
- `hours` `HH-HH` active window (wraps midnight allowed, e.g. `20-05`)
- `herd_share` the fraction of the kernel's `herd_head` this species takes on the pastures (sheep 0.55, goats 0.3, cattle 0.15; others 0)
- `model` a mesh id in `art/fauna_models.csv` (Task 2)

Rows (tags `A` for species per §8, numbers `INVENTED`): `fat_tailed_sheep`, `goat`, `zebu_cattle`, `donkey` (`@stable;@caravan_yard;street_edge`, 06-19), `kunga` (only `@warchief_tent;@envoys_house`), `mastiff_dog`, `saluki` (`@envoys_house;@watch_post`), `cat` (`yard;wall_foot;@granary;@temple_stores`, 00-24), `pig` (`@yard` of poor homes only), `goose`, `duck` (`shore;water`), `dove` (`precinct;roof`), `heron`, `flamingo` (`water`), `vulture` (`tombs`, 08-17, soaring), `crow`, `sparrow` (`street_edge;open`), `jackal` (`tombs;camp`, 20-05), `frog` (sound-only row for batch 5: `model` empty, `group` small), `carp` (`water`, a jumping fish instance).

- [ ] Step 1: tests — §10 banned ids/models (`chicken`, `camel`, `horse` outside the two allowed places); every habitat known; hours parse; `herd_share` sums to 1.0 over `group == herd`; every non-empty model exists in `art/fauna_models.csv`. Step 2: FAIL. Step 3: write the table + schema. Step 4: PASS, `canon_lint.py`, `stage_canon_for_ue.py`. Commit.

### Task 2: the models

**Files:** Create `art/fauna_models.csv` (`id,source,file,anims,variant_of,tris`), `tools/art/fetch_fauna.py` (like `fetch_characters.py`: Quaternius packs by Google-Drive id, licence checked on the pack page, `manifest.upsert` rows), `tools/art/fauna_variants.py` (Blender), `tools/art/birds_gen.py` (Blender), `tools/art/ue_import_fauna.py`.

**Interfaces — Produces:**
- Step 0 of this task is a **survey**, recorded in `art/fauna_models.csv`: open Quaternius's pack pages (`https://quaternius.com/packs/ultimatedanimatedanimals.html`, `.../farmanimals.html` or their current names — the site lists "Animals" packs) and note which species exist with which clips (Idle, Walk, Run, Eat, Death, Sit/Lie). Expected coverage: sheep, cow/bull, donkey or horse, pig, dog breeds (Husky/Shiba — the dog base for mastiff/saluki), fox (jackal base), deer (gazelle base). Anything missing becomes a `variant_of` a close base, or a code-built bird.
- `fauna_variants.py`: from a base GLB, shape edits by vertex groups/proportional scaling: `zebu_cattle` (hump over the withers +12 cm, dewlap), `fat_tailed_sheep` (tail scaled ×3 in width), `saluki` (legs ×1.2, body ×0.85 girth), `jackal` (fox ×1.3, colour to the palette's `goat_1`/`stone_1`), `kunga` (donkey ×1.1, darker), recoloured to `art/palette.csv` (textures palette-locked with `stylize.lock`, then kept as texture — skeletal meshes keep their material).
- `birds_gen.py`: dove, heron, flamingo, vulture, crow, sparrow, goose, duck — each ≤ 300 tris, one mesh with wings as separate vertex group; vertex colour A = wing weight (0 body, 1 wing tip) for the flap WPO; export `SM_Bird_<id>.glb`.
- `ue_import_fauna.py`: skeletal animals to `/Game/Art/Fauna/<id>` with `Anims/Idle|Walk|Run|Eat|Sleep` renamed like `ue_import_characters.py` does; birds as static meshes with `M_Bird` (vertex colour + flap `WPO = up × sin(Time × FlapHz × 2π + InstanceRandom × 6.28) × 15 cm × VertexColor.a`, `FlapHz` from per-instance custom data 0).

- [ ] Steps: survey → fetch (licence verified) → variants + birds → Blender contact sheet `art/review/fauna_contact.png` → UE import headless → one iGPU shot of a test lineup map-free (spawn all in a row at the gate with `-SimShotViews=gate`) → commit (LFS for uassets; raw packs gitignored under `art/source/fauna_*`).

### Task 3: `USimFaunaSubsystem` — herds, city animals, night predators

**Files:** Create `Public/SimFauna.h`, `Private/SimFauna.cpp`, `Private/Tests/SimFaunaTest.cpp`; modify `SimGameMode.cpp` if the subsystem needs the street built first (it reads `SimCityData` and `ASimStreetBuilder`'s registries).

**Interfaces — Produces:**
- `struct FSimAnimal { FName Species; FVector Home; float Radius; ESimAnimalState State; float StateTime; TObjectPtr<USkeletalMeshComponent> Mesh; }`, `enum class ESimAnimalState : uint8 { Idle, Graze, Walk, Flee, Sleep }`.
- `void USimFaunaSubsystem::Populate(int32 SimDay, int32 HerdHead)` — per species × habitat: groups from `SimHash01`; herds: `round(HerdHead × herd_share / 10)` animals on the `@cattle_pen`/`@pasture` places (the `/10` is the visual scale — one visible animal per ten head, INVENTED, ledger it), clamped to the place's footprint polygon; city animals by `per_place` at their typologies.
- `Tick`: state machine at 5 Hz per animal (idle ↔ graze ↔ walk within `Radius` of `Home`, speed from the walk clip's root motion or a fixed 120 cm/s), flee from the player within 600 cm (dogs bark: a hook for batch 5 `OnAnimalCue(Species, Cue)`), sleep outside `hours`; z from `ASimEnvironment::TerrainHeight`.
- Re-populate when the sim day changes (poll `USimWorldSubsystem::GetSimDayFor` at 1 Hz) and read `sim_world_herd_head` through `USimWorldSubsystem::GetSimHandleFor` + the C API.

- [ ] Step 1: failing tests `Sim.Fauna.*` — `Populate(day, 400)` gives `round(400 × 0.55 / 10) = 22` sheep; `Populate(day, 100)` → 6; every animal stays inside its habitat polygon after 600 simulated ticks (`TickForTest(0.2f)` loop); no diurnal animal outside its penned home at 02:00; jackals exist only 20–05; missing model → species skipped, the rest spawn (use `bUseFaunaMeshes`-style test switch per species).
- [ ] Steps 2–4: FAIL → implement → PASS; `ue_test.sh`, smoke. Commit.

### Task 4: birds (boids)

**Files:** extend `SimFauna.h/.cpp`; tests in `SimFaunaTest.cpp`.

**Interfaces — Produces:** `FSimFlock { FName Species; FVector Anchor; float Radius; TArray<FVector> Pos, Vel; TObjectPtr<UInstancedStaticMeshComponent> Ism; }` — classic boids (separation 150 cm, alignment, cohesion, a soft leash to `Anchor` within `Radius`, height band per species: doves 300–1500 cm over the precinct, vultures soaring 3,000–6,000 cm in wide circles, waders standing in `water` and walking slowly, flamingos in groups of 20–40); scatter: player within 400 cm of a perched/standing bird → the flock lifts (velocity up + away) and returns after 20 s.

- [ ] Tests: positions stay within `Radius × 1.5` of the anchor over 1,000 ticks; after the player comes within 400 cm the mean height rises ≥ 200 cm within 1 s; deterministic for a fixed seed. FAIL → implement → PASS → shots at dawn over the lagoon (flamingos, herons) and at noon over the precinct (doves) → commit.

### Task 5: records and push

- [ ] `docs/notes-for-tommy.md` (the subsystem, the licence caveat, re-import commands), HANDOFF "Stage V batch 4", `docs/proposals/invented-ledger-fauna.md` (the herd scale, group sizes, hours, the variant edits), `tools/art/README.md` (fauna commands), completion-plan progress. Fresh-context review against the Review Focus → fix pass → push → CI green.

## Status (2026-09-26): all 5 tasks done (on Tommy's Windows box)

| Task | Commits | Verified by |
|---|---|---|
| 1 fauna.csv | 7cbe9a6 | `test_art_fauna.py` (§10 guard rails, habitats, hours, herd shares = 1, models), `canon_lint`, stage `--check`, `mechanics_registry --check` |
| 2 models | 63857cd | `fauna_variants.py` 10/10, `birds_gen.py` 9/9 (`art/review/birds_contact.png`), `ue_import_fauna.py` 19/19 |
| 3 ASimFauna | c0887e0 | `Sim.Fauna.Herds` (400 head -> 22/12/6; 100 -> 6 sheep), `.HomeGround` (600 steps: on its ground, out of the lagoon), `.Night` (asleep at home at 02:00; jackals 20–05 only), `.MissingModels` (one species skipped; a fresh clone spawns nothing, one line); lineup shot `vb4_12.00_lineup.png` |
| 4 birds | de4c27b | `Sim.Fauna.Birds` (1.5 x radius over 1,000 ticks; lifts >= 2 m within 1 s; resettles after 20 s; deterministic), `ue_test.sh` 73/73, `ue_smoke.sh` all ok, `vb4_*_lagoon_birds.png` |
| 5 records | (this) | HANDOFF, notes for Tommy, `invented-ledger-fauna.md`, completion-plan, `tools/art/README.md` "Fauna", roadmap |

The review was done inline against the Review Focus list (each item has a test or a shot), not by a separate fresh-context agent.

**Rulings made during execution** (each: what — why — cost if wrong):
- T2 survey: goat = a black variant of the Ultimate pack's deer, cat = a small grey fox; the fat-tailed sheep and the pig keep the Farm pack models — no goat or cat exists in the packs, and the farm sheep and pig carry only Idle and Jump clips (the deer walks) — sheep and pigs graze in place and glide when they move.
- T2: licence LicenseRef-QAL-1.0 although the packs' bundled License.txt still says CC0 — quaternius.com's licence page is QAL v1.0 since 2026-08-28 and governs what is fetched now; `fetch_fauna.py` stops if the page changes — none.
- T2: `art/fauna_models.csv` has a `size_m` column (body length) — the bases' own sizes are arbitrary (a pig 9.8 m long) — sizes are judgement.
- T2: coats by material name (dark / main / light / horn), brightness rank for unnamed `Material.00x`; anything feeding Base Color is unlinked first — the deer's materials were all 0.8 grey fed from a mix node (the goat came out white) — none.
- T2: every material forced opaque — the Farm pack's FBX materials have alpha 0 (the sheep and pig were invisible in game) — none.
- T2: each animal imports into its own folder and keeps it (skeleton, physics asset, materials, source clips); role clips are duplicated and saved explicitly — deleting the staging folder broke the meshes, and a commandlet does not save duplicates — none.
- T2: no Blender contact sheet for the skinned animals; the in-game lineup (`-SimFaunaLineup`, view `lineup`) is the review — none.
- T3: an actor (`ASimFauna`), not a world subsystem — the pattern of `ASimScatter`, testable with a fixed day, herd, hour and player — none.
- T3: the imported animals face their local +Y: a -90 deg mesh yaw (seen in the lineup shot) — none.
- T3: culling measures from the camera, not the pawn (the review cameras and a free camera see what is near them); with no player view (a test world) nothing is culled — none.
- T3: the fauna ticks every frame (the state machine's choices are still discrete) — 5 Hz steps made walking visibly jerky — a small CPU cost.
- T3: a building's animals stand 2.5 m out of its door (not in its footprint); pens, pastures and the caravan yard hold theirs inside — none.
- T4: the bird tests and every draw hash the step count, reset by `Populate` — without the reset the same day diverged — none.

**Deferred minors:**
- The farm sheep and pig have no walk or eat clip (Idle for all roles): retarget the Ultimate pack's quadruped walk onto them, or model a sheep with the full clip set.
- The fat-tailed widening and the zebu hump are subtle at game distance; stronger shape edits (or a hand-modelled hump) would read better.
- At night the animals are silhouettes (the moonless first night); batch 5's atmosphere owns the night light.
- Doves and vultures are small against the sky in the standard views; a closer `sky` shot view would show them.
- Birds do not avoid walls or roofs while flying (they fly high over the precinct and the tombs, and the roof flocks keep 3–8 m up).
