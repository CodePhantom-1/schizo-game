# Mudbrick house kit (T8)

Procedural, code-built modular kit for the City of the Moon street, run
headless through Blender. No Blender GUI, no UE import — see the hard rules
in the coordinator's brief (an Unreal editor is running; nothing here
touches `unreal/`).

Blender: install a portable LTS build (no sudo) once —

```
mkdir -p ~/opt/blender
curl -o ~/opt/blender/blender.tar.xz https://download.blender.org/release/Blender4.5/blender-4.5.14-linux-x64.tar.xz
tar xf ~/opt/blender/blender.tar.xz -C ~/opt/blender --strip-components=1
~/opt/blender/blender --version   # sanity check
```

## Commands (run from the repo root)

```
~/opt/blender/blender -b --factory-startup -P tools/art/house_kit.py     -- --out art/generated
~/opt/blender/blender -b --factory-startup -P tools/art/assemble_house.py -- --out art/generated
~/opt/blender/blender -b --factory-startup -P tools/art/thumbs.py         -- --out art/generated
~/opt/blender/blender -b --factory-startup -P tools/art/check_kit.py      -- --out art/generated
```

Run in that order on a clean `art/generated/` (gitignored — regenerate any
time). `check_kit.py` exits non-zero if anything fails and is the gate.
Prefix with `nice -n 19` to keep CPU use modest, per the hard rules (CPU
Workbench rendering only, no GPU renderer).

## What's in the kit

Grid module = 1.0 m = 100 UE units (Blender stays in metres; FBX/GLB export
with Blender's own unit metadata so UE imports at the correct cm scale —
no manual scale math). Wall height 2.6 m, wall thickness 0.4 m, plinth
(stone/baked-brick footing course) 0.15 m — real-world Mesopotamian
mudbrick house proportions `[A]`, not canon-specified (see
`db/canon/buildings.csv` — only named landmark buildings are canon; ordinary
house construction follows general practice, e.g. Woolley's Ur excavations
and standard treatments of Mesopotamian vernacular building).

11 pieces (`tools/art/house_kit.py`), triangle budgets in parentheses:

| Piece | Budget | Notes |
|---|---|---|
| SM_WallPlain | 250 | 1×0.4×2.6 m |
| SM_WallDoor | 500 | door cut through, full height |
| SM_WallWindow | 500 | small high clerestory-style window `[A]` |
| SM_Corner | 350 | L-shaped, offered as a decorative/reinforcing corner option (the four wall runs alone already close a room without it) |
| SM_RoofSlab | 450 | flat roof tile with a parapet lip on one edge; `build_roof_slab(with_parapet=False)` variant used for interior roof tiles |
| SM_RoofAccess | 500 | roof tile with a square ladder-access hole |
| SM_Pilaster | 220 | engaged buttress |
| SM_Stair | 700 | straight solid mudbrick stair, 10 steps, rises one storey over 2 grid modules |
| SM_Lintel | 220 | timber door lintel |
| SM_CourtyardTile | 150 | floor tile |
| SM_Awning | 450 | reed mat on two timber poles |

Named material slots: `M_MudPlaster`, `M_Mudbrick`, `M_Timber`, `M_Reed`
(flat colours, no textures — see `docs/art-pipeline-research.md` for the
follow-up plan to add ambientCG PBR materials). UV0 = texturing unwrap
(`smart_project`), UV1 = a second, more-padded non-overlapping projection
used as the lightmap UV (`ponytail:` this reuses `smart_project`'s
non-overlapping-island property instead of the dedicated `lightmap_pack`
operator, which needs a live 3D-view context that doesn't exist in
background mode — upgrade to `lightmap_pack` if baked lighting seams show
up in-engine).

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

- Flat colours only, no PBR textures — next step is ambientCG materials
  per `docs/art-pipeline-research.md`.
- UV1 is a second `smart_project`, not a true `lightmap_pack` layout
  (background-mode limitation, noted above).
- The corner piece's parapet-facing logic in `build_flat_roof` gives a
  roof corner tile a parapet on only one of its two outer edges (the
  roof_slab piece only has one parapet edge) — fine for this kit's scale,
  flagged as a `ponytail:` simplification in the code.
- No door/window variety beyond one door + one window per wall run
  (deliberately centred on the run's middle tile, not one per tile).
