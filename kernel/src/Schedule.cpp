#include "sim/Schedule.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

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

bool parse_hour(const std::string& s, int& out) {
    try {
        std::size_t used = 0;
        const int v = std::stoi(s, &used);
        if (trim(s.substr(used)).size() != 0) return false;
        out = v;
        return true;
    } catch (...) {
        return false;  // malformed hour: skip rather than crash on canon data
    }
}

struct Candidate {
    const Row* row;
    bool person;
};

// The role's non-OPEN rows — one table scan; task_at reuses it across days.
std::vector<Candidate> role_rows(const Db& db, const std::string& role) {
    const std::string wanted = normalize(role);
    std::vector<Candidate> out;
    if (wanted.empty()) return out;
    for (const Row& row : db.rows("schedules"))
        if (!is_open(row) && normalize(row.get("role")) == wanted) out.push_back({&row, false});
    return out;
}

std::vector<Candidate> person_rows(const Db& db, const Id& person_id) {
    std::vector<Candidate> out;
    const std::string wanted = trim(person_id);
    if (wanted.empty()) return out;
    for (const Row& row : db.rows("person_schedules"))
        if (!is_open(row) && trim(row.get("person_id")) == wanted) out.push_back({&row, true});
    return out;
}

struct Entry {
    ScheduledTask task;
    int rank = 0;  // >= 4 means a festival row (never displaced by the gathering)
};

// One day's plan from candidate rows: filter by season + festival, rank, then
// apply the festival gathering window. Returned hour-ascending.
std::vector<ScheduledTask> build_plan(const Db& db, const Calendar& cal,
                                      const std::vector<Candidate>& rows, const std::string& role,
                                      DayNumber day) {
    const std::string& season_now = cal.season_id(day);
    const bool festival_day = cal.is_festival(day);
    const std::string fest_now = normalize(cal.festival_id(day));

    std::map<int, Entry> by_hour;
    for (const Candidate& c : rows) {
        const Row& row = *c.row;
        const std::string season = trim(row.get("season"));
        if (!season.empty() && season != season_now) continue;

        const std::string fest = normalize(row.get("festival"));
        const bool fest_row = !fest.empty();
        if (fest_row) {
            if (!festival_day) continue;
            if (fest != "any" && (fest_now.empty() || fest != fest_now)) continue;
        }

        int hour = 0;
        if (!parse_hour(row.get("hour", "0"), hour) || hour < 0 || hour > 23) continue;

        const bool specific = fest_row ? fest != "any" : !season.empty();
        const int rank = (fest_row ? 4 : 0) + (c.person ? 2 : 0) + (specific ? 1 : 0);

        auto it = by_hour.find(hour);
        if (it != by_hour.end() && it->second.rank > rank) continue;  // equal rank: later row wins

        Entry e;
        e.rank = rank;
        e.task.schedule_id = row.get("id");
        e.task.role = trim(role);
        e.task.hour = hour;
        e.task.task = row.get("task");
        e.task.place = trim(row.get("place"));
        e.task.festival = fest_row ? (fest == "any" ? cal.festival_id(day) : trim(row.get("festival")))
                                   : std::string{};
        e.task.origin = c.person ? "person" : "role";
        by_hour[hour] = std::move(e);
    }

    // The festival gathering (festivals.csv): bends the ordinary rows of every
    // non-exempt role inside [attend_from, attend_until).
    if (!by_hour.empty()) {
        if (const auto bend = festival_bend(db, cal, day)) {
            const std::string r = normalize(role);
            const bool exempt =
                std::find(bend->exempt_roles.begin(), bend->exempt_roles.end(), r) !=
                bend->exempt_roles.end();
            const int from = bend->attend_from, until = bend->attend_until;
            if (!exempt && from < until) {
                // What the ordinary plan has in force at `until` (wrapping to
                // the day's last ordinary row when nothing precedes it).
                std::optional<ScheduledTask> resume;
                for (auto& [h, e] : by_hour)
                    if (e.rank < 4 && h <= until) resume = e.task;
                if (!resume)
                    for (auto& [h, e] : by_hour)
                        if (e.rank < 4) resume = e.task;

                for (auto it = by_hour.begin(); it != by_hour.end();) {
                    if (it->second.rank < 4 && it->first >= from && it->first < until)
                        it = by_hour.erase(it);
                    else
                        ++it;
                }
                if (by_hour.find(from) == by_hour.end()) {
                    Entry g;
                    g.rank = 4;
                    g.task.schedule_id = bend->festival_id;
                    g.task.role = trim(role);
                    g.task.hour = from;
                    g.task.task = bend->gathering;
                    g.task.place = bend->place;
                    g.task.festival = bend->festival_id;
                    g.task.origin = "festival";
                    by_hour[from] = std::move(g);
                }
                if (until < 24 && resume && by_hour.find(until) == by_hour.end()) {
                    Entry back;
                    back.rank = 0;
                    back.task = *resume;
                    back.task.hour = until;
                    by_hour[until] = std::move(back);
                }
            }
        }
    }

    std::vector<ScheduledTask> out;
    out.reserve(by_hour.size());
    for (auto& [hour, e] : by_hour) out.push_back(e.task);
    return out;
}

// task_at over any candidate set: walks back through earlier days for the
// task that carries over. Day 1 has no yesterday: the world's first night is
// taken as day 1's own evening.
std::optional<ScheduledTask> carry_task_at(const Db& db, const Calendar& cal,
                                           const std::vector<Candidate>& rows,
                                           const std::string& role, DayNumber day, int hour) {
    if (rows.empty()) return std::nullopt;
    DayNumber d = day < 1 ? 1 : day;
    int h = hour;
    for (int steps = 0; steps <= 366; ++steps) {
        std::optional<ScheduledTask> best;
        for (const ScheduledTask& t : build_plan(db, cal, rows, role, d))
            if (t.hour <= h) best = t;  // plan is hour-ascending
        if (best) return best;
        if (d > 1) --d;
        else if (h == 23) break;  // day 1's evening already checked
        h = 23;
    }
    return std::nullopt;  // rows exist but none ever applies (e.g. festival-only rows, no festival)
}

void resolve_place(const Db& db, const Id& person_id, ScheduledTask& t) {
    if (t.place != "home" && t.place != "work") return;
    const std::optional<Row> person = db.find("people", person_id);
    t.place = person ? trim(person->get(t.place == "home" ? "home_place" : "work_place")) : "";
}

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

std::optional<FestivalBend> festival_bend(const Db& db, const Calendar& cal, DayNumber day) {
    const std::string& id = cal.festival_id(day);
    if (id.empty()) return std::nullopt;
    const std::optional<Row> row = db.find("festivals", id);
    if (!row || is_open(*row)) return std::nullopt;
    FestivalBend b;
    b.festival_id = id;
    b.name = row->get("name");
    b.deity = trim(row->get("deity"));
    b.place = trim(row->get("place"));
    int from = 0, until = 0;
    if (parse_hour(row->get("attend_from"), from) && parse_hour(row->get("attend_until"), until) &&
        from >= 0 && from <= 23 && until > from && until <= 24) {
        b.attend_from = from;
        b.attend_until = until;
    } else {
        b.attend_from = b.attend_until = 0;  // no gathering window
    }
    std::stringstream ss(row->get("exempt_roles"));
    std::string role;
    while (std::getline(ss, role, ';'))
        if (!normalize(role).empty()) b.exempt_roles.push_back(normalize(role));
    b.market_open = normalize(row->get("market")) != "closed";
    b.gathering = row->get("gathering");
    return b;
}

bool market_open(const Db& db, const Calendar& cal, DayNumber day) {
    const auto b = festival_bend(db, cal, day);
    return !b || b->market_open;
}

std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role,
                                     DayNumber day) {
    return build_plan(db, cal, role_rows(db, role), role, day);
}

std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role,
                                      DayNumber day, int hour) {
    return carry_task_at(db, cal, role_rows(db, role), role, day, hour);
}

std::vector<ScheduledTask> person_day_plan(const Db& db, const Calendar& cal, const Id& person_id,
                                            const std::string& role, DayNumber day) {
    std::vector<Candidate> rows = role_rows(db, role);
    const std::vector<Candidate> own = person_rows(db, person_id);
    rows.insert(rows.end(), own.begin(), own.end());
    std::vector<ScheduledTask> plan = build_plan(db, cal, rows, role, day);
    for (ScheduledTask& t : plan) resolve_place(db, person_id, t);
    return plan;
}

std::optional<ScheduledTask> person_task_at(const Db& db, const Calendar& cal, const Id& person_id,
                                             const std::string& role, DayNumber day, int hour) {
    std::vector<Candidate> rows = role_rows(db, role);
    const std::vector<Candidate> own = person_rows(db, person_id);
    rows.insert(rows.end(), own.begin(), own.end());
    std::optional<ScheduledTask> t = carry_task_at(db, cal, rows, role, day, hour);
    if (t) resolve_place(db, person_id, *t);
    return t;
}

}  // namespace sim
