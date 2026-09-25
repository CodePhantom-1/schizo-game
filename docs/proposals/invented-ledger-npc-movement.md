# Invented ledger — UE-4, the street lives: townspeople and the sun

Per D-018: every invented choice below fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto supersedes here).

## The roster

`ASimNpcDirector` spawns one `ASimNpc` for every kernel npc whose `home_city` is `city_of_the_moon` **and** whose `role` resolves to a schedules.csv role (`sim_world_npc_role` is non-empty). That's the 29 light residents from `docs/proposals/invented-ledger-street.md`, minus `the_prophet` (a named leader living in the City of the Moon with a narrative title, not a schedule role — correctly excluded).

## The task -> destination join (INVENTED glue)

The C API gives two raw facts per hour: `sim_world_npc_task` (the schedule row in force) and `sim_world_npc_place` (the first `places.csv` row with `owner_person_id == npc_id`). Neither says *where a resident should stand*, so `ASimNpcDirector::ResolveDestination` derives it with a small, documented rule set — this is the "data-light form" the track brief asked for, not new canon.

**Day (hour 6..20, i.e. not night — see below):**
1. Explicit role -> place table, for roles the kernel data documents as *worked* rather than *owned* (people.csv's own `source_ref` column says "works places.csv X" for these three), plus the track's own literal examples:
   | role | day place | why |
   |---|---|---|
   | `gatekeeper` | `moon_gate_place` | spec: "gate for gatekeeper" |
   | `watchman` | `gate_watch_post_place` | spec: "watch post for watchmen" |
   | `market trader` | `market_square_place` | spec: "market for trader" — overrides the trader's own stall (see ceiling below) |
   | `water-carrier` | `street_well_place` | spec: "the well for water-carriers" |
   | `tavern keeper` | `brewery_tavern_place` | people.csv: "works places.csv brewery_tavern_place" |
   | `cook-shop keeper` | `cookshop_place` | people.csv: "works places.csv cookshop_place" |
   | `priest of the moon` | `temple_front_place` | people.csv: "works places.csv temple_front_place" |
2. Otherwise: the resident's own place (`sim_world_npc_place`) — spec: "own place for work tasks (bakery for baker...)". Covers baker, miller, brewer, scribe, craftsman (potter/weaver), and household (their house, all day — they never left).
3. Otherwise (no owned place either): a deterministic fallback spot (below).

**Night (hour >= 21, or hour < 6 — schedules.csv's last evening rows: `evening_meal_and_lamps` hour 20, `watchmen_night_patrol` hour 21; the kernel's schedule rows carry no day/night flag, so the director owns this boundary):**
- The resident's own owned place, or, failing that, the deterministic fallback.
- **Known ceiling:** a resident's one owned `places.csv` row is sometimes their *workplace*, not a separate home (e.g. a market trader owns only their stall; the baker owns both `bakery_place` and `bakers_house_place`, but `sim_world_npc_place` returns the first CSV match — `bakery_place` — so the baker "sleeps" at the bakery in this build too). Upgrade path: extend `sim_world_npc_place` with a place-kind filter (or add a `kind=house` lookup) once a resident needs a home distinct from their first-listed place; not built now because only the baker's own data currently has two owned rows to disambiguate.

## The place-location fallback (ASimStreetBuilder doesn't exist on this branch)

`ASimNpcDirector::ResolvePlaceLocation` tries, in order:
1. An actor tagged `Place:<id>` in the level (the future join to the street agent's registry, `ASimStreetBuilder::GetPlaceLocation(FName)` — not present in this worktree at rebase time).
2. A deterministic hash of the id (`FCrc::StrCrc32`) mapped onto the grey-box street's known extent (`SimGameMode::StartPlay`'s two floor segments, X roughly `[-2000, 4500]`, Y `[-500, 500]`). Same id always lands in the same spot, so residents don't teleport between runs even without a real street layout.

When the street agent's registry lands, swapping in real tagged-actor locations needs no change here — the tag lookup already runs first.

## The day/night sun

`ASimDayNight` maps `USimWorldSubsystem::GetSimHour()` (0..24) to a directional-light pitch at 15 degrees/hour (`Pitch = Hour * 15 - 90`): hour 6 = horizon (sunrise), hour 12 = zenith (noon), hour 18 = horizon (sunset), hour 0/24 = straight up (midnight, below the horizon on the far side). No canon sun-path model exists to source this from — it's the standard 24-hour linear sweep. Night (`hour < 6 || hour >= 18`) drops the directional light to `0.05` and the SkyLight fill to `0.02` (real darkness, moonlight only, per the immersion rule); day runs `8.0`/`1.0`.

## Registration

- `docs/DECISIONS.md`: no new decision needed — this is glue under D-018's existing creative-gap-filling authorization, not a new system.
- No new canon tables or columns; every function reads only kernel data that already ships (`people.csv`, `places.csv`, `schedules.csv` via the C API additions in `kernel/include/sim/CApi.h`).
