#include "sim/Time.hpp"

#include <algorithm>
#include <stdexcept>

namespace sim {
namespace {
const std::string kEmptyName;  // unnamed months/seasons are a canon gap, not an error
}  // namespace

Calendar::Calendar(const CalendarConfig& cfg) : cfg_(cfg) {
    if (cfg_.days_per_month <= 0 || cfg_.months_per_year <= 0)
        throw std::invalid_argument("calendar shape must be positive");
    if (cfg_.month_names.empty()) {
        cfg_.month_names.resize(static_cast<std::size_t>(cfg_.months_per_year), "");
    } else if (static_cast<int>(cfg_.month_names.size()) != cfg_.months_per_year) {
        throw std::invalid_argument("month_names size must equal months_per_year");
    }
    // Seasons are looked up by "last start_day_of_year <= day_of_year".
    std::sort(cfg_.seasons.begin(), cfg_.seasons.end(),
              [](const SeasonDef& a, const SeasonDef& b) {
                  return a.start_day_of_year < b.start_day_of_year;
              });
}

int Calendar::days_per_year() const { return cfg_.days_per_month * cfg_.months_per_year; }

Date Calendar::to_date(DayNumber day) const {
    if (day < 1) throw std::invalid_argument("day numbering starts at 1");
    const int dpy = days_per_year();
    const std::int64_t idx = day - 1;
    Date d;
    d.year = static_cast<int>(idx / dpy) + 1;
    const std::int64_t within_year = idx % dpy;
    d.month = static_cast<int>(within_year / cfg_.days_per_month) + 1;
    d.day = static_cast<int>(within_year % cfg_.days_per_month) + 1;
    return d;
}

DayNumber Calendar::day_number(const Date& d) const {
    const int dpy = days_per_year();
    return static_cast<DayNumber>(d.year - 1) * dpy +
           static_cast<DayNumber>(d.month - 1) * cfg_.days_per_month + d.day;
}

int Calendar::day_of_year(DayNumber day) const {
    const int dpy = days_per_year();
    return static_cast<int>((day - 1) % dpy) + 1;
}

const std::string& Calendar::season_id(DayNumber day) const {
    if (cfg_.seasons.empty()) return kEmptyName;
    const int doy = day_of_year(day);
    const SeasonDef* best = nullptr;
    for (const SeasonDef& s : cfg_.seasons)
        if (s.start_day_of_year <= doy) best = &s;
    return best ? best->id : cfg_.seasons.back().id;  // wrap: days before the first start fall in the last season
}

const std::string& Calendar::month_name(const Date& d) const {
    const std::size_t i = static_cast<std::size_t>(d.month - 1);
    return i < cfg_.month_names.size() ? cfg_.month_names[i] : kEmptyName;
}

bool Calendar::is_festival(DayNumber day) const {
    return std::binary_search(cfg_.festival_days.begin(), cfg_.festival_days.end(), day);
}

}  // namespace sim
