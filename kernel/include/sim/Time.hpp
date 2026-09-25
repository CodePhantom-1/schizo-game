#pragma once
// Time.hpp — the calendar spine (contract, coordinator-owned, IMPLEMENTED).
// Every module learns "when it is" only from a Calendar. The calendar's shape
// is data-driven: seasons come from db/canon/seasons.csv (D-015), yearly
// festivals from db/canon/festivals.csv (INVENTED, D-018 / K-2), and the
// default shape (12 months x 30 days = 360) is an INVENTED abstraction the
// codex will show as such.
#include "sim/Types.hpp"

#include <string>
#include <vector>

namespace sim {

struct SeasonDef {
    Id id;                     // season id (name OPEN in calendar.csv)
    int start_day_of_year = 1; // 1-based day of year on which it begins
};

// A yearly festival (db/canon/festivals.csv, K-2): it falls on the same
// day_of_year every year. Its schedule bend (gathering place/hours, market)
// is read from the same row by Schedule.hpp; the Calendar only answers
// "is day D a festival, and which one".
struct FestivalDef {
    Id id;
    std::string name;
    int day_of_year = 1;  // 1-based, 1..days_per_year
};

// The moon (A14, D-024 §7): the month is lunar. Day 1 is the new crescent,
// the middle day (15 of 30) is full; the phase is pure day-of-month
// arithmetic, so it needs no state and replays identically (D-022).
enum class MoonPhase { New, Waxing, Full, Waning };
const char* moon_phase_id(MoonPhase p);  // "new" | "waxing" | "full" | "waning"

// A day of every month with a name, an omen and/or a deity
// (db/canon/calendar_days.csv): the eššešu moon days, the unfavourable days of
// the hemerologies. Same day_of_month in every month.
struct CalendarDayDef {
    Id id;
    std::string name;
    int day_of_month = 1;  // 1..days_per_month
    std::string omen;      // "favourable" | "unfavourable" | ""
    std::string deity;     // deity id honoured that day, "" when none
};

struct CalendarConfig {
    int days_per_month = 30;
    int months_per_year = 12;
    std::vector<std::string> month_names;  // empty => months are unnamed numbers
    std::vector<SeasonDef> seasons;        // empty => no seasons
    std::vector<DayNumber> festival_days;  // absolute one-off festival days (anonymous)
    std::vector<FestivalDef> festivals;    // yearly named festivals (festivals.csv)
    std::vector<CalendarDayDef> month_days;  // calendar_days.csv (A14)
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
    // True on a yearly festival's day_of_year, or on an absolute festival_days entry.
    bool is_festival(DayNumber day) const;
    // The yearly festival falling on `day`, or nullptr (an anonymous absolute
    // festival day has no def). At most one festival per day_of_year.
    const FestivalDef* festival_on(DayNumber day) const;
    // festival_on(day)->id, or "" when the day has no named festival.
    const std::string& festival_id(DayNumber day) const;
    // Every yearly festival, sorted by day_of_year.
    const std::vector<FestivalDef>& festivals() const { return cfg_.festivals; }

    MoonPhase moon_phase(DayNumber day) const;
    // Integer 0..100: 0 at the new crescent, 100 at full (D-022: no floats).
    int moon_illumination(DayNumber day) const;
    // The calendar_days row for this day of the month, or nullptr.
    const CalendarDayDef* month_day(DayNumber day) const;

private:
    CalendarConfig cfg_;
};

}  // namespace sim
