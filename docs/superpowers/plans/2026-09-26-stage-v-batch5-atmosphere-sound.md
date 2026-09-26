# Stage V batch 5 — atmosphere and sound: weather, smoke, the night sky, the drought made visible, the city heard

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or subagent-driven-development). Steps use checkbox (`- [ ]`) syntax. Needs batches 2–4 (`MPC_World`, `ASimScatter`, `USimFaunaSubsystem`'s `OnAnimalCue`). `SimDayNight` (sun, moon, fog, sky, grade) is **Tommy's** — extend it, keep his values as the clear-day defaults.

**Goal:** A dust storm, a rainy dusk and a festival night each look and sound distinct; ovens smoke at dawn and cooking fires at dusk; the night sky carries the real stars with the zodiac you can point at; the drought is visible (browning, cracking, dust, shrinking water) and the city is heard (market, smithy, frogs, dogs, wind, the call of the precinct).

**Architecture:** One driver, `USimAtmosphereSubsystem`, reads the kernel once per in-game minute — weather (`sim_world_weather`, `weather.csv` ids), season, drought, the hour, today's festival (`festivals.csv`) — and writes a single `FSimAtmosphere` state into `MPC_World` scalars (`Wither`, `Wet`, `Dust`, `Festival`) and into `SimDayNight` (fog density/colour, sky tint, sun intensity) through a new small API on `ASimDayNight`. Effects are code-built and headless-authorable (no hand-made Niagara): rain streaks and dust motes as camera-following instanced quads animated in the material; smoke as rising billboard instances at the grammar's `soot` sources; the star field a generated dome mesh from the public-domain Yale Bright Star Catalogue. Sound: a wishlist `art/sounds.csv` fetched by `sources.py` (a Freesound adapter; CC0/CC-BY only) under the same licence gate, imported to SoundWaves, played by `USimAmbienceSubsystem` from zones derived from places and habitats.

**Tech stack:** Python 3 (catalogue, fetchers, tests); Blender 5.2 (the star dome, smoke card); UE 5.8 Python (materials, sound import); C++.

**Spec:** `docs/world-art-plan.md` §9 (atmosphere and life), §13 V8; `reports/Automated game asset pipeline.md` stage on sound (Freesound CC0/BY, Sonniss); `db/canon/weather.csv`, `festivals.csv`, `planetary_powers.csv` (D-025 astrology).

## Global Constraints

- Sound licences: `CC0-1.0`, `CC-BY-4.0` (credited in `docs/credits.md` by `license_gate.py --write-credits`), `LicenseRef-Sonniss`. **No CC-BY-NC, no Sampling+.** The Freesound API needs a key: `FREESOUND_API_KEY` in the environment, created by a human on freesound.org — agents never create accounts; if it is unset the fetcher prints how to get one and exits 2 (not a failure of CI: CI never fetches).
- Performance: effects ≤ 1.5 ms GPU on Tommy's RTX 3060 at 1080p; ≤ 3 ms on the Linux iGPU (measure with `stat gpu` in a shot run and record the numbers).
- The clear day with no drought must look exactly as today (every new parameter's neutral value = no change): a shot at 12.00 before and after the batch, diffed (`tools/art/imgdiff.py`, new, mean abs difference < 1 %).
- Rendering on the designer's Linux box: iGPU (`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json`).

## Review Focus

1. Weather changing mid-day blends over ≥ 60 real seconds (fog and wetness never snap).
2. A dust storm lowers visibility enough to read as danger (fog density ×8, colour ochre) but the HUD and the player's own feet stay visible (fog start distance ≥ 3 m).
3. Wetness applies to buildings, terrain and scatter alike (`MPC_World.Wet` darkens albedo ×0.7 and lowers roughness to 0.4 on all three materials), dries over 2 in-game hours after the rain.
4. The star field turns with the sidereal day and the zodiac band sits on the ecliptic; at the latitude of Ur (~31° N) the pole star is ~31° up in the north (−Y, D-026).
5. Sound with a missing file: the zone stays silent with one warning; no crash; volume obeys the Settings screen's four volume sliders (`USimGameUserSettings`).

---

### Task 1: the atmosphere driver and `ASimDayNight`'s surface

**Files:** Create `Public/SimAtmosphere.h`, `Private/SimAtmosphere.cpp`, `Private/Tests/SimAtmosphereTest.cpp`; modify `Public/SimDayNight.h`/`Private/SimDayNight.cpp` (add `SetWeatherBlend(float Dust, float Rain, float Overcast)` that scales his fog/sky/sun values; neutral = 0,0,0), the three material scripts (`ue_import_buildings.py`, `ue_import_scatter.py`, `ue_make_terrain_material.py`: add the `Wet` darkening/roughness term reading `MPC_World.Wet`).

**Interfaces — Produces:** `struct FSimAtmosphere { float Dust, Rain, Overcast, Wet, Wither; bool bFestival; FName Weather; }`; `static FSimAtmosphere USimAtmosphereSubsystem::Target(FName WeatherId, int32 Drought, const FString& Season, float Hour, bool bFestival)` — a pure function (unit-testable) mapping `weather.csv` ids (the ids in `weather.csv` today: `clear`, `hot`, `scorching`, `sandstorm`, `rain`, `fog`) to targets; `Tick` eases the live state toward the target (time constant 60 s, `Wet` rising with rain and drying over 2 in-game hours).

- [ ] Tests: `Target("clear", 0, "rains", 12, false)` is all zeros; `sandstorm` → Dust 1; `fog` → Overcast ≥ 0.5 and fog ×4 (dawn marsh fog); `scorching` → heat shimmer on (a screen-space distortion strength in `MPC_World.Shimmer`); `rain` → Rain 1, Overcast ≥ 0.6; drought 4 → Wither 1 and Dust ≥ 0.3 even on a clear day; the easing reaches 63 % in 60 s (± 5 %). FAIL → implement → PASS → before/after clear-noon diff < 1 % → commit.

### Task 2: rain, dust, smoke

**Files:** Create `tools/art/fx_gen.py` (Blender: a rain-streak quad, a dust-mote quad, a smoke card), `tools/art/ue_make_fx_materials.py` (`M_Rain`, `M_Dust`, `M_Smoke`: translucent/unlit, animation in WPO and UV panning, alpha from `MPC_World`), extend `SimAtmosphere.cpp` (camera-following instanced fields: 1,500 rain streaks in a 30 m box, 800 dust motes; smoke instances at every building spec's `soot` source, written by `building_grammar.py` into a new tracked `unreal/Content/Sim/smoke.csv` — `place_id,x_cm,y_cm,z_cm`, world frame, with a `--check` in CI).
**Interfaces:** smoke is on at dawn (05–08) for ovens (`bakery`, `temple_kitchens`, `cookshop`, `potter`) and at dusk (17–20) for every home with a hearth, continuous for `foundry`/`armourer`, off in rain.

- [ ] Tests (`Sim.Atmosphere.Fx`): instance counts; rain hidden when Rain = 0; smoke points match `smoke.csv` rows; the smoke schedule by hour. Python test: `smoke.csv` is current. FAIL → implement → PASS → iGPU shots: rain at dusk (force with `sim.Weather rain` — add the console command next to `sim.Drought` in `SimStreetConsole.cpp`), a dust storm at noon, dawn smoke over the bakery quarter → commit.

### Task 3: the night sky

**Files:** Create `tools/art/star_dome.py` (plain Python + Blender export), `tools/tests/test_art_stars.py`, `art/source/bsc5/` (the Yale Bright Star Catalogue 5th ed., public domain, fetched by `sources.py` — add a `url` adapter row with `LicenseRef-PublicDomain`; add that id to the licence allowlist in `license_gate.py` with a test), `unreal/.../SimSky*` changes in `SimDayNight` (a `UStaticMeshComponent` star dome rotated by sidereal time).
**Interfaces:** stars brighter than magnitude 5.0 (~1,600) as small emissive quads on a 900 m dome, size/brightness by magnitude, colour by B−V index; the zodiac: the 12 ecliptic constellations' line figures as thin emissive lines (a separate mesh, faint, toggled on by the D-025 astrology UI or when the player looks through the star terrace's gnomon — keep the toggle a console var for now: `sim.Zodiac 1`); the Milky Way as a soft band texture from a procedural galactic-plane noise (galactic pole RA 192.86°, Dec 27.13°); rotation: sidereal angle = `(SimDay × 360.9856 + Hour × 15.041)` degrees about the celestial pole tilted to latitude 31° N (pole toward −Y).

- [ ] Tests: the catalogue parses ≥ 1,500 stars under mag 5; Polaris (HR 424) maps to altitude 31° ± 1° toward −Y at any hour; Aldebaran/Regulus/Antares/Fomalhaut sit within 6° of the ecliptic; the dome mesh ≤ 20,000 tris. FAIL → implement → PASS → a midnight shot looking south (zodiac on) and north (the pole) → commit.

### Task 4: the drought and the festival, visible

**Files:** extend `SimAtmosphere.cpp`, `SimEnvironment.cpp` (water level: `WaterZ` lowered by `Drought × 30 cm` for the canals only — rebuild the water mesh on a drought change; the lagoon and the sea stay), `SimScatter.cpp` (a `festival` habitat: `flora.csv` rows `banner`, `garland`, `lamp_cluster` — code-built meshes in `flora_gen.py` — shown only when `bFestival`), `db/canon/flora.csv`.

- [ ] Tests: drought 4 → canal water 120 cm lower, dust ≥ 0.3, `Wither` 1; festival day (`festivals.csv` first row's date via `sim.AdvanceDays`) → festival instances visible, hidden the next day. FAIL → implement → PASS → shots: drought 0 vs 4 at the same noon view; a festival night at the precinct → commit.

### Task 5: sound

**Files:** Create `art/sounds.csv` (wishlist: `id,source,query_or_url,license,zone,loop,purpose`), a `freesound` adapter in `tools/art/sources.py` (search by the row's query with `filter=license:"Creative Commons 0" OR license:"Attribution"`, take the top result by rating whose duration fits, download the HQ preview or the original with OAuth if the key allows — record the exact sound id and author in the manifest), `tools/art/ue_import_sounds.py`, `Public/SimAmbience.h`, `Private/SimAmbience.cpp`, `Private/Tests/SimAmbienceTest.cpp`.
**Interfaces:** zones — `city_day`, `city_night`, `market` (@market_square, stalls), `smithy` (@smithy, foundry, armourer: hammer loop), `bakery` (crackle), `lagoon` (water lap; frogs at night), `desert_wind`, `precinct` (a distant chant at the hours of the rites — `rites.csv`), `harbour` (gulls, rigging); spot cues from `OnAnimalCue` (dog bark, donkey bray, sheep bleat, goose honk); `USimAmbienceSubsystem` crossfades zone beds by the player's position (nearest place typology / habitat within 30 m, else the quarter's default), 2 s fades, volumes from `USimGameUserSettings`'s ambient/effects sliders.

- [ ] Steps: wishlist (≥ 30 rows) → fetch with the designer's key (**stop and ask the designer to set `FREESOUND_API_KEY`** if missing — this is the one human step) → licence gate + credits → import → tests (`Sim.Ambience`: the zone chosen at 6 hand-picked points; a missing sound → silent + one warning; the volume follows the setting) → a smoke run with `-nullrhi` still passes (audio device may be null there — guard it) → commit.

### Task 6: records and push

- [ ] `docs/notes-for-tommy.md` (his `SimDayNight` gained `SetWeatherBlend`; neutral values unchanged; the new console commands `sim.Weather`, `sim.Zodiac`), HANDOFF "Stage V batch 5", `docs/proposals/invented-ledger-atmosphere.md`, `docs/credits.md` regenerated, `tools/art/README.md`, completion-plan progress. Fresh-context review → fix pass → push → CI green.

## Status (2026-09-26): tasks 1–4 and 6 done, task 5 (sound) half done — deferred by the designer (on Tommy's Windows box)

| Task | Commits | Verified by |
|---|---|---|
| 1 atmosphere driver | 61ef05c | `Sim.Atmosphere.Targets/Easing` (63 % in 60 s, never a snap, half dry after one in-game hour); clear noon vs before: `imgdiff.py` 0.59 % against a 0.57 % run-to-run floor; `vb5_rain_17.50_street.png`, `vb5_sandstorm_12.00_street.png` |
| 2 rain, dust, smoke | ff4f3e6 | `Sim.Atmosphere.Fx` (1,500 streaks, 800 motes, 5 puffs per smoke.csv row, the schedule, every fire out in the rain), `test_art_smoke.py`, `building_grammar.py --check-smoke` in CI; smoke shot |
| 3 night sky | 1dbddab | `test_art_stars.py` (≥ 1,500 stars under mag 5, Polaris 31 ± 1° north at any hour, the turn equals the true sky, Aldebaran/Regulus/Antares/Spica within 6° of the ecliptic, ≤ 20,000 tris), `Sim.Atmosphere.Sky`; `vb5_sky_00.00_south.png` (zodiac on), `vb5_sky_00.00_north.png` (Polaris at the centre) |
| 4 drought and festival | bee3b77 | `Sim.Terrain.CanalDrought` (stage 4: 120 cm lower; lagoon, river, sea unchanged), `Sim.Scatter` festival checks (up on a festival day, down the next, never blocking); `vb5_festival_drought0_*`, `vb5_nextday_drought4_*` (canal, precinct at noon and 20:00) |
| 5 sound | f6edbdc (part 1) | part 1: `art/sounds.csv` (34 rows, every zone a bed, six animal cues), the Freesound adapter (`test_art_sources.py`, offline). **Part 2 deferred by the designer:** fetch with `FREESOUND_API_KEY`, licence gate + credits, `ue_import_sounds.py`, `USimAmbience`, `Sim.Ambience` |
| 6 records | (this) | HANDOFF, notes for Tommy, `invented-ledger-atmosphere.md`, completion-plan, `tools/art/README.md` "Atmosphere", roadmap; `ue_test.sh` 78/78, `ue_smoke.sh` all ok |

The review was done inline against the plan's tests and shots, not by a separate fresh-context agent.

**Rulings made during execution** (each: what — why — cost if wrong):
- T1: an actor (`ASimAtmosphere`, spawned after the fauna), not `USimAtmosphereSubsystem` — the pattern of `ASimScatter` and `ASimFauna`, testable in a fresh world — none.
- T1: Tommy's `ASimDayNight` gained one entry point, `SetWeatherBlend(Dust, Rain, Overcast, Fog)`; with all zeros his values are untouched (imgdiff within the run-to-run noise) — none.
- T1: MPC_World's parameters are only ever added, never replaced (replacing re-ids them and breaks M_Terrain) — none.
- T1: `M_Building` has no wet term yet — a separate session is fixing its colour pin bug (task_39bf96a2); add the term after it lands — wet streets beside dry walls in the rain.
- T2: rain and dust are instanced cards moved by World Position Offset in a box riding with the camera, not Niagara — no Niagara assets to author headless, testable instance counts — less rich than particles.
- T2: the smoke sources are a tracked `Content/Sim/smoke.csv` written by `building_grammar.py` (`--check-smoke` in CI) — the same data-first rule as scatter.csv — none.
- T3: the sky is `Content/Sim/stars.csv` drawn as instanced cards (`/Engine/BasicShapes/Plane`, `M_Star`), not a Blender-exported dome mesh — no glTF axis guesswork: the Python test proves the very rotation the C++ applies (`SIDEREAL_SIGN`) — none.
- T3: the dome lives in `ASimAtmosphere`, not in Tommy's `SimDayNight` — keeps his file to the one blend call — none.
- T3: Spica instead of Fomalhaut in the ecliptic test — Fomalhaut lies about 21° off the ecliptic; the plan's list was a slip — none.
- T3: `M_Star` is additive and unfogged (`use_translucency_vertex_fog` off) — at 900 m the height fog swallowed it — none.
- T3: the sky's day is the kernel's day (`GetSimDayFor`) — none.
- T4: the canal water is its own mesh, moved down on a drought change, not rebuilt — the same result for no rebuild — none.
- T4: festival rows use the season token `festival` in `flora.csv` (the flora test allows it only with the `festival` habitat); festival props never collide — they stand hidden most days — none.
- T4: the lamps glow by a colour key in `M_Scatter` (the palette's new `flame_0`, sRGB R − B > 0.75, no other palette colour comes near) — no second material slot on the scatter meshes — a future palette colour that red may glow.
- T5: the wishlist's licence column is the accepted set (CC0, CC BY 4.0, CC BY 3.0); the manifest records the actual one per sound. HQ previews (ogg), not the originals (those need OAuth) — lower fidelity.

**Deferred minors:**
- **Task 5 part 2 (sound)** waits for the designer: set `FREESOUND_API_KEY` (freesound.org/apiv2/apply; `setx FREESOUND_API_KEY "<key>"` on Windows), then `python3 tools/art/sources.py --sounds`, the licence gate and credits, `ue_import_sounds.py`, `SimAmbience` and `Sim.Ambience` per the plan above, a `-nullrhi` smoke run.
- Heat shimmer is only an MPC parameter; no screen distortion yet.
- The GPU cost of the effects was not measured (`stat gpu`).
- The festival lamps are small at the precinct shot's distance; the banners hang still (no wind term: their A is 0).
- The canal water jumps 30 cm on a drought change (no easing).
- The Milky Way's blobs read as soft cloud up close; a band texture would be finer.
- `docs/capi-reach.md` was stale from T1 to T4 (the new weather and festival calls); regenerated in T4.
