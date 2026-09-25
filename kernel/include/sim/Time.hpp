#pragma once
// Time.hpp — the calendar spine (contract, coordinator-owned, IMPLEMENTED).
// Every module learns "when it is" only from a Calendar. The calendar's shape
// is data-driven; the canon gives no month or season names, so names come from
// db/canon/calendar.csv (OPEN rows) and the default shape (12 months x 30 days
// = 360) is an INVENTED abstraction the codex will show as such.
#include "sim/Types.hpp"

#include <string>
#include <vector>

namespace sim {

struct SeasonDef {
    Id id;                     // season id (name OPEN in calendar.csv)
    int start_day_of_year = 1; // 1-based day of year on which it begins
};

// A named festival (db/canon/festivals.csv, D-018 INVENTED glue). Repeats
// every year on the same day_of_year — the festival calendar has no
// per-year exceptions.
struct FestivalDef {
    Id id;
    int day_of_year = 1;  // 1-based, 1..days_per_year
    std::string name;
};

struct CalendarConfig {
    int days_per_month = 30;
    int months_per_year = 12;
    std::vector<std::string> month_names;  // empty => months are unnamed numbers
    std::vector<SeasonDef> seasons;        // empty => no seasons
    // Day-of-year values a festival falls on (1..days_per_year); a day whose
    // day_of_year() appears here is a festival day EVERY year (not just year
    // 1). Historically these were absolute day numbers, which is identical
    // for any date in year 1 — existing callers that set this from year-1
    // constants keep working unchanged.
    std::vector<DayNumber> festival_days;
    std::vector<FestivalDef> festivals;    // optional: names for festival_id()/festival_name()
};

class Calendar {
public:
    Calendar() = default;
    explicit Calendar(const CalendarConfig& cfg);

    Date to_date(DayNumber day) const;           // day 1 => year 1, month 1, day 1
    DayNumber day_number(const Date& d) const;
    int day_of_year(DayNumber day) const;        // 1-based
    int days_per_year() const;

    // Season id for this day, or "" when no seasons are configured.
    const std::string& season_id(DayNumber day) const;
    // Month name, or "" when months are unnamed.
    const std::string& month_name(const Date& d) const;
    // True when day's day-of-year matches a configured festival day (repeats
    // every year).
    bool is_festival(DayNumber day) const;
    // The festival's id/name on `day`, or "" when today isn't a festival day
    // or no FestivalDef names that day_of_year (festival_days set without
    // festivals, e.g. in tests).
    const std::string& festival_id(DayNumber day) const;
    const std::string& festival_name(DayNumber day) const;

private:
    CalendarConfig cfg_;
};

}  // namespace sim
