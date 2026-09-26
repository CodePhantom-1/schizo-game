# Notes for Tommy

Things you need to know before you pull and work, newest first. Delete a note once you've handled it.

## 2026-09-26

- **Animals and birds (Stage V batch 4).** `ASimFauna` (spawned after the scatter) puts herds on the pastures by the kernel's head count, donkeys, dogs, cats and pigs at their places, jackals out at night, and flocks of doves, fowl, waders and vultures that lift when you walk up. Models in `/Game/Art/Fauna`; re-import with the "Fauna" commands in `tools/art/README.md`. **Licence:** the animals are Quaternius models under their QAL (no redistribution of the files) — fine while the repo is private, not if it ever goes public.
- **Your `SimEnvironment` got terrain detail (Stage V batch 3) — your colours are untouched.** `GroundKind` classifies each terrain triangle in the same regions as your `TerrainColor` (whose code did not change; a test pins its colours) and writes the kind into vertex alpha. The terrain uses the new `/Game/Art/Terrain/M_Terrain` when it is imported (your `M_Flat` look x a tiling ground detail x 2, cracking and salting with the drought), else your `FlatMat`. `TerrainHeight` gained levees, five tells on the horizon and dune gullies, all applied before your apron, lagoon, road, canal, river and sea terms so those always win; its first part is now `BaseHeight`. Your countryside palm calls go through `PlantPalm` (it records where they stand, so the new countryside scatter keeps clear). One finding: `M_Flat`'s snapped "blocky" position is linked to a Noise pin named `Position`, which does not exist (it is `World Position`), so the noise has always read the raw world position — say if you want the blocky look switched on (it changes every flat surface).
- **The city has plants and clutter now (Stage V batch 2).** Palms, reeds, lilies, grasses, spring flowers, stones, jars and sacks stand from data: `db/canon/flora.csv` says what grows where, `tools/art/scatter.py` writes `unreal/Content/Sim/scatter.csv`, and the new actor `ASimScatter` stands them after the street. Meshes are in `/Game/Art/Scatter` (`SM_F_<id>`, material `M_Scatter`). `MPC_World` (a material parameter collection) has `Wither` (the drought, 0-1, driven by the kernel) and `Bloom` (reserved); batch 5's atmosphere will drive it too. To regenerate, run the "Scatter" commands in `tools/art/README.md`. Your `SimEnvironment` got one addition (`ASimEnvironment::WaterHeight()`); its own street palms are untouched.
- **Stage V continues from your side:** your Claude can pick it up at `docs/superpowers/plans/STAGE-V-ROADMAP.md` (batches 2–7 planned in order; batch 3 edits your `SimEnvironment` and batch 5 your `SimDayNight` — they add to your code and keep your colours and values as the defaults). Say if you'd rather own those two batches by hand.
- **The buildings are generated now (Stage V batch 1).** Every house, shop and temple is its own mesh built from its `places.csv` row (`/Game/Art/Buildings/SM_B_<place>`, one material `M_Building` on one trim atlas `T_Trim`). `SimStreetBuilder` uses them and falls back to the kit if they're missing. To regenerate after a data change, run the commands in `tools/art/README.md` (the top section). The first launch after pulling builds the new meshes once, so the menu takes longer that one time. `M_Building` has a `DroughtWear` scalar (0–1) that cracks the earthen walls; tie it to the drought when you like.
- **North is −Y now (D-026).** Only the sun, the far scenery and the map changed; no gameplay point moved.
- **Licences:** every asset has a row in `art/assets.csv`; CI fails on non-commercial or share-alike licences. The Quaternius characters are under their new licence, which is fine for our private repo but not for a public one.

0. **Your `SimEnvironment` now builds the crescent** (D-025).
   - The city's shape comes from the canon through `SimCityData` (`places.csv` footprints and the `crescent_frame` row).
   - What changed:
     - the grid `BuildCity` is gone; it now draws lit windows and street palms from the places;
     - the walls are arcs, with the citadel bulge;
     - the gate and sea gate are built in frames at their places;
     - the ziggurat has moved to the northern belly, with its temenos removed;
     - the lighthouse mole runs from the east horn;
     - there is a lagoon and channels, and the coast has moved to x ≈ 425 m.
   - Your terrain, sky, materials and the gate/ziggurat/lighthouse builders are intact.
   - Views are in `art/review/crescent/`.

1. **The game has menus now** (main menu, pause, save/load, settings, journal, tablet reader). They're C++ Slate in `unreal/Source/SchizoGame/Private/UI/`, with no UMG assets, so they build headless. The colours are in `Public/UI/SimUiStyle.h` if you want to restyle them in the D-023 look.
2. **`Config/DefaultInput.ini` is committed in the engine's own normalised form.** The editor kept rewriting it. If you add a mapping, run the editor once and commit the file as it rewrites it.
3. **The world art plan (Stage V)** is in `docs/world-art-plan.md`; there is also a readable page at https://claude.ai/artifact/H2cUHe42ZJcvxhm2APxQwd. It covers every building type and trade, the materials with their wear states, terrain relief, regional flora and fauna, and atmosphere. It builds on your environment and style, so tell us where you disagree.
4. **For screenshots:** `-SimNoMenu` skips the menu, and `-SimShotScreens=MainMenu,Settings` captures screens.

## 2026-09-25

1. **Install Git LFS before your next pull:** run `git lfs install` once. Binary assets (`.uasset`, `.umap`, `.png`, `.jpg`, audio and meshes) are stored in LFS from commit `2279832` on. Without it you get small pointer files instead of the real assets, and the editor will fail to load them.
2. **Nights are darker now, on Windows too.** `SimDayNight` raises the auto-exposure floor after dusk (`kNightExposureFloor = 2.0` EV at night, back to your `-0.6` by day), because histogram exposure lifted moonlit nights to look like noon on Linux. The forward-shading priorities are now sun 2 / moon 1 / Linux fill 0: the engine clamps priorities at 0, so the fill's `-1` tied with the moon. If your Windows night looks too dark with Lumen, tune `kNightExposureFloor`. The moon is now placed and lit by its phase (the new calendar), so a new-moon night is meant to be dark.
3. **The city gets redesigned (D-025).** The designer asked for a city that is "less dense, more creative and alive, but compact". The design is in [city-of-the-moon.md](city-of-the-moon.md): a crescent of ~110 living buildings around a sacred lagoon, in nine quarters.
   - **What stays yours:** the terrain, water, sky, ziggurat, lighthouse, Moon Gate, the style materials and the whole look.
   - **What gets replaced:** only the 500-house grid in `BuildCity`. The layout becomes data (`places.csv` plus a new `city_districts.csv`) that both the simulation and `SimEnvironment` read.
   - This is Stage AA in [completion-plan.md](completion-plan.md). Step AA1 is agreeing it with you before `BuildCity` is touched. Say what you'd change.
4. **Voices:** the designer says you'll record them later. Every dialogue line and bark will get a voice-asset slot, so your recordings drop in without code changes.
5. **Useful now:**
   - `tools/ue_test.sh` runs the project's Unreal tests headless.
   - The developer console has `sim.Dump`, `sim.Give <item> [qty]`, `sim.SetNeed hunger 90`, `sim.AdvanceHours 5`, `sim.Teleport <place>` and `sim.Places`.
   - Headless `-ExecCmds` takes **commas**, not semicolons.
   - CI runs on every push. Pushing straight to `main` is still how we work, so please check the Actions tab after you push.
