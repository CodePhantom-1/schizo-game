// test_schedule.cpp — the NPC day planner: role lookup, season bend, and
// carry-over across midnight. The real db/canon/schedules.csv has no
// seasonal rows yet (D-012 authored glue is all-season), so the seasonal
// bend and tie-break paths are exercised against a throwaway fixture canon
// in the system temp directory, clearly marked as a test fixture and never
// committed as canon. The role-lookup and reachability paths are also
// checked against the real canon.
#include "sim/Schedule.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

using namespace sim;

namespace {

// The approved seasons (D-015): rains(1), sowing(91), harvest(181), vintage(271).
// The K-2 festivals (db/canon/festivals.csv): new_waters(1), first_cutting_procession
// (181), ishtars_torch(271), feeding_of_the_dead(360).
Calendar real_calendar() {
    CalendarConfig cfg;
    cfg.seasons = {{"rains", 1}, {"sowing", 91}, {"harvest", 181}, {"vintage", 271}};
    cfg.festivals = {{"new_waters", 1, "New Waters"},
                      {"first_cutting_procession", 181, "First-Cutting Procession"},
                      {"ishtars_torch", 271, "Ishtar's Torch"},
                      {"feeding_of_the_dead", 360, "Feeding of the Dead"}};
    cfg.festival_days = {1, 181, 271, 360};
    return Calendar(cfg);
}

// Writes a throwaway schedules.csv fixture exercising: an all-season row, a
// same-hour seasonal override, a season-crossing carry-over case, an
// unrelated role, and an OPEN row that must never surface.
Db load_fixture_db() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_schedule_fixture";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "schedules.csv", std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write schedule test fixture");
        out << "id,role,hour,task,season,tag,source_ref\n"
            << "baker_all_season,baker,3,bakes the daily bread,,INVENTED,test fixture\n"
            << "baker_harvest,baker,3,bakes the harvest loaves,harvest,INVENTED,test fixture\n"
            << "farmer_plow,farmer,6,plows the field,,INVENTED,test fixture\n"
            << "farmer_sowing_late,farmer,20,sharpens the scythe by lamplight,sowing,INVENTED,"
               "test fixture\n"
            << "guard_watch,guard,22,walks the wall,,INVENTED,test fixture\n"
            << "ghost_shift,ghost,9,is not real,,OPEN,test fixture\n";
    }
    return Db::load(dir.string());
}

}  // namespace

static bool test_schedule_roles_sorted_unique_skips_open() {
    const Db db = load_fixture_db();
    const std::vector<std::string> roles = schedule_roles(db);
    SIM_CHECK(std::is_sorted(roles.begin(), roles.end()));
    SIM_CHECK_EQ(roles.size(), std::size_t{3});
    SIM_CHECK(std::find(roles.begin(), roles.end(), "baker") != roles.end());
    SIM_CHECK(std::find(roles.begin(), roles.end(), "farmer") != roles.end());
    SIM_CHECK(std::find(roles.begin(), roles.end(), "guard") != roles.end());
    SIM_CHECK(std::find(roles.begin(), roles.end(), "ghost") == roles.end());
    return true;
}

static bool test_role_matching_is_case_insensitive_and_trimmed() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    const auto plan_lower = day_plan(db, cal, "baker", 200);   // harvest
    const auto plan_mixed = day_plan(db, cal, "  BaKeR  ", 200);
    SIM_CHECK_EQ(plan_lower.size(), plan_mixed.size());
    SIM_CHECK_EQ(plan_lower.size(), std::size_t{1});
    SIM_CHECK_EQ(plan_lower[0].schedule_id, plan_mixed[0].schedule_id);
    return true;
}

static bool test_seasonal_row_wins_tie_over_all_season_row() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();

    // Rains (day 1): only the all-season baker row applies.
    const auto rains_plan = day_plan(db, cal, "baker", 1);
    SIM_CHECK_EQ(rains_plan.size(), std::size_t{1});
    SIM_CHECK_EQ(rains_plan[0].schedule_id, Id("baker_all_season"));
    SIM_CHECK_EQ(rains_plan[0].task, std::string("bakes the daily bread"));

    // Harvest (day 200): the seasonal row shares hour 3 and wins.
    const auto harvest_plan = day_plan(db, cal, "baker", 200);
    SIM_CHECK_EQ(harvest_plan.size(), std::size_t{1});
    SIM_CHECK_EQ(harvest_plan[0].schedule_id, Id("baker_harvest"));
    SIM_CHECK_EQ(harvest_plan[0].task, std::string("bakes the harvest loaves"));
    return true;
}

static bool test_seasonal_bend_across_each_season() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    // farmer: all-season plow at hour 6 every day, plus a sowing-only late row.
    SIM_CHECK_EQ(day_plan(db, cal, "farmer", 1).size(), std::size_t{1});     // rains: plow only
    SIM_CHECK_EQ(day_plan(db, cal, "farmer", 100).size(), std::size_t{2});  // sowing: plow + late
    SIM_CHECK_EQ(day_plan(db, cal, "farmer", 200).size(), std::size_t{1});  // harvest: plow only
    SIM_CHECK_EQ(day_plan(db, cal, "farmer", 300).size(), std::size_t{1});  // vintage: plow only
    return true;
}

static bool test_carry_over_before_first_hour_same_day() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    // guard's only row is hour 22; at hour 1 the previous day's last task
    // (the same single row) carries over.
    const auto t = task_at(db, cal, "guard", 50, 1);
    SIM_CHECK(t.has_value());
    SIM_CHECK_EQ(t->schedule_id, Id("guard_watch"));
    return true;
}

static bool test_carry_over_crosses_a_season_boundary() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    // Day 180 is the last day of sowing (season boundary at day 181); the
    // farmer's sowing-only late row (hour 20) is that day's last task.
    // Day 181 (harvest) only has the plow row at hour 6, so a query before
    // hour 6 must carry over day 180's late row, not day 181's own plow row.
    const auto t = task_at(db, cal, "farmer", 181, 2);
    SIM_CHECK(t.has_value());
    SIM_CHECK_EQ(t->schedule_id, Id("farmer_sowing_late"));
    SIM_CHECK_EQ(t->task, std::string("sharpens the scythe by lamplight"));
    return true;
}

static bool test_task_at_exact_and_between_hours() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    SIM_CHECK_EQ(task_at(db, cal, "farmer", 1, 6)->schedule_id, Id("farmer_plow"));
    SIM_CHECK_EQ(task_at(db, cal, "farmer", 1, 12)->schedule_id, Id("farmer_plow"));
    return true;
}

static bool test_unknown_role_returns_nullopt() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    SIM_CHECK(day_plan(db, cal, "dragon rider", 1).empty());
    SIM_CHECK(!task_at(db, cal, "dragon rider", 1, 12).has_value());
    return true;
}

// Batch-1 review: on day 1, before a role's first hour, the world's first night
// is day 1's own evening — never nullopt for a known role.
static bool test_day_one_before_first_hour_is_not_empty() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    for (const std::string& role : schedule_roles(db)) {
        const auto plan = day_plan(db, cal, role, 1);
        if (plan.empty()) continue;  // a seasonal-only role out of season has no task on day 1
        const auto t = task_at(db, cal, role, 1, 0);
        SIM_CHECK(t.has_value());
        SIM_CHECK_EQ(t->schedule_id,
                     plan.front().hour == 0 ? plan.front().schedule_id : plan.back().schedule_id);
    }
    return true;
}

static bool test_open_row_never_surfaces() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    SIM_CHECK(day_plan(db, cal, "ghost", 1).empty());
    SIM_CHECK(!task_at(db, cal, "ghost", 1, 9).has_value());
    return true;
}

static bool test_every_fixture_row_reachable() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    std::set<Id> reachable;
    for (DayNumber day : {DayNumber{1}, DayNumber{100}, DayNumber{200}, DayNumber{300}}) {
        for (const std::string& role : schedule_roles(db)) {
            for (const ScheduledTask& t : day_plan(db, cal, role, day)) reachable.insert(t.schedule_id);
        }
    }
    for (const Row& row : db.rows("schedules")) {
        const std::string tag = row.get("tag");
        if (tag == "OPEN") continue;
        SIM_CHECK(reachable.count(row.get("id")) == 1);
    }
    return true;
}

static bool test_every_real_canon_row_reachable() {
    const Db db = Db::load("../db/canon");
    const Calendar cal = real_calendar();
    std::set<Id> reachable;
    // Every season boundary, every real festival day (1/181/271/360), and
    // every shift variant used in schedules.csv/people.csv (K-2: two people
    // of a role can now differ) — a variant-tagged row is invisible to a
    // bare "" query, by design (Schedule.hpp), so it must be probed for
    // explicitly to be counted reachable.
    const std::vector<DayNumber> days = {DayNumber{1}, DayNumber{100}, DayNumber{200},
                                          DayNumber{300}, DayNumber{181}, DayNumber{271},
                                          DayNumber{360}};
    const std::vector<std::string> variants = {"",       "dawn",   "dusk",
                                                "night_patrol", "deep", "rains_shift"};
    for (DayNumber day : days) {
        for (const std::string& role : schedule_roles(db)) {
            for (const std::string& variant : variants) {
                for (const ScheduledTask& t : day_plan(db, cal, role, day, variant))
                    reachable.insert(t.schedule_id);
            }
        }
    }
    for (const Row& row : db.rows("schedules")) {
        const std::string tag = row.get("tag");
        if (tag == "OPEN") continue;
        SIM_CHECK(reachable.count(row.get("id")) == 1);
    }
    return true;
}

static bool test_determinism_same_inputs_same_outputs() {
    const Db db = load_fixture_db();
    const Calendar cal = real_calendar();
    const auto plan_a = day_plan(db, cal, "farmer", 100);
    const auto plan_b = day_plan(db, cal, "farmer", 100);
    SIM_CHECK_EQ(plan_a.size(), plan_b.size());
    for (std::size_t i = 0; i < plan_a.size(); ++i) {
        SIM_CHECK_EQ(plan_a[i].schedule_id, plan_b[i].schedule_id);
        SIM_CHECK_EQ(plan_a[i].hour, plan_b[i].hour);
        SIM_CHECK_EQ(plan_a[i].task, plan_b[i].task);
    }
    const auto t_a = task_at(db, cal, "farmer", 181, 2);
    const auto t_b = task_at(db, cal, "farmer", 181, 2);
    SIM_CHECK_EQ(t_a.has_value(), t_b.has_value());
    SIM_CHECK_EQ(t_a->schedule_id, t_b->schedule_id);
    return true;
}

SIM_MAIN(test_schedule_roles_sorted_unique_skips_open, test_role_matching_is_case_insensitive_and_trimmed,
         test_seasonal_row_wins_tie_over_all_season_row, test_seasonal_bend_across_each_season,
         test_carry_over_before_first_hour_same_day, test_carry_over_crosses_a_season_boundary,
         test_task_at_exact_and_between_hours, test_unknown_role_returns_nullopt,
         test_day_one_before_first_hour_is_not_empty,
         test_open_row_never_surfaces, test_every_fixture_row_reachable,
         test_every_real_canon_row_reachable, test_determinism_same_inputs_same_outputs)
