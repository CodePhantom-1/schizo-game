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
