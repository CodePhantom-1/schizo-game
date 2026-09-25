# Bug review: unreal/ and tools/*.py (2026-09-25)

Scope: `unreal/` (the SimRuntime plugin, the SchizoGame module, Config/ and the .uproject), `tools/*.py` (canon_lint, coverage_check, codex_gen, export_datatables, stage_canon_for_ue), plus a foreign-key scan of `db/canon/*.csv`. `kernel/` was read to check how the C API behaves but was not changed.

There is no Unreal Engine on the review machine, so nothing in `unreal/` could be compiled here. UE C++ was edited only for bugs I'm certain of, and only with fixes of a few lines using APIs the same files already call. Everything else below is a **proposed** patch for the designer's local build.

Severity: **High** means a crash, silently wrong world data, or a broken build or package. **Medium** means wrong behaviour in a reachable case. **Low** means a latent problem, cleanup or docs.

## Summary

| # | Where | Severity | Status |
|---|---|---|---|
| U1 | unreal/Content/Sim/canon/* | High | Fixed (tracked files refreshed); 2 untracked tables proposed |
| U2 | SimWorldSubsystem.cpp:70-76 | High | Proposed |
| U3 | unreal/Config (no DefaultGame.ini) | High | Proposed |
| U4 | SimGameMode.cpp:104 | High (crash) | **Fixed** |
| U5 | SimGameMode.cpp:50, SimPlayerController.cpp:96 | High (Game target build) | **Fixed** |
| U6 | SimPlayerController.cpp:85-88 | Low | **Fixed** |
| U7 | SimWorldSubsystem.h:65 / .cpp:131 (GetSimHandle) | Medium | Proposed (docs + usage rule) |
| U8 | SimWorldSubsystem.cpp:121-129 (GetSimHour) | Medium | Proposed |
| U9 | SimWorldSubsystem.cpp (all static getters) | Medium | Proposed |
| U10 | SimWorldSubsystem lifecycle (world subsystem) | Medium (latent) | Proposed |
| U11 | SimRuntime.Build.cs:21-22 | Medium | Proposed |
| U12 | SimRuntime.Build.cs / SchizoGame linking | Low | Proposed |
| U13 | SimWorldSubsystem.cpp:33 | Low | Proposed |
| U14 | SimWorldSubsystem.cpp:70, 164-165 | Low | Proposed |
| U15 | SimWorldSubsystem.h:14, :52-58 | Low | Proposed |
| U16 | SimGameMode.cpp:98-110 | Low | Proposed |
| U17 | SimGameMode.cpp:145 | Low | Proposed |
| U18 | .gitignore:22 | Low | Proposed |
| U19 | Config/DefaultInput.ini | Low | Note |
| T1 | tools/canon_lint.py:39-48 | High | **Fixed** |
| T2 | tools/export_datatables.py:34-42 | Medium | **Fixed** |
| T3 | tools/export_datatables.py:36-37 | Medium | **Fixed** |
| T4 | tools/stage_canon_for_ue.py | Medium | **Fixed** |
| T5 | tools/codex_gen.py:121-122 | Low | **Fixed** |
| T6 | tools/coverage_check.py:57 | Low | Note |
| D1 | db/canon/calendar.csv:5 | High | **Fixed** |
| D2 | db/canon/schedules.csv:16 | Medium | **Fixed** |
| D3 | db/canon/schedules.csv (7 roles) | Content | Reported |
| D4 | db/canon/laws.csv, buildings.csv | Note | Reported |
| K1 | kernel/src/CApi.cpp (for the kernel owner) | High | Reported |
| K2 | kernel/tests/test_capi.cpp:165 (for the kernel owner) | Low | Reported |

---

## Unreal

### U1 — The committed staged canon is stale (High, fixed for the tracked files)

`unreal/Content/Sim/canon/` is tracked in git. It was committed at bring-up (e3d10ee) and never regenerated. 15 of the 31 tracked tables differed from `db/canon`, including people, quests, schedules, items, deities and calendar. `places.csv` and `recipes.csv` were missing entirely.

**Scenario:** the designer pulls and plays. The in-engine kernel then runs the old canon: 29 fewer street residents, stale quests, and the D-018 schedule rows missing. Because `recipes.csv` is absent, `sim_world_craft` returns -2 (unknown recipe) for every recipe, even though the kernel's own tests pass against `db/canon`.

**Fixed:** I ran `tools/stage_canon_for_ue.py` and committed the refreshed tracked files. `stage_canon_for_ue.py --check` (T4) now exists so CI can catch drift.

**Proposed:** `places.csv` and `recipes.csv` are generated output that isn't tracked yet, so I did not commit them (per the review brief). Choose one:

```sh
git add unreal/Content/Sim/canon/places.csv unreal/Content/Sim/canon/recipes.csv
```

or stop tracking the directory and stage at build time (see U18).

### U2 — The "canon missing" check never fires; a missing canon yields an empty world (High, proposed)

`SimWorldSubsystem.cpp:70-76` treats `sim_world_create == nullptr` as "canon missing". The kernel's `Db::load` skips missing tables (`kernel/src/Db.cpp:88`, `if (!in) continue;`), so a wrong or missing directory still produces a valid world with no data.

**Reproduction** (kernel built out of tree, linked against `libsim_core.a`):

```
sim_world_create("/no/such/canon/dir", 1)
-> handle=NON-NULL season='' len=0 npcs=0 price=2
```

**Scenario:** a packaged build (see U3), a wrong content dir, or a fresh clone without staging gives a world with no NPCs, no seasons and no quests. There is no error; the only symptom is an empty season string on screen.

**Proposed patch** (FPaths is already used here; `FPaths::FileExists` is in the same header):

```diff
@@ SimWorldSubsystem.cpp  Tick()
-		SimHandle = sim_world_create(TCHAR_TO_UTF8(*FString(FTCHARToUTF8(*CanonDir()).Length(), FTCHARToUTF8(*CanonDir()).Get())), 1);
+		// The kernel skips missing tables, so a wrong directory would still
+		// "succeed" with an empty world: require a sentinel table first.
+		if (!FPaths::FileExists(FPaths::Combine(CanonDir(), TEXT("seasons.csv"))))
+		{
+			UE_LOG(LogSimRuntime, Error, TEXT("Canon missing at %s (no seasons.csv) — run tools/stage_canon_for_ue.py. Retries stopped."), *CanonDir());
+			bCanonFailed = true;
+			return;
+		}
+		SimHandle = sim_world_create(TCHAR_TO_UTF8(*CanonDir()), 1);
```

### U3 — Loose canon CSVs are not staged into a packaged build (High, proposed)

The runtime reads loose files from `Content/Sim/canon`. The cooker only packages cooked assets unless a directory is listed as non-UFS, and `unreal/Config/` has no `DefaultGame.ini` at all.

**Scenario:** a packaged or cooked build has no canon, and because of U2 it runs an empty world with no error.

**Proposed patch** (new file `unreal/Config/DefaultGame.ini`):

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysStageAsNonUFS=(Path="Sim/canon")
```

### U4 — Null dereference of StreetStart when its spawn fails (High, fixed)

`SimGameMode.cpp:104` (was :99) called `StreetStart->GetActorTransform()` unconditionally. The code just above logs "PlayerStart spawn failed — the pawn will start at the default" and continues, so a failed spawn crashed instead of falling back.

**Fixed:** the placement block now runs only when `PC != nullptr && StreetStart != nullptr`.

### U5 — Editor-only actor labels used in the Game target (High for SchizoGameGame, fixed)

`AActor::SetActorLabel` and `GetActorLabel` are declared only when `WITH_EDITOR` is set. The designer's editor build compiles them, but `SchizoGameGame.Target.cs` (the packaging target) has `WITH_EDITOR=0` and fails to compile at `SimGameMode.cpp:50` and `SimPlayerController.cpp:96`.

**Fixed:**
- In `SimGameMode.cpp`, `SetActorLabel` is wrapped in `#if WITH_EDITOR`.
- In `SimPlayerController.cpp`, the on-screen text uses `GetName()`, which the same function already calls on line 88.

The edit compiles safely in both targets, even if a future engine exposes labels at runtime.

### U6 — Use trace dereferences Hit.GetActor() without a check (Low, fixed)

`SimPlayerController.cpp:85-88`: a blocking hit on a component with no owning actor dereferenced null. **Fixed:** `&& Hit.GetActor() != nullptr` was added to the trace condition, so that case now logs "hit nothing".

### U7 — GetSimHandle(): correct, but the lifetime contract is incomplete (Medium, proposed)

The resolution logic is correct. It uses the same `GetSim(GEngine->GetCurrentPlayWorld())` path as every other getter and returns nullptr before lazy creation. The `struct SimWorld*` elaborated declaration in the header names the global `::SimWorld` from `CApi.h`, so the types match.

Problems:
- The handle is destroyed in `Deinitialize` (cpp:52), which runs on every world teardown, map travel and PIE stop. A caller that caches the pointer in a member or UPROPERTY-less field dangles after travel. The doc comment only says "Never destroy it."
- Future save/load (`sim_world_load*` returns a *new* world) will have to replace `SimHandle`, which invalidates every cached pointer.
- It returns a mutable handle, which breaks the header's own rule that nothing but the kernel's tick writes. Callers can call `sim_world_give_item` and the like. That may be intended for wave 3, but it should be stated.

**Proposed doc patch** (SimWorldSubsystem.h:60-65):

```diff
-	 * schedules, save/load. nullptr before the world exists. Never destroy it.
+	 * schedules, save/load. nullptr before the world exists. Never destroy it,
+	 * and never cache it: fetch it at each use. It dies in Deinitialize (map
+	 * travel, PIE stop) and will be replaced by save/load.
```

### U8 — GetSimHour(): the maths is correct; the day starts at midnight (Medium, proposed)

`24 * SecondsSinceLastDay / SecondsPerDay()`, clamped to [0, 23.999], is correct, and SecondsPerDay() matches the Tick's divisor exactly. The refactor in 73fdc53 kept the Tick behaviour, apart from the new 0.01-minute floor. Issues:

1. `SecondsSinceLastDay` starts at 0 when the world is created, so the game opens at **00:00**. The header defines day 1 as "the morning the prisoner arrives". Every wave-3 reader (sun, NPC schedules, needs) will start in the deep watch (`deep_watch`, hour 0). Choosing the start hour is a content decision, so this is proposed:

   ```diff
   @@ SimWorldSubsystem.h (after SimDaysPerRealMinute)
   +	/** Hour of day 1 at which play begins (the prisoner lands at dawn). */
   +	UPROPERTY(Config, EditAnywhere, Category = "Sim")
   +	float StartHour = 6.0f;
   @@ SimWorldSubsystem.cpp Tick(), right after the successful sim_world_create
   +		SecondsSinceLastDay = SecondsPerDay() * FMath::Clamp(StartHour, 0.f, 23.999f) / 24.0;
   ```

2. Changing `sim.DaysPerRealMinute` mid-day rescales the current hour immediately. For example, lowering it can jump the clock to 23.999 until the next Tick rolls the day over. This is acceptable for a debug CVar, but it should be documented.

3. `AdvanceSimDays` leaves the sub-day clock alone. That is fine and gives the same hour on a later day.

### U9 — Static getters resolve "the" world via GEngine->GetCurrentPlayWorld() (Medium, proposed)

All getters (cpp:105-179) ignore the caller's world. With multiple PIE instances (listen server plus clients), every instance reads and writes the sim of whichever world `GetCurrentPlayWorld` returns. The same happens when called from editor utility code outside PIE. This is fine for a single-player -game run, and wrong once PIE runs more than one client.

**Proposed:** add world-context overloads and keep the old ones as wrappers.

```cpp
UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
static int64 GetSimDayFor(const UObject* WorldContextObject);
// impl: GetSim(WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
```

### U10 — The sim is a world subsystem: map travel resets the world (Medium, latent)

`Deinitialize` destroys the kernel world, and the next map's subsystem creates a fresh one with seed 1 on day 1. With one map this doesn't matter. The first `OpenLevel` or seamless travel (entering a house map, a second city) will silently wipe all progress.

**Proposed:** move `SimHandle` ownership to a `UGameInstanceSubsystem` and keep the tickable world subsystem as a thin driver. Alternatively, decide now that the game is one persistent World Partition map and record that in DECISIONS.

### U11 — Build.cs links a Unix-only artifact from an undocumented build dir (Medium, proposed)

`SimRuntime.Build.cs:21-22` hard-codes `kernel/build-ue/libsim_core.a`:
- On Win64 the CMake artifact is `sim_core.lib`, so linking fails.
- Nothing in the repo says how `build-ue` is produced. On Linux the static library must be built with UE's bundled clang and libc++ (`-stdlib=libc++`) and `-fPIC`, because the editor links modules as shared objects. A plain `cmake -B kernel/build-ue` with system gcc/libstdc++ gives undefined `std::__cxx11` symbols.
- The header comment (line 2, line 17) still says `kernel/build/libsim_core.a`.

**Proposed patch:**

```diff
-		PublicAdditionalLibraries.Add(System.IO.Path.Combine(
-			ModuleDirectory, "..", "..", "..", "..", "..", "kernel", "build-ue", "libsim_core.a"));
+		string KernelLib = Target.Platform == UnrealTargetPlatform.Win64 ? "sim_core.lib" : "libsim_core.a";
+		string KernelPath = System.IO.Path.GetFullPath(System.IO.Path.Combine(
+			ModuleDirectory, "..", "..", "..", "..", "..", "kernel", "build-ue", KernelLib));
+		if (!System.IO.File.Exists(KernelPath))
+		{
+			throw new BuildException("SimRuntime: kernel library missing at " + KernelPath +
+				" — build it with UE's toolchain (see the comment above).");
+		}
+		PublicAdditionalLibraries.Add(KernelPath);
```

Also commit the exact `build-ue` cmake invocation the designer used, as a script or a comment.

### U12 — The kernel static lib is linked into every dependent module (Low, proposed)

`PublicAdditionalLibraries` propagates to SchizoGame. In modular (editor) builds, SimRuntime.so and SchizoGame.so each get their own copy of any kernel code they reference. Once wave-3 game code calls the C API directly on `GetSimHandle()`, two copies of the kernel operate on one `SimWorld`. This is harmless today, since the kernel has no mutable globals, but it is fragile.

**Proposed:** export thin `SIMRUNTIME_API` wrappers from SimRuntime, or have the game module call through SimRuntime only.

### U13 — CanonDir() is relative (Low, proposed)

`SimWorldSubsystem.cpp:33`: `FPaths::ProjectContentDir()` is relative to the engine binary directory (`../../../...`). `std::ifstream` in the kernel resolves it against the process working directory. This works because UE changes into the binary directory at startup, but the log line prints an unreadable path. Future `sim_world_save` paths would have the same dependency.

**Proposed:** `return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon")));`

### U14 — String conversion round-trip (Low, proposed)

`cpp:70` and `:164-165`: `TCHAR_TO_UTF8(*FString(FTCHARToUTF8(x).Length(), FTCHARToUTF8(x).Get()))` converts to UTF-8, back to TCHAR, then to UTF-8 again. It also runs `CanonDir()` twice. On UE5 `FTCHARToUTF8::Get()` yields `UTF8CHAR*`, so the round trip is lossless and merely wasteful. If an engine version resolves it to the ANSICHAR constructor, non-ASCII project paths would be mangled.

**Proposed:**

```diff
-		SimHandle = sim_world_create(TCHAR_TO_UTF8(*FString(FTCHARToUTF8(*CanonDir()).Length(), FTCHARToUTF8(*CanonDir()).Get())), 1);
+		SimHandle = sim_world_create(TCHAR_TO_UTF8(*CanonDir()), 1);
@@ GetSimPrice
-	const FTCHARToUTF8 City(*CityId);
-	const FTCHARToUTF8 Item(*ItemId);
-	return sim_world_price(Sim->SimHandle, TCHAR_TO_UTF8(*FString(City.Length(), City.Get())),
-		TCHAR_TO_UTF8(*FString(Item.Length(), Item.Get())));
+	return sim_world_price(Sim->SimHandle, TCHAR_TO_UTF8(*CityId), TCHAR_TO_UTF8(*ItemId));
```

`TCHAR_TO_UTF8` temporaries live until the end of the full expression, which covers the call.

### U15 — SimDaysPerRealMinute is named backwards (Low, proposed)

The value is real minutes per sim day (45 means one day every 45 real minutes). The CVar `sim.DaysPerRealMinute` has the same inversion, and the class comment (h:14) says "one sim day advances per SimDaysPerRealMinute of engine time". Renaming the property would drop existing ini overrides, so rename it with `+PropertyRedirects` or leave it and fix the comments. The buffer and API use are otherwise correct: `Season[32]` against 4-8 character ids, `sim_world_season` never returns -1 for a live handle, and null handles are guarded everywhere.

### U16 — StartPlay destroys the old pawn before the new spawn is known to succeed (Low, proposed)

`SimGameMode.cpp:98-110`: if `SpawnActor<APawn>` fails (for example, a null `DefaultPawnClass`), the player is left without a pawn. `Possess` already unpossesses the old pawn.

**Proposed:** spawn first, then `PC->Possess(NewPawn)`, then `Old->Destroy()` only on success.

### U17 — The on-screen clock disappears after 5 seconds (Low, proposed)

`SimGameMode.cpp:145`: the day line is rewritten only when the day changes, with a 5-second lifetime. At 45 minutes per day the clock is invisible about 99.8% of the time. **Proposed:** pass a duration of `SecondsPerDay`, or refresh every Tick using the same key (1), which replaces rather than stacks.

### U18 — .gitignore names a path that doesn't exist (Low, proposed)

`.gitignore:22` ignores `unreal/SchizoGame/Content/Sim/canon/`. The real stage is `unreal/Content/Sim/canon/`, and it is tracked. The old stage docstring called it "git-ignored". Decide which one you want:
- **Keep it tracked** so the designer's pull works with no extra step. Then fix the dead ignore line, commit `places.csv` and `recipes.csv`, and add `stage_canon_for_ue.py --check` to the gate.
- **Ignore it.** Then change the line to `unreal/Content/Sim/canon/`, `git rm --cached` the directory, and have Build.cs or a PreBuildStep run the stage script.

### U19 — Legacy input axes are unused (Low, note)

`DefaultInput.ini` defines `MoveForward`, `MoveRight`, `Turn` and `LookUp`, but the engine's `ADefaultPawn` binds its own `DefaultPawn_*` axes. Only `Use` is consumed. Legacy action mappings are deprecated in UE5 in favour of Enhanced Input. This is a note for the input wave.

---

## tools/*.py

### T1 — canon_lint doesn't check field count, despite its docstring (High, fixed)

**Reproduction:** a row `x,s,v,INVENTED,ref (a, b)` under a 5-column header passed the lint (rc 0). DictReader parks the surplus field under the `None` key and silently truncates `source_ref`. A real instance is D1.

**Fixed:**
- Rows with surplus fields (the `None` key) or missing fields (`None` values) now fail.
- Error locations use `reader.line_num`, which stays correct with multi-line quoted fields.
- An id with surrounding whitespace now fails. The kernel matches ids verbatim, and the lint used to strip before checking.
- The docstring now lists INVENTED as a valid tag.

### T2 — export_datatables crashes on a blank line (Medium, fixed)

**Reproduction:** a blank line inside a table raised `IndexError: list index out of range` at `[r[0]] + r[1:]`. **Fixed:** blank rows are filtered out.

### T3 — export_datatables leaves stale exports behind (Medium, fixed)

**Reproduction:** if a table is exported and then drops to header-only (or is removed), the old `data/ue/<table>.csv` stays as live DataTable data while the MANIFEST no longer lists it. **Fixed:** a table that is skipped has its previous export removed (only files named in `TABLES` are touched).

### T4 — stage_canon_for_ue never removes stale tables and has no drift check (Medium, fixed)

**Reproduction:** a table staged and then removed from db/canon stayed staged, and the kernel kept loading it in-engine. Two smaller problems: the error message said "data/ue missing" while the source is db/canon, and the docstring claimed the stage is git-ignored.

**Fixed:**
- The stage now mirrors db/canon, removing stale files.
- New `--check` flag: writes nothing and exits 1 on drift, for CI.
- The message and docstring are corrected.

### T5 — The codex "Voices" section had no text (Low, fixed)

**Reproduction:** dialogue rows rendered as `- **d1** [INVENTED]` with no speaker and no line, because `text` was not in the detail chain. **Fixed:** dialogues render as `speaker (id) — text`.

### T6 — coverage_check directory evidence reads binaries as UTF-8 (Low, note)

`coverage_check.py:57`: `p.read_text(encoding="utf-8")` over `**/*` would raise on a binary file. No current evidence entry is a directory, so this is not triggered. Left as is.

---

## db/canon data defects

### D1 — calendar.csv:5 `festival_days` has an unquoted comma (High, fixed)

`source_ref` contained `(kispum_feeding_the_dead, first_portion_to_the_gods)` unquoted. That gave 6 fields against 5, so every reader truncated `source_ref` at the comma. Canon_lint missed it (T1). **Fixed:** the field is quoted, and the content is unchanged. The row is INVENTED, not CANON, so D-018 allows it.

### D2 — schedules.csv:16 `midday_rest` had a role no one holds (Medium, fixed)

The role was `workshops and households`. Roles are matched exactly against `people.csv` (`matching_schedule_role`) and in `sim_world_task_at`, so the midday rest never applied to any NPC. **Fixed:** the row is split into `midday_rest` (role `craftsman`) and `midday_rest_households` (role `household`), with the same text, both INVENTED. This is recorded in `docs/proposals/invented-ledger-street.md`.

Craftsmen now rest from 12 until `workshops_full_work` at 10 the next day, instead of working around the clock. Households rest from 12 to 20 instead of holding the evening meal all day. All 24 kernel suites that run on this machine still pass; the one failure is the pre-existing test_capi issue in K2.

### D3 — Schedule roles no resident holds (content decision, reported)

`dockworker`, `field hand`, `fisherman`, `herdsman`, `lighthouse keeper`, `paladin` and `priest of the sun` have schedule rows but no `people.csv` resident. The street's temple is the temple of *sun and moon*, yet only moon priests exist. Either author residents, for example a sun priest at `temple_front_place` and a lighthouse keeper if the lighthouse is on-map, or accept these as off-street roles.

### D4 — Notes (no action)

The other foreign keys are clean:
- Every rite deity exists (or is `any`).
- Every recipe input, output and station is in items.csv.
- Every place city, building and owner exists.
- Every person's city and faction exists.
- Every ending's faction and every schedule season exists.
- No duplicate (role, hour, season) keys.

Two free-text columns are expected rather than defects. `laws.jurisdiction` uses institution values (`city_gates`, `crown`, `temple`), not ids. `buildings.first_great_temple.city_or_context` is free text; the sweep already noted it. `ranks.tier` values such as `0 — the Outsider` are CANON and not integers, so the kernel must parse the leading number.

---

## Kernel notes (for the kernel owner; not changed)

- **K1 (High):** only `sim_world_create`, `sim_world_load*` and a few others catch exceptions. `sim_world_advance_days`, `sim_world_save`, `sim_world_craft` and the rest can throw (`std::bad_alloc`, `.at()`) across the `extern "C"` boundary into UE code, which is compiled with exceptions off. That is `std::terminate`, a hard crash. Wrap every C entry point in `try { … } catch (...) { return <error>; }`.
- **K2 (Low):** `kernel/tests/test_capi.cpp:165` writes to the hard-coded `/tmp/claude-1000/`. It fails on any machine without that directory, including this one (`claude-0`), and fails the same way at HEAD. Use `std::filesystem::temp_directory_path()`.

## Tool runs (after fixes)

```
canon_lint            LINT PASSED — every row tagged, sourced and unique.
coverage_check        COVERAGE PASSED — all 27 notes elements have a blueprint home.
codex_gen             wrote build/codex.md (319 lines)          (build/ is untracked)
export_datatables     manifest written — 18 tables              (data/ue/schedules.csv 33 rows)
stage_canon_for_ue    staged 33 canon tables; --check: staged canon is current
kernel ctest          24/25 (test_capi: K2, pre-existing, fails identically at HEAD)
```
