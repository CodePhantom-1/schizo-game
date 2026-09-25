#pragma once
// Schedule.hpp — the NPC day planner (contract, T2 module).
// Pure functions over canon: schedules are tasks bent by season, not fixed
// positions (docs/mechanics.md row 7; ../docs/living-world.md §2;
// ../docs/city-life.md §1). No state of its own => nothing to save.
#include "sim/Db.hpp"
#include "sim/Time.hpp"
#include "sim/Types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sim {

struct ScheduledTask {
    Id schedule_id;
    std::string role;
    int hour = 0;
    std::string task;
};

// Every role named in db/canon/schedules.csv (OPEN rows excluded), sorted and
// unique (case-insensitive dedupe; the first-seen casing is kept).
std::vector<std::string> schedule_roles(const Db& db);

// The rows for `role` that apply on `day`: season blank, or season equal to
// cal.season_id(day). Sorted by hour. When a seasonal row and an all-season
// row share the same hour, the seasonal one wins. Role matching is
// case-insensitive and trims whitespace. OPEN-tagged rows are skipped.
std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role,
                                     DayNumber day);

// The task in force at `hour` (0..23): the day_plan row with the greatest
// hour <= hour. Before the first row of the day, the previous day's last
// task carries over. nullopt if the role has no rows at all in canon.
std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role,
                                      DayNumber day, int hour);

// Festival-override hook: festival days are OPEN in canon (day content is
// undecided), so this module makes no behavioural change for
// cal.is_festival(day) yet. When festival schedules are authored, day_plan
// should consult a `festival` column (or a dedicated table) the same way it
// already consults `season`. No behaviour lives here today — documented only.

}  // namespace sim
