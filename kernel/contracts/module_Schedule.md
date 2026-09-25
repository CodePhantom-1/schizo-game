# MODULE CONTRACT — Schedule

**Status:** T2 agent. The NPC day planner — schedules are tasks bent by season, not fixed positions
(docs/mechanics.md row 7; ../docs/living-world.md §2; ../docs/city-life.md §1).

**Owns:** nothing. Pure functions over `db/canon/schedules.csv` + `Calendar`; no state, so nothing to save.
**May read:** `Db`, `Calendar` (both coordinator-owned contracts). No `WorldContext` needed — the query
surface is narrower than a WorldContext consumer, so it takes only what it reads.
**Must never:** write state, hold its own Rng, use wall-clock time, touch engine code, resolve an OPEN row.

**Definition of done:** `src/Schedule.cpp` implements every declaration in `include/sim/Schedule.hpp`;
`tests/test_schedule.cpp` passes; determinism holds (same inputs -> same outputs).

## API

```cpp
std::vector<std::string> schedule_roles(const Db& db);
std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role, DayNumber day);
std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role, DayNumber day, int hour);
```

Role matching is case-insensitive and trims whitespace. OPEN-tagged rows are always skipped (content
policy: never surface an unresolved row). Rows apply on a given day when their `season` field is blank
(all seasons) or equals `cal.season_id(day)`; when a seasonal row and an all-season row for the same role
share an hour, the seasonal row wins (`Schedule.cpp` keeps a `bool` per chosen hour to arbitrate this).
`task_at` returns the plan row with the greatest `hour <= hour`; if the queried hour is before the day's
first row, it walks back day by day (bounded at 366 days, and never below day 1 — `Calendar`'s own day
numbering starts at 1, Time.hpp) picking up the previous day's last task, so a role's night-hours task
always resolves to something rather than nullopt. `nullopt` is returned only when the role has no rows
in canon at all (checked via `schedule_roles`, not via an empty `day_plan` for the queried day/season).

## INVENTED constants

None. This module invents no machinery constants of its own — it is pure lookup logic over
`schedules.csv`, whose 24 rows are already tagged `INVENTED` (D-012 authored glue) at the content layer.

## Festival-override hook

Festival days are `OPEN` in canon (`calendar.csv`: `festival_days` is an open row) — per the content
policy this module never resolves an OPEN row, so festival overrides are **documented, not built**:
`Schedule.hpp`'s header comment notes that when festival schedules are authored, `day_plan` should
consult a `festival` column (or a dedicated table) the same way it consults `season` today. No behaviour
change ships for `cal.is_festival(day)` in this module.

## Content gap for the coordinator (not authored here — do not fill)

`db/canon/people.csv` currently has only 3 rows (the three named leaders: `law_giver`, `the_prophet`,
`the_warchief`), and their `role` column holds a narrative title/description
("founder of the Empire; the player's previous incarnation (implied)", etc.), not a schedule-matching
role string. **None of the 19 schedule roles** (baker, miller, water-carrier, herdsman, gatekeeper,
priest of the sun, paladin, fisherman, field hand, scribe, market trader, lighthouse keeper, craftsman,
"workshops and households", cook-shop keeper, dockworker, "household", priest of the moon, watchman)
currently have any matching row in `people.csv` — i.e. the populace that would actually run these
schedules does not exist yet as named or generic NPCs with a `role` field the planner could key off of.
This is a content/Population-module gap, not something to author here.

## Tests (tests/test_schedule.cpp)

The real `db/canon/schedules.csv` has zero seasonal rows (every row's `season` is blank), so the
seasonal-bend, same-hour-tie-break, and season-boundary carry-over paths are exercised against a
throwaway fixture canon written to the system temp dir at test time (never committed as canon) —
the same pattern `test_justice.cpp` uses for its laws.csv fixture. Role-matching case/whitespace
handling, the unknown-role nullopt path, the OPEN-row skip, determinism, and "every schedule row is
reachable through `day_plan`" are each checked against both the fixture and the real canon (the latter
additionally proves the real 24-row table is fully reachable through this module).
