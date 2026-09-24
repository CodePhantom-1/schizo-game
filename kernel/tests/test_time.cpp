// test_time.cpp — the calendar spine.
#include "sim/Time.hpp"

#include "sim/Test.hpp"

using namespace sim;

static bool test_day_one_is_year_one_month_one_day_one() {
    Calendar cal;
    SIM_CHECK_EQ(cal.to_date(1), (Date{1, 1, 1}));
    return true;
}

static bool test_year_rolls_over() {
    Calendar cal;  // 360-day year
    SIM_CHECK_EQ(cal.to_date(360), (Date{1, 12, 30}));
    SIM_CHECK_EQ(cal.to_date(361), (Date{2, 1, 1}));
    SIM_CHECK_EQ(cal.to_date(721), (Date{3, 1, 1}));
    return true;
}

static bool test_day_number_round_trip() {
    Calendar cal;
    for (DayNumber d : {DayNumber{1}, DayNumber{30}, DayNumber{31}, DayNumber{180},
                        DayNumber{359}, DayNumber{360}, DayNumber{361},
                        DayNumber{1000}, DayNumber{36000}}) {
        SIM_CHECK_EQ(cal.day_number(cal.to_date(d)), d);
    }
    return true;
}

static bool test_day_of_year() {
    Calendar cal;
    SIM_CHECK_EQ(cal.day_of_year(1), 1);
    SIM_CHECK_EQ(cal.day_of_year(360), 360);
    SIM_CHECK_EQ(cal.day_of_year(361), 1);
    return true;
}

static bool test_seasons_wrap() {
    CalendarConfig cfg;
    cfg.seasons = {{"sowing", 271}, {"harvest", 91}, {"vintage", 181}};  // ids are OPEN data; machinery only
    Calendar cal(cfg);
    SIM_CHECK_EQ(cal.season_id(1), "sowing");     // before the first start => the last-started season wraps over new year
    SIM_CHECK_EQ(cal.season_id(90), "sowing");
    SIM_CHECK_EQ(cal.season_id(180), "harvest");
    SIM_CHECK_EQ(cal.season_id(181), "vintage");
    SIM_CHECK_EQ(cal.season_id(270), "vintage");
    SIM_CHECK_EQ(cal.season_id(271), "sowing");
    return true;
}

static bool test_unnamed_months_by_default() {
    Calendar cal;
    SIM_CHECK(cal.month_name(cal.to_date(1)).empty());
    return true;
}

static bool test_festival_days() {
    CalendarConfig cfg;
    cfg.festival_days = {10, 20};
    Calendar cal(cfg);
    SIM_CHECK(cal.is_festival(10));
    SIM_CHECK(!cal.is_festival(11));
    return true;
}

SIM_MAIN(test_day_one_is_year_one_month_one_day_one,
         test_year_rolls_over,
         test_day_number_round_trip,
         test_day_of_year,
         test_seasons_wrap,
         test_unnamed_months_by_default,
         test_festival_days)
