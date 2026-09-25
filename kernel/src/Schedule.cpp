#include "sim/Schedule.hpp"

#include "sim/Text.hpp"

#include <algorithm>
#include <cctype>
#include <map>

namespace sim {
namespace {

bool is_open(const Row& row) { return trim(row.get("tag")) == "OPEN"; }

}  // namespace

std::vector<std::string> schedule_roles(const Db& db) {
    std::map<std::string, std::string> by_key;  // normalized -> first-seen casing
    for (const Row& row : db.rows("schedules")) {
        if (is_open(row)) continue;
        const std::string role = trim(row.get("role"));
        if (role.empty()) continue;
        by_key.emplace(normalize_key(role), role);
    }
    std::vector<std::string> out;
    out.reserve(by_key.size());
    for (auto& [key, role] : by_key) out.push_back(role);
    std::sort(out.begin(), out.end());
    return out;
}

namespace {

// The role's non-OPEN rows matching `variant` (blank `shift` = shared by
// everyone in the role; a set `shift` only matches that exact variant) — one
// table scan; task_at reuses it across days.
std::vector<const Row*> role_rows(const Db& db, const std::string& role, const std::string& variant) {
    const std::string wanted_role = normalize_key(role);
    const std::string wanted_variant = normalize_key(variant);
    std::vector<const Row*> out;
    for (const Row& row : db.rows("schedules")) {
        if (is_open(row) || normalize_key(row.get("role")) != wanted_role) continue;
        const std::string row_variant = normalize_key(row.get("shift"));
        if (!row_variant.empty() && row_variant != wanted_variant) continue;
        out.push_back(&row);
    }
    return out;
}

// Priority a row wins the hour-tie-break with: festival beats season beats
// all-season (day_plan's documented precedence).
int row_priority(const Row& row, const std::string& season_now, const std::string& festival_now) {
    const std::string festival = trim(row.get("festival"));
    if (!festival.empty() && !festival_now.empty() && festival == festival_now) return 2;
    const std::string season = trim(row.get("season"));
    if (!season.empty() && season == season_now) return 1;
    return 0;
}

std::vector<ScheduledTask> plan_from_rows(const std::vector<const Row*>& rows,
                                          const std::string& season_now,
                                          const std::string& festival_now) {
    std::map<int, std::pair<int, ScheduledTask>> by_hour;  // hour -> (priority, chosen row)

    for (const Row* rp : rows) {
        const Row& row = *rp;
        const std::string festival = trim(row.get("festival"));
        // A festival-tagged row only ever applies on its own festival's day;
        // a seasonal row only on its own season; an all-season row (both
        // blank) always applies.
        if (!festival.empty()) {
            if (festival_now.empty() || festival != festival_now) continue;
        } else {
            const std::string season = trim(row.get("season"));
            if (!season.empty() && season != season_now) continue;
        }

        int hour = 0;
        try {
            hour = std::stoi(row.get("hour", "0"));
        } catch (...) {
            continue;  // malformed hour: skip rather than crash on canon data
        }

        const int priority = row_priority(row, season_now, festival_now);
        auto it = by_hour.find(hour);
        if (it != by_hour.end() && it->second.first > priority) continue;  // a higher row already won

        ScheduledTask t;
        t.schedule_id = row.get("id");
        t.role = trim(row.get("role"));
        t.hour = hour;
        t.task = row.get("task");
        by_hour[hour] = {priority, t};
    }

    std::vector<ScheduledTask> out;
    out.reserve(by_hour.size());
    for (auto& [hour, entry] : by_hour) out.push_back(entry.second);
    return out;
}

}  // namespace

std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role,
                                     DayNumber day, const std::string& variant) {
    return plan_from_rows(role_rows(db, role, variant), cal.season_id(day), cal.festival_id(day));
}

std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role,
                                      DayNumber day, int hour, const std::string& variant) {
    const std::vector<const Row*> rows = role_rows(db, role, variant);
    if (rows.empty()) return std::nullopt;  // the only nullopt: a role with no rows

    auto last_at_or_before = [&](DayNumber d, int h) -> std::optional<ScheduledTask> {
        std::optional<ScheduledTask> best;
        for (const ScheduledTask& t : plan_from_rows(rows, cal.season_id(d), cal.festival_id(d)))
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
