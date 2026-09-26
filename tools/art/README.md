# The asset pipeline (Stage V batch 1): generated buildings

Every building in the crescent is generated from its `places.csv` row: a unique, weathered,
trade-readable low-poly mesh on one trim atlas. The kit below stays as the fallback (a fresh clone
before the import, or a place with no generated mesh). Run from the repo root, in order:

```
python3 tools/art/sources.py                        # 0. fetch art/sources.csv (Poly Haven, ambientCG, Kenney, KayKit) -> art/source/ (gitignored)
python3 tools/art/license_gate.py --write-credits   # 1. licence gate (CI runs it) + docs/credits.md
python3 tools/art/trim_atlas.py                     # 2. palette-locked trim atlas: 16 materials x 4 wear -> art/generated/tex/T_Trim.png
python3 tools/art/building_grammar.py               # 3. places.csv -> art/generated/buildings/specs.json (the recipe per building)
nice -n 19 ~/.local/bin/blender -b --factory-startup -P tools/art/building_mesh.py -- [--sheet]   # 4. GLB per building (+ art/review/buildings_contact.png)
~/.local/bin/blender -b --factory-startup -P tools/art/check_buildings.py      # 5. gate: every GLB present, in budget, doorway open
~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" \
    -run=pythonscript -script="$PWD/tools/art/ue_import_buildings.py"           # 6. /Game/Art/Buildings: SM_B_<place>, T_Trim, M_Building
```

- **Palette** `art/palette.csv` (64 colours); `stylize.py` locks every texture to it (blur, box
  downscale, no-dither quantize) so photos and procedural cells read as one hand.
- **Atlas** 8x8 cells of 256 px, row-major `MATERIALS x WEAR` (`trim_cells.json`); photo cells from
  the CC0 sources, procedural ones for cone mosaic, glazed lapis, ochre bands, textiles, goat hair.
- **Grammar** (`building_grammar.py`, tested in `tools/tests/test_art_building_grammar.py`): plinth
  and wall finish by wealth, wear by a hash of the id and wealth, palm-beam ends, parapets, roof
  shelters, lintels, windows, and a trade marker for every non-home (ovens, kilns, chimneys and
  soot, looms, dye vats, racks, jars, bales, boats...). Reed halls are gapped barrel vaults, tents
  a goat-hair ridge propped at the door. The door is where `SimStreetBuilder` puts the door slot,
  and the doorway is walkable (tested: nothing stands in it above a 0.45 m step).
- **Mesh contract** (`building_mesh.py`): UV0 metres / tile, UV1 = the atlas cell, vertex RGB =
  tint x grime at half (M_Building x2), A = drought wear. `M_Building`'s `DroughtWear` scalar
  (default 0) blends every earthen wall toward its cracked cell.
- **In game:** `SimStreetBuilder` places `SM_B_<place_id>` (collision complex-as-simple) where it
  exists, else the kit room; `Sim.StreetBuildings` checks both paths give the same door slots. The
  first boot after an import builds the meshes into the DDC (the menu takes longer once).

## Scatter (Stage V batch 2): plants, stones and clutter

The palms, reeds, lilies, grasses, spring flowers, stones, jars and sacks of the crescent, placed
from data. Run from the repo root, in order (Windows: `py` for `python3`, Blender at
`"C:\Program Files\Blender Foundation\Blender <v>\blender.exe"`, UE at `<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`):

```
python3 tools/art/sources.py kenney_nature_kit kaykit_medieval_hexagon   # 0. the two model packs -> art/source/
blender -b --factory-startup -P tools/art/scatter_mesh.py -- [--sheet]    # 1. art/scatter_meshes.csv -> art/generated/scatter/SM_F_<id>.glb (+ art/review/scatter_contact.png)
blender -b --factory-startup -P tools/art/check_scatter.py                 # 2. gate: every GLB present, in budget, standing on z = 0
python3 tools/art/license_gate.py --write-credits                          # 3. the new SM_F_ manifest rows + docs/credits.md
python3 tools/art/scatter.py                                               # 4. db/canon (places, flora) -> unreal/Content/Sim/scatter.csv (CI: --check)
python3 tools/plot_city.py --scatter                                       # 5. art/review/scatter_plan.png: every instance over the city plan
UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" \
    -run=pythonscript -script="$PWD/tools/art/ue_import_scatter.py"         # 6. /Game/Art/Scatter: SM_F_<id>, M_Scatter, MPC_World
```

- **What grows where** is `db/canon/flora.csv` (species, habitats, seasons, withering, density,
  scale; tested in `test_art_flora.py`). `scatter.py` turns each habitat (shore, water, yard,
  garden, street_edge, open, wall_foot, precinct, tombs, camp) into candidate points and rejects
  them by one rule set: footprints grown 0.5 m, 2.5 m round every door slot, the lagoon for all but
  water plants (reeds to 0.3 m of water, lilies float), the channels, the wall, the sea, the gate
  passages. Seeded: the same canon gives the same bytes (`test_art_scatter.py`).
- **The meshes** (`art/scatter_meshes.csv`): an allow-list of Kenney Nature Kit and KayKit pieces
  (nothing from world-art-plan §10's banned list) plus five code-built plants (`flora_gen.py`:
  reed clump, cattail, tamarisk, saltbush, dead palm). `size_m` is the real longest side (the packs
  are toy scale). Colours are snapped to the palette (Kenney's named materials by hand in
  `MATERIAL_MAP`, atlas pieces sampled per face) and written as linear vertex colour; A = 1 on the
  green faces of leafy meshes.
- **In game:** `ASimScatter` (built by the game mode after the street) stands one HISM per mesh and
  season set, z on the terrain (lilies on the water), culled by group. `M_Scatter` lerps a leaf to
  straw by `MPC_World.Wither` (= drought stage / 4, from the kernel, polled each second) and sways
  leaves in the wind; out-of-season species (spring flowers, lilies) are hidden. The mesh build
  stores vertex colour sRGB-encoded, so `M_Scatter` decodes it with a 2.2 power. `Sim.Scatter` tests
  it; `-SimShotDrought=4` shoots the withered city.

# Mudbrick house kit (T8)

Procedural, code-built modular kit for the City of the Moon street, run
headless through Blender. No Blender GUI. The UE import step
(`ue_import_kit.py`, W6-B) is also headless and writes only to
`unreal/Content/Art/Kit/`.

Blender: install a portable LTS build (no sudo) once —

```
mkdir -p ~/opt/blender
curl -o ~/opt/blender/blender.tar.xz https://download.blender.org/release/Blender4.5/blender-4.5.14-linux-x64.tar.xz
tar xf ~/opt/blender/blender.tar.xz -C ~/opt/blender --strip-components=1
~/opt/blender/blender --version   # sanity check
```

## Commands (run from the repo root)

```
python3 tools/art/fetch_textures.py                                           # once per machine (CC0 maps -> art/source/, ~167 MB, gitignored)
~/opt/blender/blender -b --factory-startup -P tools/art/house_kit.py     -- --out art/generated
~/opt/blender/blender -b --factory-startup -P tools/art/assemble_house.py -- --out art/generated
~/opt/blender/blender -b --factory-startup -P tools/art/thumbs.py         -- --out art/generated
~/opt/blender/blender -b --factory-startup -P tools/art/check_kit.py      -- --out art/generated
```

Run in that order on a clean `art/generated/` (gitignored — regenerate any
time). `check_kit.py` exits non-zero if anything fails and is the gate.
Prefix with `nice -n 19` to keep CPU use modest, per the hard rules (CPU
Workbench rendering only, no GPU renderer).

## UE import (W6-B: the street consumes these meshes at runtime)

After the kit is generated, import the pieces once (needs the project
compiled; `SimStreetBuilder.cpp` falls back to engine cubes until then):

```
~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
    "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
    -script=tools/art/ue_import_kit.py
```

Imports every `mesh_kit_piece` row of `art/assets.csv` to
`/Game/Art/Kit/Meshes` WITH the CC0 surface maps embedded in the FBXs
(`import_materials`/`import_textures` on — the rebuilt per-slot materials
wear the colour maps, and get a matte roughness constant pinned on them
because the FBX cannot carry Blender's roughness wiring). It also authors
two tiled materials as real assets in `/Game/Art/Kit/Materials` via
`MaterialEditingLibrary` (idempotent — existing assets are reused):

- **M_Ground** — Ground109 colour map, TextureCoordinate tiling 30x30,
  roughness 0.95. Worn by the street's ground cube
  (`SimStreetBuilder::BuildGroundAndLighting` LoadObjects it; the flat MIC
  stays as fallback).
- **M_PlasterWall** — Ground087, tiling 8x8, roughness 0.95 — spare for
  runtime-coloured fallback surfaces (door leafs stay flat this wave).

The runtime slot colours in `SimStreetBuilder::LoadKitMeshes` are now a
FALLBACK: they are painted only onto slots whose material interface is
null (kit not imported), so the imported textured materials win.

## Characters (ART-2: the cylinders become people)

`ASimNpc`'s tinted cylinder and `ASimCharacter`'s cylinder + sphere are
replaced at runtime by a low-poly rigged cast (Quaternius "Ultimate
Animated Character Pack", CC0, flat vertex colours, ~2.5-7k tris, 23
bones, 17 embedded animations each). Two steps, both from the repo root:

```
python3 tools/art/fetch_characters.py

~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
    "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
    -script=tools/art/ue_import_characters.py
```

- `fetch_characters.py` (plain Python, mirrors `fetch_textures.py`)
  downloads the ten selected `.gltf` files (~19 MB, gitignored, per
  machine) into `art/source/characters/` and records them in
  `art/assets.csv` as `kind == character_model` rows keyed by UE asset id:
  `Player` + `Npc0..Npc8` (the cast mapping and licence live in
  `docs/proposals/invented-ledger-humans.md`). Anonymous Google Drive
  downloads are rate-limited — if the fetcher reports "Drive quota
  exceeded", wait ~an hour and re-run (it is idempotent and never writes
  a failed payload to disk).
- `ue_import_characters.py` (UE's own Python, mirrors `ue_import_kit.py`)
  imports each glTF via `AssetImportTask` (glTF routes through Interchange
  in 5.8) into a private `_staging` folder, then normalizes the layout the
  C++ side `LoadObject`s:

  | Asset | Path |
  |---|---|
  | SkeletalMesh | `/Game/Art/Characters/<id>.<id>` |
  | walk loop | `/Game/Art/Characters/<id>/Anims/Walk.Walk` |
  | idle loop | `/Game/Art/Characters/<id>/Anims/Idle.Idle` |

  Skeletons / physics assets / materials stay under `_staging` (referenced
  by object, not path — do not delete it). Idempotent: re-running deletes
  and re-imports each `<id>`.

  **Untested by the art agent** — it writes into the live project
  `Content`, so only the coordinator runs it. If the glTF import misfires
  (e.g. skeletal import through headless Interchange), the FBX fallback:
  convert with the installed Blender, then import with the
  `ue_import_kit.py` `FbxImportUI` pattern (`import_as_skeletal = True`,
  `import_animations = True`):

  ```
  ~/opt/blender/blender -b --factory-startup -P - <<'EOF'   # glTF -> FBX, one file
  import bpy, sys, os
  bpy.ops.wm.read_factory_settings(use_empty=True)
  bpy.ops.import_scene.gltf(filepath="art/source/characters/Worker_Male.gltf")
  bpy.ops.export_scene.fbx(filepath="/tmp/Worker_Male.fbx",
      add_leaf_bones=False, bake_space_transform=True,
      export_apply_scale_options='FBX_SCALE_ALL', use_metadata=False)
  EOF
  ```

  (loop over `art/source/characters/*.gltf`; then point
  `ue_import_characters.py`'s task at the FBX with an `unreal.FbxImportUI`
  options object, `import_as_skeletal`/`import_animations` True.)

The C++ side degrades gracefully: `SimNpc.cpp` / `SimCharacter.cpp` try
`LoadObject` for the skeletal mesh, then a static mesh at the same path,
and keep the tinted cylinder programmer art when neither exists — the
import can be run at any point without a code change.

## What's in the kit

Grid module = 1.0 m = 100 UE units (Blender stays in metres; FBX/GLB export
with Blender's own unit metadata so UE imports at the correct cm scale —
no manual scale math). Wall height 2.6 m, wall thickness 0.4 m, plinth
(stone/baked-brick footing course) 0.15 m — real-world Mesopotamian
mudbrick house proportions `[A]`, not canon-specified (see
`db/canon/buildings.csv` — only named landmark buildings are canon; ordinary
house construction follows general practice, e.g. Woolley's Ur excavations
and standard treatments of Mesopotamian vernacular building).

13 pieces (`tools/art/house_kit.py`), triangle budgets in parentheses:

| Piece | Budget | Notes |
|---|---|---|
| SM_WallPlain | 250 | 1×0.4×2.6 m |
| SM_WallDoor | 500 | door cut through, full height |
| SM_WallWindow | 500 | small high clerestory-style window `[A]` |
| SM_Corner | 350 | L-shaped, offered as a decorative/reinforcing corner option (the four wall runs alone already close a room without it) |
| SM_RoofSlab | 450 | flat roof tile with a parapet lip on one edge; `build_roof_slab(with_parapet=False)` variant used for interior roof tiles |
| SM_RoofSlabFlat | 50 | the parapet-free variant, exported as its own piece — the UE street builder tiles whole roofs with it and wraps the edge in SM_ParapetRun (as `assemble_house.py`'s `build_flat_roof` + `build_parapet_ring` do offline) |
| SM_ParapetRun | 50 | 1 m of continuous mudbrick parapet; instanced and X-scaled per roof side at runtime (invisible stretch with D-023 flat-colour materials) |
| SM_RoofAccess | 500 | roof tile with a square ladder-access hole |
| SM_Pilaster | 220 | engaged buttress |
| SM_Stair | 700 | straight solid mudbrick stair, 10 steps, rises one storey over 2 grid modules |
| SM_Lintel | 220 | timber door lintel |
| SM_CourtyardTile | 150 | floor tile |
| SM_Awning | 450 | reed mat on two timber poles |

Named material slots: `M_MudPlaster`, `M_Mudbrick`, `M_Timber`, `M_Reed`,
wearing the CC0 ambientCG surface maps from `fetch_textures.py`
(albedo + roughness per `tools/art/fetch_textures.py` TEX_DEFS; no normal
map — the bevelled geometry carries the relief, per D-023's low-fi lean).
UV0 = exact planar projection in metres / tile_m (`kit_common.planar_uv0`),
so texel density is uniform across every piece — a 1x2.6 m wall face samples
exactly tile_m's worth of texture (2 m for plaster/brick, 1 m for
timber/reed), and the UE-side FBX material rebuild (a bare TextureSample on
UV0, since FBX cannot carry Blender's Mapping node) sees the same density.
UV1 = a more-padded non-overlapping `smart_project` used as the lightmap UV
(`ponytail:` this reuses smart_project's non-overlapping-island property
instead of the dedicated `lightmap_pack` operator, which needs a live
3D-view context that doesn't exist in background mode — upgrade to
`lightmap_pack` if baked lighting seams show up in-engine).

## Assembly

`assemble_house.py` tiles pieces on the grid to build 3 example houses,
each exported as one joined mesh:

- **House_SmallSingleRoom** — 3×3 single room, one door, one window, flat
  roof with parapet.
- **House_TwoRoomCourtyard** — two 2×2 rooms opening onto a shared 4×2 open
  courtyard with a reed awning, matching the real courtyard-house plan `[A]`
  (rooms around an open-air yard, not a fully roofed block).
- **House_RoofAccess** — 3×3 room with an external stair to a roof-access
  hole, for roof-top living/sleeping `[A]` (standard in the hot climate).
  Also demonstrates SM_Corner and SM_Pilaster as standalone exterior
  elements.

The placement math (`place()` in `assemble_house.py`) rotates each piece
about its own corner-origin and compensates the translation so the rotated
footprint still lands exactly on its target grid cell — get this wrong and
rotated pieces swing outside their cell (this was caught and fixed during
the build: see the piece-by-piece bounding-box check in `check_kit.py`).

## Known weaknesses

- No normal maps on the kit surfaces (deliberate, D-023 lean — bevelled
  geometry carries the relief); roughness reaches UE as a pinned matte
  constant, not the map, when the importer does not wire the embedded
  roughness jpg by filename suffix.
- Runtime X-scaled instances (`SM_ParapetRun` roof runs, the gate's 2.5x
  wall pillars) now visibly stretch their texture where flat colours hid
  the stretch — acceptable at this wave's scale; world-aligned UVs would
  fix it if it reads badly.
- UV1 is a second `smart_project`, not a true `lightmap_pack` layout
  (background-mode limitation, noted above).
- The corner piece's parapet-facing logic in `build_flat_roof` gives a
  roof corner tile a parapet on only one of its two outer edges (the
  roof_slab piece only has one parapet edge) — fine for this kit's scale,
  flagged as a `ponytail:` simplification in the code.
- No door/window variety beyond one door + one window per wall run
  (deliberately centred on the run's middle tile, not one per tile).

## Street props + sky dome (dressing wave)

`props_gen.py` builds the quarter's dressing the same headless way
(kit_common helpers, CC0 maps embedded in the FBX): 10 street props
(amphora, granary jar stack, basket, rolled reed mat, bench, dates tray,
bread tray, loose loaves, unlit brazier, waterskin-on-post) and the 120 m
inverted-hemisphere `SM_SkyDome`, each under 300 tris (the script exits
non-zero if any misses its budget). Props are modelled with origin at BASE
CENTRE (not the kit's corner origin) so runtime placement yaws them about
their own centre. Origins/colours/rules are logged in
`docs/proposals/invented-ledger-dressing.md`.

```
~/opt/blender/blender -b --factory-startup -P tools/art/props_gen.py -- --out art/generated/props
```

Imports the props to `/Game/Art/Props/Meshes` and authors the sky's
gradient material `/Game/Art/Sky/M_SkyDome` (Unlit, TwoSided,
Lerp(horizon, zenith, clamp(AbsWorldPosition.Z / DomeRadius)) into
Emissive — never a SkyLight; see DECISIONS.md for the GPU-capture hang):

```
~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd \
    "$PWD/unreal/SchizoGame.uproject" -run=pythonscript \
    -script=tools/art/ue_import_props.py
```

Headless-written, not yet driven against a compiled editor — expect a
first-run shakeout (the AssetImportTask half mirrors `ue_import_kit.py`
exactly; the MaterialEditingLibrary half is new). At runtime
`SimPropsBuilder` (a tickable world subsystem, like `SimVerbSpawner`) reads
the street's place/door registries and places everything deterministically;
missing meshes are skipped silently, so the street never depends on this
import. `sim.PropsDressing 0` disables the whole pass.

