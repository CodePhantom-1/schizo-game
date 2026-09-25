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
    std::sort(cfg_.festival_days.begin(), cfg_.festival_days.end());  // binary_search below
    // Yearly festivals: sorted by day_of_year, one per day, inside the year.
    std::sort(cfg_.festivals.begin(), cfg_.festivals.end(),
              [](const FestivalDef& a, const FestivalDef& b) {
                  return a.day_of_year < b.day_of_year;
              });
    for (std::size_t i = 0; i < cfg_.festivals.size(); ++i) {
        const int doy = cfg_.festivals[i].day_of_year;
        if (doy < 1 || doy > days_per_year())
            throw std::invalid_argument("festival day_of_year outside the year");
        if (i > 0 && cfg_.festivals[i - 1].day_of_year == doy)
            throw std::invalid_argument("two festivals on the same day_of_year");
    }
    std::sort(cfg_.month_days.begin(), cfg_.month_days.end(),
              [](const CalendarDayDef& a, const CalendarDayDef& b) {
                  return a.day_of_month < b.day_of_month;
              });
    for (std::size_t i = 0; i < cfg_.month_days.size(); ++i) {
        const int dom = cfg_.month_days[i].day_of_month;
        if (dom < 1 || dom > cfg_.days_per_month)
            throw std::invalid_argument("calendar day outside the month");
        if (i > 0 && cfg_.month_days[i - 1].day_of_month == dom)
            throw std::invalid_argument("two calendar days on the same day_of_month");
    }
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
    if (festival_on(day) != nullptr) return true;
    return std::binary_search(cfg_.festival_days.begin(), cfg_.festival_days.end(), day);
}

const FestivalDef* Calendar::festival_on(DayNumber day) const {
    if (cfg_.festivals.empty() || day < 1) return nullptr;
    const int doy = day_of_year(day);
    const auto it = std::lower_bound(
        cfg_.festivals.begin(), cfg_.festivals.end(), doy,
        [](const FestivalDef& f, int d) { return f.day_of_year < d; });
    return (it != cfg_.festivals.end() && it->day_of_year == doy) ? &*it : nullptr;
}

const std::string& Calendar::festival_id(DayNumber day) const {
    const FestivalDef* f = festival_on(day);
    return f ? f->id : kEmptyName;
}

const char* moon_phase_id(MoonPhase p) {
    switch (p) {
        case MoonPhase::New: return "new";
        case MoonPhase::Waxing: return "waxing";
        case MoonPhase::Full: return "full";
        case MoonPhase::Waning: return "waning";
    }
    return "new";
}

MoonPhase Calendar::moon_phase(DayNumber day) const {
    const int dom = to_date(day).day;
    const int full = cfg_.days_per_month / 2;
    if (dom == 1) return MoonPhase::New;
    if (dom < full) return MoonPhase::Waxing;
    if (dom == full) return MoonPhase::Full;
    return MoonPhase::Waning;
}

int Calendar::moon_illumination(DayNumber day) const {
    const int dom = to_date(day).day;
    const int dpm = cfg_.days_per_month;
    const int full = dpm / 2;
    if (full <= 1) return 100;  // degenerate shapes: always lit
    if (dom <= full) return (dom - 1) * 100 / (full - 1);
    return (dpm + 1 - dom) * 100 / (dpm + 1 - full);
}

const CalendarDayDef* Calendar::month_day(DayNumber day) const {
    const int dom = to_date(day).day;
    const auto it = std::lower_bound(cfg_.month_days.begin(), cfg_.month_days.end(), dom,
        [](const CalendarDayDef& d, int v) { return d.day_of_month < v; });
    return (it != cfg_.month_days.end() && it->day_of_month == dom) ? &*it : nullptr;
}

}  // namespace sim
