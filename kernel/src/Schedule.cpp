#include "sim/Schedule.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string normalize(const std::string& s) {
    std::string t = trim(s);
    std::transform(t.begin(), t.end(), t.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return t;
}

bool is_open(const Row& row) { return trim(row.get("tag")) == "OPEN"; }

}  // namespace

std::vector<std::string> schedule_roles(const Db& db) {
    std::map<std::string, std::string> by_key;  // normalized -> first-seen casing
    for (const Row& row : db.rows("schedules")) {
        if (is_open(row)) continue;
        const std::string role = trim(row.get("role"));
        if (role.empty()) continue;
        by_key.emplace(normalize(role), role);
    }
    std::vector<std::string> out;
    out.reserve(by_key.size());
    for (auto& [key, role] : by_key) out.push_back(role);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role,
                                     DayNumber day) {
    const std::string wanted = normalize(role);
    const std::string season_now = cal.season_id(day);

    std::map<int, ScheduledTask> by_hour;  // hour -> chosen row (seasonal beats all-season)
    std::map<int, bool> seasonal_at_hour;

    for (const Row& row : db.rows("schedules")) {
        if (is_open(row)) continue;
        if (normalize(row.get("role")) != wanted) continue;
        const std::string season = trim(row.get("season"));
        const bool is_seasonal = !season.empty();
        if (is_seasonal && season != season_now) continue;

        int hour = 0;
        try {
            hour = std::stoi(row.get("hour", "0"));
        } catch (...) {
            continue;  // malformed hour: skip rather than crash on canon data
        }

        auto it = by_hour.find(hour);
        if (it != by_hour.end() && seasonal_at_hour[hour] && !is_seasonal) {
            continue;  // an existing seasonal row wins over an all-season one
        }
        ScheduledTask t;
        t.schedule_id = row.get("id");
        t.role = trim(row.get("role"));
        t.hour = hour;
        t.task = row.get("task");
        by_hour[hour] = t;
        seasonal_at_hour[hour] = is_seasonal;
    }

    std::vector<ScheduledTask> out;
    out.reserve(by_hour.size());
    for (auto& [hour, task] : by_hour) out.push_back(task);
    return out;
}

std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role,
                                      DayNumber day, int hour) {
    const std::string wanted = normalize(role);
    bool role_known = false;
    for (const std::string& r : schedule_roles(db)) {
        if (normalize(r) == wanted) {
            role_known = true;
            break;
        }
    }
    if (!role_known) return std::nullopt;

    DayNumber d = day;
    int h = hour;
    // Bounded walk-back: a known role always has at least one row somewhere,
    // and Calendar's own day numbering starts at 1 (Time.hpp), so we stop
    // there rather than call the calendar with an out-of-contract day.
    for (int steps = 0; steps <= 366 && d >= 1; ++steps) {
        const std::vector<ScheduledTask> plan = day_plan(db, cal, role, d);
        const ScheduledTask* best = nullptr;
        for (const ScheduledTask& t : plan) {
            if (t.hour <= h && (best == nullptr || t.hour > best->hour)) best = &t;
        }
        if (best != nullptr) return *best;
        d -= 1;
        h = 23;
    }
    return std::nullopt;
}

}  // namespace sim
