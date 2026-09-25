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

Session A's K-2 and A-1 (realistic PBR textures) were dropped. K-2 is done in B, and A-1 contradicts D-023 (low-poly, Valheim-like).

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
6. **Housekeeping:** Session A's agent worktrees under `.claude/worktrees/` on this machine are no longer needed. Remove them with `git worktree list`, then `git worktree remove --force <path>` for each, and `git worktree prune`.
