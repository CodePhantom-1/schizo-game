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

namespace {

// The role's non-OPEN rows — one table scan; task_at reuses it across days.
std::vector<const Row*> role_rows(const Db& db, const std::string& role) {
    const std::string wanted = normalize(role);
    std::vector<const Row*> out;
    for (const Row& row : db.rows("schedules"))
        if (!is_open(row) && normalize(row.get("role")) == wanted) out.push_back(&row);
    return out;
}

std::vector<ScheduledTask> plan_from_rows(const std::vector<const Row*>& rows,
                                          const std::string& season_now) {
    std::map<int, ScheduledTask> by_hour;  // hour -> chosen row (seasonal beats all-season)
    std::map<int, bool> seasonal_at_hour;

    for (const Row* rp : rows) {
        const Row& row = *rp;
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

}  // namespace

std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role,
                                     DayNumber day) {
    return plan_from_rows(role_rows(db, role), cal.season_id(day));
}

std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role,
                                      DayNumber day, int hour) {
    const std::vector<const Row*> rows = role_rows(db, role);
    if (rows.empty()) return std::nullopt;  // the only nullopt: a role with no rows

    auto last_at_or_before = [&](DayNumber d, int h) -> std::optional<ScheduledTask> {
        std::optional<ScheduledTask> best;
        for (const ScheduledTask& t : plan_from_rows(rows, cal.season_id(d)))
            if (t.hour <= h) best = t;  // plan is hour-ascending
        return best;
    };

    // Walk back through earlier days for the task that carries over. Day 1 has
    // no yesterday: the world's first night is taken as day 1's own evening.
    DayNumber d = day < 1 ? 1 : day;
    int h = hour;
    for (int steps = 0; steps <= 366; ++steps) {
        if (auto t = last_at_or_before(d, h)) return t;
        if (d > 1) --d;
        else if (h == 23) break;  // day 1's evening already checked
        h = 23;
    }
    return std::nullopt;  // unreachable for a role with rows in some season of the year
}

}  // namespace sim
