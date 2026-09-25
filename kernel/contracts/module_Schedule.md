# MODULE CONTRACT — Schedule

**Status:** T2 agent; festivals + per-person resolution added by track K-2. The NPC day planner —
schedules are tasks bent by season and festival, not fixed positions (docs/mechanics.md rows 7 and 14;
../docs/living-world.md §2; ../docs/city-life.md §1, §5).

**Owns:** nothing. Pure functions over `db/canon/schedules.csv`, `person_schedules.csv`,
`festivals.csv`, `people.csv` (home/work places) + `Calendar`; no state, so nothing to save.
**May read:** `Db`, `Calendar` (both coordinator-owned contracts). No `WorldContext` needed — the query
surface is narrower than a WorldContext consumer, so it takes only what it reads.
**Must never:** write state, hold its own Rng, use wall-clock time, touch engine code, resolve an OPEN row.

**Definition of done:** `src/Schedule.cpp` implements every declaration in `include/sim/Schedule.hpp`;
`tests/test_schedule.cpp` and `tests/test_festivals.cpp` pass; determinism holds (same inputs -> same outputs).

## API

```cpp
struct ScheduledTask { Id schedule_id; std::string role; int hour; std::string task;
                       std::string place; Id festival; std::string origin; };
struct FestivalBend  { Id festival_id; std::string name; Id deity; std::string place;
                       int attend_from, attend_until; std::vector<std::string> exempt_roles;
                       bool market_open; std::string gathering; };

std::vector<std::string> schedule_roles(const Db& db);
std::vector<ScheduledTask> day_plan(const Db&, const Calendar&, const std::string& role, DayNumber day);
std::optional<ScheduledTask> task_at(const Db&, const Calendar&, const std::string& role, DayNumber day, int hour);
std::optional<FestivalBend> festival_bend(const Db&, const Calendar&, DayNumber day);
bool market_open(const Db&, const Calendar&, DayNumber day);
std::vector<ScheduledTask> person_day_plan(const Db&, const Calendar&, const Id& person_id, const std::string& role, DayNumber day);
std::optional<ScheduledTask> person_task_at(const Db&, const Calendar&, const Id& person_id, const std::string& role, DayNumber day, int hour);
```

`ScheduledTask` gained `place`, `festival` and `origin` (additive, defaulted — no existing caller
changed). `Population.hpp`'s `npc_task_at` now delegates to `person_task_at` with the npc's own role.

## Resolution rules

Role matching is case-insensitive and trims whitespace. OPEN-tagged rows are always skipped (content
policy: never surface an unresolved row) — in all three tables.

**Filters.** A row applies on `day` when its `season` is blank or equals `cal.season_id(day)`, and its
`festival` is blank, or — on a festival day — `any` or equal to `cal.festival_id(day)`
(case-insensitive). A festival row never applies on an ordinary day.

**Ranking per hour.** Candidates are the role's `schedules.csv` rows plus (person-level queries only)
the person's `person_schedules.csv` rows. At each hour the highest rank wins:

| rank | row |
|---|---|
| 7 | person row for the named festival |
| 6 | person row for `any` festival |
| 5 | role row for the named festival |
| 4 | role row for `any` festival |
| 3 | person row, seasonal |
| 2 | person row, all-season |
| 1 | role row, seasonal |
| 0 | role row, all-season |

Equal rank: the later row in file order wins (the pre-K-2 behaviour). So festival rows beat
ordinary rows, person rows beat role rows, and the narrower filter beats the wider one — the
seasonal-beats-all-season rule of T2 is the rank 1 > rank 0 case.

**The festival gathering.** On a day whose named festival has a `festivals.csv` row with a valid
`[attend_from, attend_until)` window, and whose role is not in the row's `exempt_roles`
(`;`-separated, trimmed, case-insensitive), and which has at least one row that day:
1. ordinary rows (rank < 4) with `attend_from <= hour < attend_until` are removed;
2. unless a festival row already sits at `attend_from`, a gathering task is inserted there
   (`schedule_id` = the festival id, `task` = the row's `gathering`, `place` = the row's `place`,
   `origin` = `"festival"`);
3. unless `attend_until` is 24 or a row already sits there, the ordinary task that was in force at
   `attend_until` (the last ordinary row at or before it, wrapping to the day's last ordinary row)
   resumes at `attend_until`.
A malformed window (non-integer hours, `from` outside 0..23, `until` not in `from+1..24`) disables
the gathering but keeps the rest of the bend (market flag, festival rows).

**Market.** `market_open` is false only when today's festival row says `market=closed`.

**Places.** `place` is `""` (unplaced), `home`, `work`, or a `places.csv` id. Role-level queries
(`day_plan`, `task_at`) return the raw token; person-level queries resolve `home`/`work` through the
person's `people.csv` `home_place`/`work_place` (`""` when the person has none, i.e. the person
lives or works off the slice street).

**Carry-over.** `task_at`/`person_task_at` return the plan row with the greatest `hour <= hour`; before
the day's first row they walk back day by day (bounded at 366 days, never below day 1) — each earlier
day planned with its own season and festival — picking up that day's last task. `nullopt` only when
the role (and, person-level, the person) has no rows at all, or when no row ever applies.

## INVENTED constants

None in code. The rank order and gathering-window semantics are the imported "festival overrides"
of mechanics.md row 7 made concrete; every placeholder content row they act on (festivals, festival
schedule rows, per-person overrides, home/work places) is tagged `INVENTED` and listed in
[docs/proposals/invented-ledger-festivals.md](../../docs/proposals/invented-ledger-festivals.md).

## Tests

- `tests/test_schedule.cpp` — T2's original suite (fixture canon without festivals: unchanged
  behaviour) plus a festival fixture: the ordinary day is untouched, the gathering replaces and
  resumes, named vs `any` festival rows, exempt roles, OPEN rows, a malformed window, per-person
  overrides beating role rows, per-person festival rows beating the gathering, place resolution,
  determinism; and every real-canon schedule/person-schedule row and every festival gathering is
  reachable with the calendar exactly as `WorldState::init` builds it.
- `tests/test_festivals.cpp` — the real canon end to end (see module_Time.md).
