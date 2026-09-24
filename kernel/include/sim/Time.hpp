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

struct CalendarConfig {
    int days_per_month = 30;
    int months_per_year = 12;
    std::vector<std::string> month_names;  // empty => months are unnamed numbers
    std::vector<SeasonDef> seasons;        // empty => no seasons
    std::vector<DayNumber> festival_days;  // absolute day numbers, from canon
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
    bool is_festival(DayNumber day) const;

private:
    CalendarConfig cfg_;
    const std::string empty_;
};

}  // namespace sim
