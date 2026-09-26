# Notes for Tommy

Things you need to know before you pull and work, newest first. Delete a note once you've handled it.

## 2026-09-26

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
