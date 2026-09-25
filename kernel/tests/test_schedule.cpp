// test_schedule.cpp — the NPC day planner: role lookup, season bend, and
// carry-over across midnight. The real db/canon/schedules.csv has no
// seasonal rows yet (D-012 authored glue is all-season), so the seasonal
// bend and tie-break paths are exercised against a throwaway fixture canon
// in the system temp directory, clearly marked as a test fixture and never
// committed as canon. The role-lookup and reachability paths are also
// checked against the real canon.
#include "sim/Schedule.hpp"
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

using namespace sim;

namespace {

// The approved seasons (D-015): rains(1), sowing(91), harvest(181), vintage(271).
Calendar real_calendar() {
    CalendarConfig cfg;
    cfg.seasons = {{"rains", 1}, {"sowing", 91}, {"harvest", 181}, {"vintage", 271}};
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
    // The calendar exactly as the world builds it (seasons + festivals.csv).
    WorldState w;
    w.init("../db/canon", 1);
    const Db& db = w.db;
    const Calendar& cal = w.cal;
    std::vector<DayNumber> days = {1, 100, 200, 300};
    for (const FestivalDef& f : cal.festivals()) days.push_back(f.day_of_year);
    std::set<Id> reachable;
    for (DayNumber day : days) {
        for (const std::string& role : schedule_roles(db))
            for (const ScheduledTask& t : day_plan(db, cal, role, day)) reachable.insert(t.schedule_id);
        for (const Npc& n : w.population.npcs)
            for (const ScheduledTask& t : person_day_plan(db, cal, n.id, n.role, day))
                reachable.insert(t.schedule_id);
    }
    for (const char* table : {"schedules", "person_schedules"}) {
        for (const Row& row : db.rows(table)) {
            if (row.get("tag") == "OPEN") continue;
            if (reachable.count(row.get("id")) != 1)
                std::fprintf(stderr, "unreachable %s row %s\n", table, row.get("id").c_str());
            SIM_CHECK(reachable.count(row.get("id")) == 1);
        }
    }
    // Every festival's gathering reaches somebody.
    for (const FestivalDef& f : cal.festivals()) SIM_CHECK(reachable.count(f.id) == 1);
    return true;
}

// ---------------------------------------------------------------------------
// K-2: festivals and per-person resolution, against a fixture canon.
namespace {

// Day-of-year 50 is the fixture's "fixture_feast" (market closed, gathering
// 9..12 at fixture_temple, guards exempt); day 60 is "fixture_fair" (market
// open, malformed gathering hours).
Db load_festival_fixture_db() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_schedule_festival_fixture";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "schedules.csv", std::ios::trunc);
        out << "id,role,hour,task,season,festival,place,tag,source_ref\n"
            << "baker_oven,baker,3,fires the oven,,,work,INVENTED,test fixture\n"
            << "baker_sell,baker,10,sells loaves,,,work,INVENTED,test fixture\n"
            << "baker_home,baker,18,goes home,,,home,INVENTED,test fixture\n"
            << "priest_morning,priest,6,morning rite,,,work,INVENTED,test fixture\n"
            << "priest_feast,priest,9,leads the feast rite,,fixture_feast,work,INVENTED,test fixture\n"
            << "priest_any,priest,20,feast-night vigil,,any,work,INVENTED,test fixture\n"
            << "guard_watch,guard,8,walks the wall,,,fixture_wall,INVENTED,test fixture\n"
            << "ghost_feast,ghost,9,is not real,,fixture_feast,,OPEN,test fixture\n";
    }
    {
        std::ofstream out(dir / "festivals.csv", std::ios::trunc);
        out << "id,name,day_of_year,deity,place,attend_from,attend_until,exempt_roles,market,"
               "gathering,description,tag,source_ref\n"
            << "fixture_feast,Fixture Feast,50,nanna,fixture_temple,9,12, Guard ;watchman,closed,"
               "attends the feast,test,INVENTED,test fixture\n"
            << "fixture_fair,Fixture Fair,60,utu,fixture_square,x,y,,open,browses the fair,test,"
               "INVENTED,test fixture\n";
    }
    {
        std::ofstream out(dir / "people.csv", std::ios::trunc);
        out << "id,name,role,city,faction,home_place,work_place,tag,source_ref\n"
            << "baker_a,A,baker,c,,house_a,bakery_a,INVENTED,test fixture\n"
            << "baker_b,B,baker,c,,house_b,bakery_b,INVENTED,test fixture\n";
    }
    {
        std::ofstream out(dir / "person_schedules.csv", std::ios::trunc);
        out << "id,person_id,hour,task,place,season,festival,tag,source_ref\n"
            << "baker_b_late,baker_b,3,sleeps late,home,,,INVENTED,test fixture\n"
            << "baker_b_feast,baker_b,9,bakes the feast bread,work,,fixture_feast,INVENTED,test fixture\n"
            << "baker_b_open,baker_b,14,is not real,,,,OPEN,test fixture\n";
    }
    return Db::load(dir.string());
}

Calendar festival_calendar() {
    CalendarConfig cfg;
    cfg.seasons = {{"rains", 1}, {"sowing", 91}, {"harvest", 181}, {"vintage", 271}};
    cfg.festivals = {{"fixture_feast", "Fixture Feast", 50}, {"fixture_fair", "Fixture Fair", 60}};
    return Calendar(cfg);
}

}  // namespace

static bool test_no_festival_day_plan_is_unchanged() {
    const Db db = load_festival_fixture_db();
    const Calendar cal = festival_calendar();
    const auto plan = day_plan(db, cal, "baker", 49);
    SIM_CHECK_EQ(plan.size(), std::size_t{3});
    for (const ScheduledTask& t : plan) {
        SIM_CHECK(t.festival.empty());
        SIM_CHECK_EQ(t.origin, std::string("role"));
    }
    SIM_CHECK_EQ(day_plan(db, cal, "priest", 49).size(), std::size_t{1});  // festival rows dormant
    SIM_CHECK(market_open(db, cal, 49));
    SIM_CHECK(!festival_bend(db, cal, 49).has_value());
    return true;
}

static bool test_festival_gathering_bends_the_role_day() {
    const Db db = load_festival_fixture_db();
    const Calendar cal = festival_calendar();
    // baker on the feast: 3 oven, 9 gathering (10 sell removed), 12 resume sell, 18 home.
    const auto plan = day_plan(db, cal, "baker", 50);
    SIM_CHECK_EQ(plan.size(), std::size_t{4});
    SIM_CHECK_EQ(plan[0].schedule_id, Id("baker_oven"));
    SIM_CHECK_EQ(plan[1].hour, 9);
    SIM_CHECK_EQ(plan[1].schedule_id, Id("fixture_feast"));
    SIM_CHECK_EQ(plan[1].origin, std::string("festival"));
    SIM_CHECK_EQ(plan[1].place, std::string("fixture_temple"));
    SIM_CHECK_EQ(plan[1].task, std::string("attends the feast"));
    SIM_CHECK_EQ(plan[1].festival, Id("fixture_feast"));
    SIM_CHECK_EQ(plan[2].hour, 12);
    SIM_CHECK_EQ(plan[2].schedule_id, Id("baker_sell"));  // the ordinary task resumes
    SIM_CHECK_EQ(plan[3].schedule_id, Id("baker_home"));
    SIM_CHECK_EQ(task_at(db, cal, "baker", 50, 11)->schedule_id, Id("fixture_feast"));
    SIM_CHECK_EQ(task_at(db, cal, "baker", 50, 13)->schedule_id, Id("baker_sell"));
    // Next day: back to normal; the carry-over at 1h is yesterday's last task.
    SIM_CHECK_EQ(task_at(db, cal, "baker", 51, 10)->schedule_id, Id("baker_sell"));
    SIM_CHECK_EQ(task_at(db, cal, "baker", 51, 1)->schedule_id, Id("baker_home"));
    // A festival every year: same bend on day 50 + 360.
    SIM_CHECK_EQ(task_at(db, cal, "baker", 410, 11)->schedule_id, Id("fixture_feast"));
    SIM_CHECK(!market_open(db, cal, 50));
    return true;
}

static bool test_festival_rows_and_exemptions() {
    const Db db = load_festival_fixture_db();
    const Calendar cal = festival_calendar();
    // priest: the named-festival row owns the gathering hour; the "any" row applies.
    const auto plan = day_plan(db, cal, "priest", 50);
    SIM_CHECK_EQ(plan.size(), std::size_t{4});
    SIM_CHECK_EQ(plan[0].schedule_id, Id("priest_morning"));
    SIM_CHECK_EQ(plan[1].schedule_id, Id("priest_feast"));
    SIM_CHECK_EQ(plan[1].festival, Id("fixture_feast"));
    SIM_CHECK_EQ(plan[2].hour, 12);
    SIM_CHECK_EQ(plan[2].schedule_id, Id("priest_morning"));  // resumed after the window
    SIM_CHECK_EQ(plan[3].schedule_id, Id("priest_any"));
    SIM_CHECK_EQ(plan[3].festival, Id("fixture_feast"));  // "any" reports the day's festival
    // "any" rows apply on every festival, named rows only on theirs.
    const auto fair = day_plan(db, cal, "priest", 60);
    SIM_CHECK_EQ(fair.size(), std::size_t{2});
    SIM_CHECK_EQ(fair[1].schedule_id, Id("priest_any"));
    // exempt roles (trimmed, case-insensitive) keep their day.
    const auto guard = day_plan(db, cal, "guard", 50);
    SIM_CHECK_EQ(guard.size(), std::size_t{1});
    SIM_CHECK_EQ(guard[0].schedule_id, Id("guard_watch"));
    // an OPEN festival row never surfaces.
    SIM_CHECK(day_plan(db, cal, "ghost", 50).empty());
    // a malformed gathering window disables the gathering, keeps the market flag.
    const auto bend = festival_bend(db, cal, 60);
    SIM_CHECK(bend.has_value());
    SIM_CHECK_EQ(bend->attend_from, bend->attend_until);
    SIM_CHECK(market_open(db, cal, 60));
    SIM_CHECK_EQ(day_plan(db, cal, "baker", 60).size(), std::size_t{3});
    return true;
}

static bool test_person_overrides_and_places() {
    const Db db = load_festival_fixture_db();
    const Calendar cal = festival_calendar();
    // baker_a: the plain role day, places resolved through people.csv.
    const auto a = person_day_plan(db, cal, "baker_a", "baker", 49);
    SIM_CHECK_EQ(a.size(), std::size_t{3});
    SIM_CHECK_EQ(a[0].place, std::string("bakery_a"));
    SIM_CHECK_EQ(a[2].place, std::string("house_a"));
    // baker_b: own row at 3 overrides the role row; OPEN person row skipped.
    const auto b = person_day_plan(db, cal, "baker_b", "baker", 49);
    SIM_CHECK_EQ(b.size(), std::size_t{3});
    SIM_CHECK_EQ(b[0].schedule_id, Id("baker_b_late"));
    SIM_CHECK_EQ(b[0].origin, std::string("person"));
    SIM_CHECK_EQ(b[0].place, std::string("house_b"));
    SIM_CHECK_EQ(b[1].place, std::string("bakery_b"));
    // On the feast, baker_b's own festival row replaces the gathering for him.
    SIM_CHECK_EQ(person_task_at(db, cal, "baker_b", "baker", 50, 10)->schedule_id,
                 Id("baker_b_feast"));
    SIM_CHECK_EQ(person_task_at(db, cal, "baker_a", "baker", 50, 10)->schedule_id,
                 Id("fixture_feast"));
    SIM_CHECK_EQ(person_task_at(db, cal, "baker_a", "baker", 50, 10)->place,
                 std::string("fixture_temple"));
    // Role-level queries keep the raw place token.
    SIM_CHECK_EQ(day_plan(db, cal, "baker", 49)[0].place, std::string("work"));
    // Unknown person with a role: the role plan, unplaced; unknown both: nullopt.
    SIM_CHECK(person_task_at(db, cal, "nobody", "baker", 49, 10).has_value());
    SIM_CHECK_EQ(person_task_at(db, cal, "nobody", "baker", 49, 10)->place, std::string(""));
    SIM_CHECK(!person_task_at(db, cal, "nobody", "", 49, 10).has_value());
    // Determinism.
    const auto x = person_day_plan(db, cal, "baker_b", "baker", 50);
    const auto y = person_day_plan(db, cal, "baker_b", "baker", 50);
    SIM_CHECK_EQ(x.size(), y.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        SIM_CHECK_EQ(x[i].schedule_id, y[i].schedule_id);
        SIM_CHECK_EQ(x[i].place, y[i].place);
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
         test_every_real_canon_row_reachable, test_determinism_same_inputs_same_outputs,
         test_no_festival_day_plan_is_unchanged, test_festival_gathering_bends_the_role_day,
         test_festival_rows_and_exemptions, test_person_overrides_and_places)
