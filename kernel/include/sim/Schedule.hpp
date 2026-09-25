#pragma once
// Schedule.hpp — the NPC day planner (contract, T2 module).
// Pure functions over canon: schedules are tasks bent by season and festival,
// not fixed positions (docs/mechanics.md rows 7 and 14; ../docs/living-world.md
// §2; ../docs/city-life.md §1, §5). No state of its own => nothing to save.
//
// Three layers resolve a day (K-2):
//   1. role rows      db/canon/schedules.csv        (role, hour, season, festival, place)
//   2. person rows    db/canon/person_schedules.csv (person_id, hour, season, festival, place)
//   3. the festival   db/canon/festivals.csv        (gathering window, place, exempt roles, market)
// Per hour, the winning row is the highest rank:
//   festival rows (festival column set)  beat  ordinary rows,
//   then person rows  beat  role rows,
//   then the more specific filter (named festival over "any"; seasonal over all-season) wins;
//   equal rank => the later row in file order wins.
// On a named festival day, a non-exempt role's ordinary rows inside the
// festival's [attend_from, attend_until) window are replaced by the
// festival's gathering task (at attend_from, unless a festival row already
// sits there), and the ordinary task in force at attend_until resumes then.
#include "sim/Db.hpp"
#include "sim/Time.hpp"
#include "sim/Types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace sim {

struct ScheduledTask {
    Id schedule_id;        // schedules.csv / person_schedules.csv id, or the festivals.csv id for a gathering
    std::string role;
    int hour = 0;
    std::string task;
    // Where. Role-level queries return the raw token: "" (unplaced), "home",
    // "work", or a places.csv id. Person-level queries resolve "home"/"work"
    // through people.csv home_place/work_place ("" when the person has none).
    std::string place;
    Id festival;           // the festival this task belongs to ("" = an ordinary task)
    std::string origin;    // "role", "person" or "festival" (a gathering)
};

// Today's festival, as schedules see it (festivals.csv row for cal.festival_id(day)).
struct FestivalBend {
    Id festival_id;
    std::string name;
    Id deity;
    std::string place;                      // places.csv id or "home"
    int attend_from = 0;                    // 0..23
    int attend_until = 24;                  // attend_from+1..24 (24 = through the night)
    std::vector<std::string> exempt_roles;  // normalized (trimmed, lower-case)
    bool market_open = true;
    std::string gathering;                  // the gathering task text
};

// Every role named in db/canon/schedules.csv (OPEN rows excluded), sorted and
// unique (case-insensitive dedupe; the first-seen casing is kept).
std::vector<std::string> schedule_roles(const Db& db);

// The rows for `role` that apply on `day`: season blank or equal to
// cal.season_id(day); festival blank, or (on a festival day) "any" or equal
// to cal.festival_id(day). Sorted by hour, bent by today's festival
// gathering (see the header comment). Role matching is case-insensitive and
// trims whitespace. OPEN-tagged rows are skipped.
std::vector<ScheduledTask> day_plan(const Db& db, const Calendar& cal, const std::string& role,
                                     DayNumber day);

// The task in force at `hour` (0..23): the day_plan row with the greatest
// hour <= hour. Before the first row of the day, the previous day's last
// task carries over. nullopt if the role has no rows at all in canon.
std::optional<ScheduledTask> task_at(const Db& db, const Calendar& cal, const std::string& role,
                                      DayNumber day, int hour);

// The festival bend for `day`: nullopt when the day has no named festival or
// its festivals.csv row is missing/OPEN. Malformed hours disable the
// gathering window (attend_from = attend_until = 0) but keep the rest.
std::optional<FestivalBend> festival_bend(const Db& db, const Calendar& cal, DayNumber day);

// False only when today's festival row says market=closed.
bool market_open(const Db& db, const Calendar& cal, DayNumber day);

// Per-person resolution: the person's role rows plus their own
// person_schedules.csv rows, bent by season and festival, with places
// resolved through people.csv. `role` is the person's schedule role (Npc::role;
// may be empty — then only person rows apply).
std::vector<ScheduledTask> person_day_plan(const Db& db, const Calendar& cal, const Id& person_id,
                                            const std::string& role, DayNumber day);

// As task_at, for one person. nullopt when neither the role nor the person
// has any rows in canon.
std::optional<ScheduledTask> person_task_at(const Db& db, const Calendar& cal, const Id& person_id,
                                             const std::string& role, DayNumber day, int hour);

}  // namespace sim
