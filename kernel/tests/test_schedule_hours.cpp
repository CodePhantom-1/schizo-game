// test_schedule_hours.cpp — strict hour parsing in the day planner (kernel
// bug review follow-up, 2026-09-25). A schedules.csv / person_schedules.csv
// row whose `hour` is not a plain integer in 0..23 is skipped, never read
// as a prefix ("6am" -> 6) or kept out of range ("30"). Its own file so it
// stays clear of concurrent edits to test_schedule.cpp. The fixture canon is
// test scaffolding in the system temp directory, never committed as canon.
#include "sim/Schedule.hpp"

#include "sim/Test.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace sim;

namespace {

Db load_hours_fixture_db() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "schizo_game_test_schedule_hours_fixture";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "schedules.csv", std::ios::trunc);
        out << "id,role,hour,task,season,festival,place,tag,source_ref\n"
            << "ok_zero,miller,0,banks the fire,,,,INVENTED,test fixture\n"
            << "ok_padded,miller, 7 ,opens the mill,,,,INVENTED,test fixture\n"
            << "ok_last,miller,23,sleeps,,,,INVENTED,test fixture\n"
            << "bad_suffix,miller,6am,is not real,,,,INVENTED,test fixture\n"
            << "bad_range,miller,30,is not real,,,,INVENTED,test fixture\n"
            << "bad_24,miller,24,is not real,,,,INVENTED,test fixture\n"
            << "bad_negative,miller,-1,is not real,,,,INVENTED,test fixture\n"
            << "bad_fraction,miller,6.5,is not real,,,,INVENTED,test fixture\n"
            << "bad_blank,miller,,is not real,,,,INVENTED,test fixture\n"
            << "bad_word,miller,noon,is not real,,,,INVENTED,test fixture\n"
            << "bad_hex,miller,0x6,is not real,,,,INVENTED,test fixture\n"
            << "bad_huge,miller,99999999999,is not real,,,,INVENTED,test fixture\n";
    }
    {
        std::ofstream out(dir / "person_schedules.csv", std::ios::trunc);
        out << "id,person_id,hour,task,place,season,festival,tag,source_ref\n"
            << "p_ok,miller_a,12,eats with his brother,,,,INVENTED,test fixture\n"
            << "p_bad_suffix,miller_a,9pm,is not real,,,,INVENTED,test fixture\n"
            << "p_bad_range,miller_a,25,is not real,,,,INVENTED,test fixture\n";
    }
    return Db::load(dir.string());
}

std::set<std::string> ids_of(const std::vector<ScheduledTask>& plan) {
    std::set<std::string> out;
    for (const ScheduledTask& t : plan) out.insert(t.schedule_id);
    return out;
}

}  // namespace

static bool test_malformed_or_out_of_range_hours_are_skipped() {
    const Db db = load_hours_fixture_db();
    const Calendar cal;

    const auto plan = day_plan(db, cal, "miller", 10);
    const std::set<std::string> want = {"ok_zero", "ok_padded", "ok_last"};
    SIM_CHECK(ids_of(plan) == want);
    for (const ScheduledTask& t : plan) SIM_CHECK(t.hour >= 0 && t.hour <= 23);

    // "6am" must not become a 6 o'clock row: at 6 the 0 o'clock task is in force.
    const auto at6 = task_at(db, cal, "miller", 10, 6);
    SIM_CHECK(at6.has_value());
    SIM_CHECK_EQ(at6->schedule_id, Id("ok_zero"));
    const auto at7 = task_at(db, cal, "miller", 10, 7);
    SIM_CHECK(at7.has_value());
    SIM_CHECK_EQ(at7->schedule_id, Id("ok_padded"));
    return true;
}

static bool test_person_rows_with_bad_hours_are_skipped() {
    const Db db = load_hours_fixture_db();
    const Calendar cal;
    const auto plan = person_day_plan(db, cal, "miller_a", "miller", 10);
    const std::set<std::string> want = {"ok_zero", "ok_padded", "p_ok", "ok_last"};
    SIM_CHECK(ids_of(plan) == want);
    for (const ScheduledTask& t : plan) SIM_CHECK(t.hour >= 0 && t.hour <= 23);
    return true;
}

SIM_MAIN(test_malformed_or_out_of_range_hours_are_skipped,
         test_person_rows_with_bad_hours_are_skipped)
