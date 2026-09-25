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

// K-2: festival_days holds day-of-year values, so a festival repeats every
// year (day 10 of year 2 is day number 370 on the 360-day calendar).
static bool test_festival_days_repeat_every_year() {
    CalendarConfig cfg;
    cfg.festival_days = {10, 20};
    Calendar cal(cfg);
    SIM_CHECK(cal.is_festival(370));   // year 2's day 10
    SIM_CHECK(cal.is_festival(380));   // year 2's day 20
    SIM_CHECK(!cal.is_festival(371));
    SIM_CHECK(cal.is_festival(730));   // year 3's day 10 (day_of_year wraps again)
    return true;
}

// K-2: festivals.csv rows give a festival its id/name; festival_id/name are
// "" on a non-festival day, or when festival_days is set without any
// FestivalDef naming that day (the plain test_festival_days case above).
static bool test_festival_id_and_name_lookup() {
    CalendarConfig cfg;
    cfg.festival_days = {181};
    cfg.festivals = {{"first_cutting_procession", 181, "First-Cutting Procession"}};
    Calendar cal(cfg);
    SIM_CHECK_EQ(cal.festival_id(181), std::string("first_cutting_procession"));
    SIM_CHECK_EQ(cal.festival_name(181), std::string("First-Cutting Procession"));
    SIM_CHECK_EQ(cal.festival_id(541), std::string("first_cutting_procession"));  // repeats in year 2
    SIM_CHECK(cal.festival_id(182).empty());   // not a festival day at all
    SIM_CHECK(cal.festival_name(182).empty());
    return true;
}

SIM_MAIN(test_day_one_is_year_one_month_one_day_one,
         test_year_rolls_over,
         test_day_number_round_trip,
         test_day_of_year,
         test_seasons_wrap,
         test_unnamed_months_by_default,
         test_festival_days,
         test_festival_days_repeat_every_year,
         test_festival_id_and_name_lookup)
