# Stage V batch 6 — the props library: every item visible, the life of the street, the reference board

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or subagent-driven-development). Steps use checkbox (`- [ ]`) syntax. Needs batch 2 (`scatter_meshes.csv`, the stylizer, `M_Scatter`, `ASimScatter`). Runs with Stage C (items/trade) — coordinate with whoever owns `items.csv`.

**Goal:** Every row of `items.csv` has a world mesh, so what a shop sells is what you see on its counter; the household, religious, transport and "life" props of world-art-plan §5 exist (300+ in total counting variants), placed by owner trade and hour (laundry out in the morning, lamps lit at dusk, bread gone by noon); and the V0 reference board of real Mesopotamian objects exists as the style anchor.

**Architecture:** A code-built generator `tools/art/props_gen_v2.py` (Blender; the batch-1 mesher's closed-solid primitives `box/prism/dome/hull/extrude_x` from `building_mesh.py`, moved to `tools/art/solids.py` so both import them) builds each prop from a **recipe row** in `art/props.csv` (shape list, palette colour names, size) — the same "data row → mesh" idea as the building grammar. Item meshes are keyed by `items.csv` ids. Placement rides on batch 2's scatter with two new habitat kinds: `counter` (on the stall/shop counter parts the grammar emits — `building_grammar.py` gains a `slots` list per spec: counter tops, doorstep, roof) and a time window per row (`hours`), toggled by `ASimScatter::ApplyWorldState` from the sim hour.

**Tech stack:** Blender 5.2 headless; Python 3; UE 5.8 Python; C++.

**Spec:** `docs/world-art-plan.md` §5 (props and clutter), §10, §13 V0 and V4; `db/canon/items.csv` (66 rows: weapons, stations, armour, staples, ritual goods, tribute, textiles, metals, medicine…).

## Global Constraints

- Every prop ≤ 600 tris (stations ≤ 1,500), palette colours by name from `art/palette.csv` (numbered families: `mud_0..7`, `brick_0..3`, `reed_0..4`, `leaf_0..2`, `lapis_0..3`…), vertex colour only, `M_Scatter`.
- §10: no coins (silver is weighed — rings and coils and scale pans), no glass, iron only as a rare curiosity (one `iron_curio` item at most), no barrels/crates.
- Licences: code-built props are `LicenseRef-Original`; museum reference images for the board only from CC0 open-access collections (The Met Open Access, Cleveland Museum of Art Open Access) — recorded in `art/assets.csv` with the object's accession number; never used as textures in game unless CC0 and palette-locked.
- Rendering on the designer's Linux box: iGPU (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`).

## Review Focus

1. An `items.csv` row added later without a recipe: the test fails naming the item (every item needs a mesh), CI red until it has one.
2. Time windows wrap midnight correctly (lamps 18–06); bread "gone by noon" means hidden 12–05, back at dawn.
3. Counter props never float or sink: their z comes from the counter part's top (`slots`), not the terrain.
4. The shop shows what it sells: the stall's props come from the items its owner's trade produces (`items.csv` `producer` column ↔ `work_roles.csv`), not random.
5. A prop recipe with an unknown palette name: the generator fails that row by name, exits 1.

---

### Task 1: shared solids and the prop recipe format

**Files:** Create `tools/art/solids.py` (move `box, ring_solid, circle, prism, dome, extrude_x, vault, gable, hull, solid` out of `building_mesh.py`; `building_mesh.py` imports them — no behaviour change: `check_buildings.py` 94/94 and a byte-compare of 3 GLBs before/after), `art/props.csv`, `tools/tests/test_art_props.py`.
**Interfaces — Produces:** `art/props.csv` columns `id,item_id,group,parts,hours,habitats,per_place,tag,source_ref` where `parts` is a `|`-separated list of `shape:sx,sy,sz@x,y,z:palette_name` (e.g. a storage jar: `prism:0.22,0.12,0.55@0,0,0:brick_1|dome:0.22,0.22,0.12@0,0,0.5:brick_1|prism:0.06,0.07,0.08@0,0,0.62:brick_1`); `item_id` empty for non-item props.
- [ ] Tests: every `items.csv` id has ≥ 1 row; every palette name exists; every shape is a `solids.solid` shape; §10 banned words (`coin`, `glass`, `barrel`, `crate`) absent from ids. FAIL → write the move + the first 20 rows → the rest in Task 2 → commit.

### Task 2: the 300 props

**Files:** Create `tools/art/props_gen_v2.py`; extend `art/props.csv`; output `art/generated/props/SM_P_<id>.glb`, `art/review/props_contact_<group>.png` (one sheet per group, tracked).
**The list** (write every row; numbers in the recipe are yours, invented, ledgered):
- **Items (66):** one per `items.csv` row by category — weapons (spear, sickle-sword, axe, mace, bow, quiver, sling, shield types), armour (leather cap, copper helmet, scale coat, felt cloak), stations (loom, potter's wheel, kiln, forge, oil press, quern), staples (barley sack, date basket, beer jar, oil jar, fish string, onion string), ritual goods (incense stand, libation cup, figurine, foundation peg), tribute (bundled reeds, bitumen cakes, copper ingots), textiles (folded wool, dyed bolts), metals (silver coil, copper ingot, tin lump), medicine (herb bundle, salve pot), texts (tablet, tablet basket, cylinder seal).
- **Household (60+):** jars plain/spouted/ribbed (3 sizes each), cooking pot, bowls, cups, clay lamp, shell lamp, brazier, reed mat (rolled, flat), rope bed, stool, chest, spindle, broom, water-skin, grinding stone, oven-front, hearth ring.
- **Religious (25):** altar, offering table, Tell-Asmar worshipper statue (3 sizes, big inlaid eyes as a dark-disc part), incense stand, Nanna's crescent standard, Utu's rayed disc, Inanna's ring-post, divine emblem banners.
- **Transport (15):** donkey pack saddle, solid-wheel cart (2 sizes), sledge, reed boat, quffa (round coracle), oars, punting poles.
- **Life (40+):** laundry line with cloths (3), children's toys (clay rattle, cart toy, knucklebones), a sleeping dog pose is batch 4's — here a dog bowl; dung cakes drying on a wall; bricks drying in rows; refuse heap; a sleeping-mat roll; the Royal Game of Ur board on a doorstep; water troughs; baskets of every size; a fish-drying rack.
- [ ] Steps: implement the generator (parse `parts`, build with `solids`, palette by name, flat shading, bottom at z=0, export); the generator fails an unknown palette name by row id (Review Focus 5); a `check_props.py` gate (present, ≤ budget, z=0) → contact sheets per group, viewed → commit.

### Task 3: counters, doorsteps and the hours

**Files:** Modify `tools/art/building_grammar.py` (emit `slots`: `{"kind": "counter"|"doorstep"|"roof", "at": [x,y,z], "size": [sx,sy]}` for every awning counter, bench, jeweller counter, the doorstep 0.6–1.2 m out beside the door, and the roof centre), `tools/tests/test_art_building_grammar.py` (slots exist for every trade with a counter; doorstep slots ≥ 1.2 m from the door centre), `tools/art/scatter.py` (a slot pass: props whose `habitats` include `counter`/`doorstep`/`roof` go on the place's slots, chosen from the items its owner's trade produces — `items.csv producer` ∈ the place's `work_roles`/typology mapping; write `z_cm` for slot rows, empty for ground rows), `unreal/Content/Sim/scatter.csv` (new `z_cm`, `hours` columns), `SimScatter.cpp` (use `z_cm` when present; hide by `hours`).
- [ ] Tests (Python): counter props' z equals the counter top; a bakery's counter holds bread/grain items only; lamps' hours `18-06` visible at 23 and 02, hidden at 12. (C++ `Sim.Scatter.Hours`): the HISM for `hours=06-12` rows is visible at 08, hidden at 14. FAIL → implement → PASS → shots of the market at 09.00 and 20.00 → commit.

### Task 4: the reference board (V0)

**Files:** Create `art/references.csv` (`id,museum,accession,title,url,license,what_it_anchors`), `tools/art/reference_board.py` (fetch the CC0 images via the museums' open-access APIs — The Met: `https://collectionapi.metmuseum.org/public/collection/v1/objects/<id>` → `primaryImageSmall` when `isPublicDomain`; Cleveland: `https://openaccess-api.clevelandart.org/api/artworks/<id>` → `images.web.url` when `share_license_status == "CC0"` — refuse anything else), output `art/review/reference_board.png` (a captioned grid) and `docs/reference-board.md` (each image's anchor: "cone mosaic → cone_mosaic cell", "Tell Asmar statues → worshipper statue", "Standard of Ur → carts, kunga, banquet props", "seals → zebu, lions, the hero"…).
- [ ] ≥ 24 objects; the fetcher checks the licence field per object and skips non-CC0 with a message; `license_gate.py` accepts the rows (`CC0-1.0`). Commit (images are review-only, gitignored raw; the board PNG tracked).

### Task 5: records and push

- [ ] HANDOFF "Stage V batch 6", notes for Tommy (the new generator, how items get meshes), `docs/proposals/invented-ledger-props.md`, README, completion-plan progress (V0, V4). Fresh-context review → fix pass → push → CI green.
