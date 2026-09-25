# MODULE CONTRACT — Time

**Status:** IMPLEMENTED (Wave 0). The calendar spine.

**Owns:** Date/DayNumber math, season lookup, festival days
**May read:** anything through WorldContext (const) + its own db tables.
**Must never:** write another module's state; hold its own Rng; use wall-clock time; touch engine code; resolve an OPEN row.

**Definition of done:** src/Time.cpp implements every declaration in include/sim/Time.hpp; tests/test_time.cpp passes; determinism holds (same seed -> same state bytes).

## Festivals (track K-2)

The calendar answers **"is day D a festival, and which one"**:

```cpp
struct FestivalDef { Id id; std::string name; int day_of_year; };
CalendarConfig::festivals                         // yearly named festivals
CalendarConfig::festival_days                     // absolute one-off days (anonymous; pre-K-2, kept)
bool Calendar::is_festival(DayNumber) const;      // yearly day_of_year match OR absolute day
const FestivalDef* Calendar::festival_on(DayNumber) const;   // nullptr when none / anonymous
const std::string& Calendar::festival_id(DayNumber) const;   // "" when none / anonymous
const std::vector<FestivalDef>& Calendar::festivals() const; // sorted by day_of_year
```

- Yearly festivals recur on the same `day_of_year` every year. The constructor throws
  `std::invalid_argument` on a `day_of_year` outside `1..days_per_year()` or two festivals on the
  same day (one festival per day keeps `festival_id` a single answer).
- `WorldState::init` fills `festivals` from **`db/canon/festivals.csv`** (OPEN rows skipped) — the
  machine-readable form of `calendar.csv`'s `festival_days` row, which D-018 already filled as
  `INVENTED` (invented-ledger-design #1). The months stay unnamed in the kernel (`calendar.csv`'s
  `month_names` row is not wired by K-2).
- **Magic's time power** (`Magic.hpp`: +0.10 when a rite whose `time_window` names a festival is
  performed on a festival day) reads `ctx.cal.is_festival(ctx.day)`; it is now live against canon
  with no change to Magic. **Events' `festival` trigger** likewise; `festival:<id>` narrows it to one
  named festival (module_Events.md).
- The festival's bend on the day (gathering window, place, exempt roles, market) lives on the same
  `festivals.csv` row and is read by the Schedule module (module_Schedule.md), not by Time.
- No new state: the calendar is rebuilt from canon on every `init`/`load_world`, so festivals
  round-trip through a save by construction (`tests/test_festivals.cpp` checks it).

Tests: `tests/test_time.cpp` (`test_yearly_festivals`, `test_bad_festival_config_throws`);
`tests/test_festivals.cpp` (real canon on the calendar, the street on each festival, Magic's time
power from a festivals.csv fixture, save/load, the C API).
