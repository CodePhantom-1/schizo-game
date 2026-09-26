# HANDOFF: how the repo was consolidated (2026-09-25)

## What happened
Two Claude Code sessions worked on this game on the same day:

- **Session A** (local, `~/Desktop/game/schizo-game`) ran waves 1–3: save/load, needs, crafting, schedules, crime/quest/rite scenarios, the Blender house kit, street residents, dialogue, journal, and the wave-2 bug review. It pushed to `main` up to `73fdc53`.
- **Session B** (branch `claude/admiring-babbage-c0jyzr`) continued from that `main` with its own K-1/K-2, D-020–D-023, the Unreal review fixes, and wave 4: character progression, wild lands, and combat.

To consolidate, `main` on GitHub and on this machine was fast-forwarded to Session B's branch. Session B's branch already contained everything Session A had pushed, so nothing merged was lost. From now on, **one session coordinates** (Session B's line of work).

## What was parked (not merged) and where it is on GitHub
| Branch | What it is | Status |
|---|---|---|
| `backup-local-session` | Session A's unpushed local `main`: its own K-1 merge (a duplicate of B's K-1) | Reference only, don't merge |
| `wip/ue-1-simruntime-bindings` | Engine bindings, `sim.Save`/`sim.Load`/`sim.Status` commands, start hour, canon staging | Unfinished and mostly overlapped by D-020 part 2. Salvage ideas only. |
| `wip/ue-2-player-verbs` | Doors, well, bed, pickup, eat/drink, needs over time, HUD | Unfinished, never compiled. Good spec for the next Unreal wave. |
| `wip/ue-3-street-from-kit` | House kit → UE import, Moon Gate Quarter built from `places.csv`, door slots, place registry | Unfinished. Good spec for the next Unreal wave. |
| `wip/ue-4-townspeople-daynight` | NPC actors walking their schedules, director, day/night sun | Unfinished. Good spec for the next Unreal wave. |
| `archive/session-a-wave2-kernel-wip` | Session A's stopped wave-2 follow-on (kernel test WIP) | Archived; superseded by later waves |
| `archive/session-a-texture-tools` | Session A's realistic-texture pipeline (dropped per D-023) | Archived; do not build on it |

Session A's K-2 and A-1 (realistic PBR textures) were dropped. K-2 is done in B, and A-1 contradicts D-023 (low-poly, Valheim-like).

`claude/admiring-babbage-c0jyzr` on GitHub is fully contained in `main` — delete it once you are sure that session is closed.

## Audit after consolidation (2026-09-25, later session)
- `main` local == `origin/main`; the combined tree **builds clean and passes 37/37 kernel tests**.
- Salvaged from `backup-local-session`: the Entry-map default in `unreal/Config/DefaultEngine.ini` (editor/`-game` froze on the OpenWorld template; the tiny engine Entry map fixes it — "the black screen").
- Deleted: the stale untracked `unreal/SchizoGame/` tree (an old canon staging from before wave 4; the tracked `unreal/Content/Sim/canon` is current per `stage_canon_for_ue.py --check`), all 22 agent worktrees, and every dead local branch. Local repo is `main` only.
- **Lost with the accidentally-resumed session and must be redone** (no trace in any commit, branch, or worktree):
  1. **Divine wrath** — the kernel module and its C API block were built but never committed.
  2. **Faction politics deciding raids** — `kernel/src/Faction.cpp` exists (wave 2, bug-reviewed) but nothing consults it; the wild-lands raid formula does not use factions, and Faction is not exposed in the C API.

## Wave 5 (2026-09-25 evening) — done, reviewed, pushed
Both lost pieces rebuilt and merged; 40/40 kernel tests. Divine wrath (`sim/Divine.hpp`, `CApiDivine.h`: wrath per offender×deity from broken oaths/impure rites/convictions, tiers → omens/favour/curse, atonement; ledger `invented-ledger-divine.md`). Faction raid politics (treaties in `wild_groups`/`treaties.csv`, `CApiFaction.h`: war/grudge/treaty/outlawry modulate the raid formula, treaty blocks raids outright; ledger `invented-ledger-faction-raids.md`). Bug review applied: treaty blocks player-led raids too, unaligned-band grudges restored, C API contract nits fixed.

## Wave 6 (2026-09-25 evening) — merged; the playable slice
- **Player verbs + HUD** (W6-A): E use (doors/well/bed/pickups/tablet), F eat / G quaff (kernel ranks), Tab inventory; HUD draws needs/health/conditions/purse from the C API; new kernel reads `sim_world_inventory/best_food/best_drink` (`CApiVerbs.h`).
- **The street** (W6-B): `SimStreetBuilder` builds the Moon Gate Quarter from `places.csv` (25 places, 16 door slots) with the mudbrick kit (13 pieces, `unreal/Content/Art/Kit/` — committed, so a pull plays with real meshes); layout is a pure function of CSV row order. `tools/art/ue_import_kit.py` re-imports after kit changes.
- **Townspeople + day/night** (W6-C): `SimNpcDirector` spawns residents from kernel schedules (place resolver wired to the street's registry), `SimDayNight` sweeps the sun 15°/h on the sim clock.
- **Coordinator wiring**: street → gate PlayerStart/tablet; sleep skips the clock (`USimWorldSubsystem::SkipSimHours`) without double-charging needs.
- **Launching — READ THIS FIRST (AMD GPU + UE 5.8 Linux driver bug):**
  UE 5.8 + the stock RADV (Mesa) driver hangs the GPU: `VK_ERROR_DEVICE_LOST` on queue submit (known issue: forums.unrealengine.com/t/5-8-freezing-on-ui-interaction/2729467). Before the first launch on this machine, build the patched RADV once:
  ```
  # deps into ~/.local (no root): pip3 install --user --break-system-packages meson ninja;
  # build glslang 16.1 into ~/.local/glslang (cmake), extract the libxcb *-dev + xshmfence-dev
  # debs into ~/.local/xcb-deb (see git history of this file / session log for the list)
  git clone --depth 1 -b stacked-fix https://gitlab.freedesktop.org/bertonha/mesa.git ~/mesa-stacked-fix
  cd ~/mesa-stacked-fix && meson setup build . -Dgallium-drivers= -Dvulkan-drivers=amd \
    -Dllvm=disabled -Dplatforms=x11,wayland -Dbuildtype=debugoptimized -Dbuild-tests=true \
    -Dprefix="$HOME/.local/mesa-radv-test" && ninja -C build && ninja -C build install
  ```
  **Status on this machine (2026-09-25): even the patched RADV did not save the 5700 XT — the kernel amdgpu driver (6.17) wedges on UE's workload into a GPU-reset storm (journal: "GPU reset … device wedged"), regardless of Mesa version. The game runs STABLE on the iGPU the monitor is wired to.** Playable launch today:
  ```
  VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json SDL_VIDEODRIVER=x11 \
    ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor "$PWD/unreal/SchizoGame.uproject" \
    -game -windowed -ResX=1280 -ResY=720
  ```
  To return to the 5700 XT: install an LTS kernel (e.g. `sudo apt install linux-generic-hwe-24.04` style — needs the designer's sudo) and retry the plain launch; the patched RADV above is still worth keeping. **Never launch with `-opengl4`** on this hybrid-GPU box — it hard-crashed the whole machine once (2026-09-25). The first window can sit black a few minutes while pipelines compile — that is loading, not a hang.
  The renderer is already configured conservatively (`DefaultEngine.ini` [/SystemSettings]: no Lumen/RT/reflections/virtual shadows, no split barriers).
- Known-unfinished in the slice: placeholder demo props for well/bed/pickups (`sim.VerbDemoProps 0` hides them), no sky ambient light (runtime sky capture is the GPU-hang path; return with an offline cubemap), NPC names show kernel ids, wave-6 review findings to be triaged next session.

## How to proceed
1. **One coordinator only.** Continue in Session B (or a fresh session that reads this file). Don't run two coordinators on this repo.
2. **Keep `main` current.** Whoever works merges finished work into `main` and pushes, so the collaborator always has it.
3. **Next wave: make it playable in Unreal.** Player verbs, the street from the house kit (in the D-023 low-poly style), townspeople and day/night. Use the `wip/ue-*` branches as specs, but rebuild on current `main`.
4. **Process rules** (the designer's):
   - Batch of agents → merge → bug review → fix → push → next batch.
   - Push `main` **before** launching agents: new worktrees are cut from `origin/main`.
5. **Build and run locally:**
   ```
   cmake -S kernel -B kernel/build-ue -DCMAKE_BUILD_TYPE=Release && cmake --build kernel/build-ue
   python3 tools/stage_canon_for_ue.py
   ~/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh SchizoGameEditor Linux Development -Project="$PWD/unreal/SchizoGame.uproject"
   ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor "$PWD/unreal/SchizoGame.uproject" -game
   ```
6. **Housekeeping:** done in the audit above — worktrees and dead branches are removed; `main` is the only local branch.

## The art pass (2026-09-25 night) — the slice looks like a game
- Real CC0 surfaces on every wall/roof (ambientCG via tools/art/fetch_textures.py; metre-scaled planar UVs; authored M_Ground/M_PlasterWall; tools/art/README.md is the pipeline bible).
- Real people: 10 Quaternius CC0 animated humans (tools/art/fetch_characters.py + ue_import_characters.py) — the player and every NPC; cylinders remain only as the no-asset fallback.
- The street is dressed: 10 code-built props placed by place kind, a gradient sky dome (unlit, no GPU capture), warm post-process (tools/art/props_gen.py + ue_import_props.py; sim.PropsDressing 0 hides).
- Verification: kernel probed end-to-end like a player (69/70 beats; the one bug — player exile voiding faction treaties — fixed, split books in Faction.hpp, 40/40); UE code audited (gate now passable, dead PSO cvar fixed, window/roof collision restored).
- The iGPU's FIRST launch after any content change compiles pipelines for ~10-15 min (frames stall, window may sit black) — it is working, not hung; subsequent launches are fast. The 5700 XT remains gated on the kernel reboot.

## Linux check of Tommy's b5a0e5e (2026-09-25)
- Builds on Linux / UE 5.8.3; `-game -nullrhi` boots to day 1 (the city: 491 houses, terrain, walls, ziggurat, lighthouse); the iGPU windowed launch renders day and night with no GPU reset.
- Fixed on the way: `tools/build_kernel_for_ue.sh` now finds libc++ inside the UE 5.8 clang SDK (it moved out of ThirdParty/Unix). `SimDayNight`: forward-shading priorities clamp at 0 in the engine, so the fill's -1 tied with the moon at night (the orange "multiple directional lights" warning) — now sun 2 / moon 1 / fill 0; and the night exposure floor rises after dusk (`kNightExposureFloor`), because histogram auto-exposure lifted moonlit nights to look like noon.

## Stage A part 1 (2026-09-25 night) — done, reviewed, pushed
- **Landed:** the calendar turned on (months.csv, calendar_days.csv; moon phase + integer illumination in the kernel; six C API calls; the HUD's date and moon lines; the moon placed and lit by phase, smooth through the month); the 48-minute day (adjustable); the developer console (`sim.Dump`, `sim.Give`, `sim.SetNeed`, `sim.AdvanceHours/Days`, `sim.Standing`, `sim.Favour`, `sim.Drought`, `sim.War`, `sim.Teleport`, `sim.Places`); CI on every push to main; the Linux night-lighting fixes (A1 above).
- **Test tools:** `tools/ue_test.sh` (Unreal automation, `Sim.*` only; fails on crash/timeout/incomplete runs) and `tools/ue_smoke.sh` (boots the real map headless, runs a day and a half, checks city + sim + calendar). Headless `-ExecCmds` take **commas**.
- **Decisions:** stale branches deleted (designer yes). No branch protection (delegated): pushes to main stay direct; CI reports and is checked after every push. The moon rites that fire at the Moon Pool moved to O4; the rest of A3's smoke lines arrive with A10/C3/D1.
- **For Tommy:** [docs/notes-for-tommy.md](docs/notes-for-tommy.md) (git lfs install; darker nights; the crescent city redesign).
- **Next:** Stage A part 2 — the bridge (quests, journal, dialogue, NPC memory, events feed through the C API; a report of C API calls Unreal doesn't reach yet), then part 3 (the UI shell: menus, settings, save/load, new game, camera), then Stage AA (the crescent city).

## Stage A part 2 (2026-09-26) — the bridge: done, reviewed, pushed
- **C API:** `CApiQuests.h` (quest lists, title/giver/kind/act/stage/deadline, accept/advance/complete/abandon, the journal, dialogue lines) and `CApiPeople.h` (npc display names, memories, the events feed). `Quests::abandon` journals "abandoned".
- **Rule:** a quest with no `quests.csv` row (WildWorld's `rescue_<npc>`) is driven by the world: never offered, and the player's verbs return -4 (so completing it can't pay a rescue that didn't happen); it gets a generated title ("Rescue <name>"). Every canon quest of any kind stays the player's; the act is metadata, never a lock (D-021).
- **Unreal:** `USimQueryLibrary` (Blueprint) + `SimQuery::` (C++) for quests, journal, dialogue, names, memories, events; console `sim.Quests/Accept/Journal/Talk/Memory/Events`.
- **Reach report:** `docs/capi-reach.md` — every kernel call, whether Unreal uses it yet, and the stage that will; CI fails a call with no stage.
- **Next:** Stage A part 3 — the UI shell (menus, settings, save/load slots, new game, the journal/dialogue panels on USimQueryLibrary, third-person camera polish).

## Stage A part 3 (2026-09-26) — the UI shell: done, reviewed, pushed
- **How to play now:** start the game → a loading notice (first launch after an update compiles shaders for 10–15 min) → the main menu over the city: Continue · New Game · Load · Settings · Quit.
- **Keys:** WASD move · mouse look · E use · F eat · G drink · Tab (hold) carried goods · J journal · Esc pause (gamepad Start / B back) · F5 quicksave · F9 quickload · right mouse / left trigger aim · V shoulder swap · Space jump · Shift sprint. All rebindable (Settings → Controls; a key taken by one action leaves the other; Reset restores the project keys).
- **Saving:** named slots (Save/Load screen), quicksave, autosave on every night slept. A bad save is refused with a notice and the game continues. Loads restore the hour even if the day length changed.
- **Settings** persist per user (GameUserSettings.ini): day length, needs severity, Mythic/Chronicle, guided mode, fast travel, illness, permadeath, graphics, 4 volumes, sensitivity, invert-Y, subtitles, text size, colour vision, player voice. Unapplied changes are discarded however the screen closes. Fields whose system is not built yet are stored and read by their stage (listed in SimGameUserSettings.h).
- **UI tech:** Slate in C++ (unreal/Source/SchizoGame/Private/UI/), a screen stack in USimShellSubsystem; `-SimShotScreens=MainMenu,Pause,…` captures each screen; `-SimNoMenu` for unattended runs.
- **Known gaps:** the camera and the menus were not checked on screen by a person yet (the windowed launch stalls while the desktop session is idle/locked) — play it once; the placeholder street goods reset on New Game but not on Load (world items become kernel state in Stage C).
- **Next:** Stage AA — the crescent city (agree docs/city-of-the-moon.md with Tommy first).

## Stage AA (2026-09-26) — the crescent city stands (D-025)
- **Data:** `building_types.csv` (70 types), `city_districts.csv` (the crescent frame + quarters), `places.csv` with 121 footprints from `tools/city_layout.py` (tested: no overlaps, clusters on their ground, nothing in the sea; CI checks it's current); `tools/plot_city.py` → `art/review/city_plan.png`.
- **Engine:** `SimCityData` reads the crescent; `SimStreetBuilder` builds every building in its own frame, doors to the ring street; `SimEnvironment` builds the lagoon with channels to river and sea, the arc walls with the citadel bulge, both gates in the wall, the ziggurat on the belly, the lighthouse mole from the east horn, lit windows and street palms from the places; the game starts outside the quayside prison barracks (AA6).
- **Screenshots without a screen:** `-RenderOffscreen` renders even while the desktop is locked: `UnrealEditor … -game -RenderOffscreen -SimShots=<dir> -SimShotViews=aerial,zig,gate,street,harbor`. Views in `art/review/crescent/`.
- **Next:** Stage V1–V3 — the material library, house kit v2 and the typology catalogue, so buildings stop being plain boxes.

## Stage V batch 1 (2026-09-26) — the asset pipeline and generated buildings
- **Compass (D-026):** north is −Y; the world no longer mirrors the map (noon sun south, desert south, mountains north).
- **Assets:** `art/assets.csv` v2 (SPDX licence, author, url, date, sha256); `license_gate.py` in CI rejects NC/ND/SA/GPL and keeps `docs/credits.md` current. `sources.py` fetches `art/sources.csv` (Poly Haven, ambientCG, Kenney, KayKit) into gitignored `art/source/`.
- **Look:** `art/palette.csv` (64 colours) + `trim_atlas.py` (16 materials x 4 wear states) → `art/review/trim_atlas.png`.
- **Buildings:** `building_grammar.py` turns each `places.csv` row into a recipe, `building_mesh.py` (Blender) into a GLB, `check_buildings.py` gates them (doorway open, budget), `ue_import_buildings.py` puts 94 `SM_B_<place>` + `M_Building` in `/Game/Art/Buildings`. `SimStreetBuilder` stands one mesh per place, the kit as fallback; `Sim.StreetBuildings` proves the door slots are the same either way. Review: `art/review/buildings_contact.png`. Commands: `tools/art/README.md`.
- **Invented choices:** `docs/proposals/invented-ledger-buildings.md`.
- **Known gaps:** the first boot after an import builds the meshes into the DDC (slow once). Screenshots and any rendering run go on the iGPU (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`): an unprefixed offscreen run on the 5700 XT hard-crashed the machine again on 2026-09-26. The Kenney/KayKit/Quaternius sources are fetched but not placed yet (batch 2).
- **Next:** start at [docs/superpowers/plans/STAGE-V-ROADMAP.md](docs/superpowers/plans/STAGE-V-ROADMAP.md) — batches 2–7 are fully planned (flora and street life, terrain, fauna, atmosphere and sound, the props library, other cities and performance), in order, with how to run every tool on Linux and Windows and the rules batch 1 learned the hard way.

## Stage V batch 2 (2026-09-26) — flora and street life (done on Tommy's Windows box)
- **Data:** `db/canon/flora.csv` (19 species and clutter rows: habitats, seasons, withering, density, scale); `art/scatter_meshes.csv` (36 meshes: an allow-list of Kenney Nature Kit and KayKit pieces + 5 code-built plants, with real sizes).
- **Pipeline:** `scatter_mesh.py` (Blender: palette-locked GLBs, leaf mask in vertex alpha) → `check_scatter.py` → `scatter.py` (placement → tracked `unreal/Content/Sim/scatter.csv`, `--check` in CI) → `ue_import_scatter.py` (`/Game/Art/Scatter`, `M_Scatter`, `MPC_World`). Commands: `tools/art/README.md` "Scatter". Review: `art/review/scatter_contact.png`, `art/review/scatter_plan.png`, `art/review/crescent/vb2_*.png`.
- **Engine:** `ASimScatter` stands 1,053 instances in 36 HISMs after the street; the kernel's drought browns every leaf (`MPC_World.Wither`) and spring flowers hide out of season. `Sim.Scatter`; `ue_test.sh` 66/66, `ue_smoke.sh` all ok. `SimPropsBuilder` no longer dresses the doorsteps of generated buildings. `-SimShotDrought=<stage>` for review shots.
- **Windows:** `tools/ue_test.sh` and `tools/ue_smoke.sh` run from Git Bash (`UE_ROOT="/c/Program Files/Epic Games/UE_5.8"`); `sources.py` hashes and paths are separator-safe. Run the Python tools with `PYTHONUTF8=1` on Windows (several read files without an encoding).
- **Invented choices:** `docs/proposals/invented-ledger-flora.md`. Rulings and deferred minors: the batch 2 plan's Status section.
- **Found in passing:** `ue_import_buildings.py` connects VertexColor's `"RGB"` pin, which does not exist (the RGB output is `""`), so `M_Building`'s tint and grime never reach the buildings; and the mesh build stores vertex colour sRGB-encoded (decode with a 2.2 power, as `M_Scatter` now does). Not fixed here (batch 1's material, changes the look).
- **Next:** batch 3 — terrain ([plan](docs/superpowers/plans/2026-09-26-stage-v-batch3-terrain.md)).

## Stage V batch 3 (2026-09-26) — the terrain (done on Tommy's Windows box)
- **Ground detail:** `tools/art/ground_atlas.py` (16 luminance-normalised tiling cells from the CC0 Poly Haven sets + procedural salt, dunes, sherds) → `ue_make_terrain_material.py` → `/Game/Art/Terrain/M_Terrain`. `ASimEnvironment::GroundKind` names each terrain triangle's ground (vertex alpha); Tommy's `TerrainColor` is untouched (`Sim.Terrain.GroundKind` pins its colour hash from the pre-refactor build). The drought cracks fields and silt and spreads salt pans through `MPC_World.Wither`.
- **Landforms** in `TerrainHeight` (before every mask, so the apron, lagoon, road, canals, river and sea win): levees, five tells 3–4.5 km out, gullies down the southern dunes; salt pans as a field ground kind. `Sim.Terrain.Landforms` pins the apron's heights and checks the road, canals and tells.
- **Countryside scatter:** `ASimScatter::ScatterCountryside` — barley (shoots at sowing, ripe at harvest), bare rows in the rains, date groves on levees, reeds on banks, dune scrub, sherds on the tells; ~44,500 instances, 6 m clear of the road west and 3 m of Tommy's palms (`Sim.Scatter.Countryside`).
- **Tools:** `-SimShotAdvanceDays=<n>` for season shots; `-SimQuitAfter=<s>` lets an unattended run quit itself (the Windows smoke test no longer kills the editor, which tripped an engine shutdown ensure). `ue_test.sh` 68/68, `ue_smoke.sh` all ok on Windows.
- **Invented choices:** `docs/proposals/invented-ledger-terrain.md`. Rulings (the plan's tell distances were beyond the 15 km terrain window; field dykes cannot read on the 15 m grid) and deferred minors: the batch 3 plan's Status section.
- **Found in passing:** Tommy's `M_Flat` links its snapped "blocky" position to a Noise pin called `Position`, which does not exist (it is `World Position`), so the blocky look was never on; `M_Terrain` matches what renders. Tell Tommy before fixing (it changes every flat surface).
- **Next:** batch 4 — fauna ([plan](docs/superpowers/plans/2026-09-26-stage-v-batch4-fauna.md)).

## Stage V batch 4 (2026-09-26) — the fauna (done on Tommy's Windows box)
- **Data:** `db/canon/fauna.csv` (20 species, §10-checked) and `art/fauna_models.csv` (the Quaternius survey: no goat or cat exists, so they are variants of the deer and the fox; the farm sheep and pig have Idle and Jump only).
- **Pipeline:** `fetch_fauna.py` (QAL checked on quaternius.com) → `fauna_variants.py` (breeds, palette coats, real sizes, clips kept) + `birds_gen.py` → `ue_import_fauna.py` (`/Game/Art/Fauna`, 5 role clips per animal, `M_Bird`). Commands: `tools/art/README.md` "Fauna".
- **Engine:** `ASimFauna` — herds that follow the kernel's head count, beasts and city animals at their places, jackals by night, flocks as boids that lift from the player; every frame. `Sim.Fauna.Herds/HomeGround/Night/MissingModels/Birds`; `ue_test.sh` 73/73, `ue_smoke.sh` all ok. Review: `art/review/crescent/vb4_*.png` (the lineup, the lagoon birds).
- **Invented choices:** `docs/proposals/invented-ledger-fauna.md`. Rulings and deferred minors: the batch 4 plan's Status section.
- **Licence:** the animal models are QAL (no redistribution): keep this repo private, or replace them before it goes public.
- **Next:** batch 5 — atmosphere and sound ([plan](docs/superpowers/plans/2026-09-26-stage-v-batch5-atmosphere-sound.md)); `ASimFauna::OnAnimalCue` is its hook for barks, lifts and flights.

## Stage V batch 5 (2026-09-26) — atmosphere (done on Tommy's Windows box; sound deferred)
- **Engine:** `ASimAtmosphere` reads the kernel's weather, drought, hour and festival, eases toward a target state (60 s; wet ground dries over two game hours) and drives `MPC_World` (Wet, Dust, Festival, Shimmer, Rain) and Tommy's `ASimDayNight::SetWeatherBlend`. It draws rain and dust around the camera, smoke from every soot source on the fire schedule, and the night sky: 1,604 real stars, the zodiac's figures (`sim.Zodiac 1`) and the Milky Way, turning by sidereal time at 31° N. The drought lowers the canals 30 cm a stage; festival days put up banners, garlands and lamps at the temple. Console: `sim.Weather <clear|hot|scorching|sandstorm|rain|fog|auto>`, `sim.Zodiac 0/1`. Shot views `smoke`, `south`, `north`, `precinct`, `canal`.
- **Tests:** `Sim.Atmosphere.Targets/Easing/Fx/Sky`, `Sim.Terrain.CanalDrought`, the `Sim.Scatter` festival checks, `test_art_stars.py`, `test_art_smoke.py`; `ue_test.sh` 78/78, `ue_smoke.sh` all ok. Review: `art/review/crescent/vb5_*.png`.
- **Pipeline:** `fx_gen.py` → `ue_make_fx_materials.py` (`/Game/Art/FX`), `star_dome.py` (the Yale Bright Star Catalogue, public domain, fetched by `sources.py bsc5`). Commands: `tools/art/README.md` "Atmosphere".
- **Invented choices:** `docs/proposals/invented-ledger-atmosphere.md`. Rulings and deferred minors: the batch 5 plan's Status section.
- **Sound is not done:** the wishlist (`art/sounds.csv`) and the Freesound adapter are in; the fetch, import and `SimAmbience` wait for the designer's `FREESOUND_API_KEY` (Task 5 part 2 in the plan).
- **Next:** batch 5 Task 5 part 2 when the key is set; batch 6 — the props library ([plan](docs/superpowers/plans/2026-09-26-stage-v-batch6-props-library.md)).
