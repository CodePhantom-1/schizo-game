// test_scenario_drought.cpp — scenario hardening: one full 360-day year in the
// City of the Moon, once at drought 0 and once at drought 3.
//
// Proves the two things the engine will rely on when it binds to the kernel:
//   1. the drought-3 grain price curve is STRICTLY dearer than the drought-0
//      curve at every one of the year's twelve month ends; and
//   2. each world is individually deterministic — the same year run twice from
//      the same seed records identical prices (and identical end-of-year
//      economy bytes).
//
// Method (per the ask): two worlds from the SAME seed, differing only in
// WorldFacts::drought_stage (set after init — WorldState::init resets facts);
// both advance the year in twelve 30-day month chunks, recording the grain
// price after each month's last daily tick (the close of that month's market).
//
// No content numbers are hardcoded: the exact values asserted are rerun
// equality, the staple price floor (include/sim/Economy.hpp "never negative;
// never zero for food staples"), and the season labels read from
// db/canon/seasons.csv (CANON, designer approval 2026-09-25, D-015).
#include "sim/World.hpp"

#include "sim/Test.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace sim;

namespace {

const char* kCity = "city_of_the_moon";  // CANON row of db/canon/cities.csv
constexpr std::uint64_t kSeed = 360;     // fixed: the A/B comparison isolates the drought alone

constexpr int kMonths = 12;  // the default calendar shape (calendar.csv months_count is
                             // OPEN: 12 unnamed months x 30 days = the ask's 360-day year)
constexpr int kDaysPerMonth = 30;

// The season at each month's close, computed from db/canon/seasons.csv (CANON,
// D-015): rains from day-of-year 1, sowing from 91, harvest from 181, vintage
// from 271. Month m closes on day-of-year 30*m.
const char* season_at_month_end(int month) {
    const int doy = kDaysPerMonth * month;
    if (doy < 91) return "rains";
    if (doy < 181) return "sowing";
    if (doy < 271) return "harvest";
    return "vintage";
}

// One world, one year. Advances 360 days in month chunks and records the City
// of the Moon grain price after each month's last daily tick. Uses SIM_CHECK,
// so it returns bool and fills its out-parameter.
struct YearCurve {
    std::vector<Silver> month_end_price;  // [m-1]: grain price at the close of month m
    EconomyState end_of_year;             // the final economy (for byte comparisons)
    DayNumber day = 0;                    // the world's day counter after the year
};

bool run_year(int drought_stage, YearCurve& out) {
    WorldState w;
    w.init("../db/canon", kSeed);
    // init resets the world facts (src/World.cpp: facts = WorldFacts{}), so the
    // Act's drought is set here, after init — the stage then holds all year.
    w.facts.drought_stage = drought_stage;

    SIM_CHECK_EQ(w.cal.days_per_year(), kMonths * kDaysPerMonth);  // the ask's year
    SIM_CHECK(price_of(w.economy, kCity, "grain") >= 1);           // seeded staple

    for (int m = 1; m <= kMonths; ++m) {
        w.advance_days(kDaysPerMonth);  // one month of the default shape
        // The last ticked day is month m's last day: the sample is the close.
        SIM_CHECK_EQ(w.cal.to_date(w.day - 1).month, m);
        SIM_CHECK_EQ(w.cal.season_id(w.day - 1), season_at_month_end(m));
        const Silver price = price_of(w.economy, kCity, "grain");
        SIM_CHECK(price >= 1);  // staple: never negative, never zero (Economy.hpp)
        out.month_end_price.push_back(price);
    }
    // Exactly one full year has ticked: the world stands on new year's morning.
    SIM_CHECK_EQ(w.day, DayNumber{1 + kMonths * kDaysPerMonth});
    SIM_CHECK_EQ(w.cal.to_date(w.day).year, 2);
    SIM_CHECK_EQ(w.cal.to_date(w.day).month, 1);
    SIM_CHECK_EQ(w.cal.to_date(w.day).day, 1);

    out.end_of_year = w.economy;
    out.day = w.day;
    return true;
}

// --- the drought curve --------------------------------------------------------

static bool test_drought_three_is_strictly_dearer_at_every_month_end() {
    YearCurve wet;  // drought 0: no drought
    YearCurve dry;  // drought 3: the drought that is breaking the world
    SIM_CHECK(run_year(0, wet));
    SIM_CHECK(run_year(3, dry));
    SIM_CHECK_EQ(wet.month_end_price.size(), std::size_t{kMonths});
    SIM_CHECK_EQ(dry.month_end_price.size(), std::size_t{kMonths});

    // Strictly dearer at EVERY month's end — the scarcity premium (the grain
    // anchor rises per drought stage) and the failing harvest (the inflow
    // shrinks per stage, failing outright at stage 4) both push the same way,
    // so no month's close may even tie.
    for (int m = 1; m <= kMonths; ++m) {
        const Silver dear = dry.month_end_price[m - 1];  // drought 3
        const Silver fair = wet.month_end_price[m - 1];  // drought 0
        SIM_CHECK(dear >= 1);
        SIM_CHECK(fair >= 1);
        SIM_CHECK(dear > fair);  // month m's close: drought 3 strictly dearer
    }
    return true;
}

// --- determinism --------------------------------------------------------------

static bool test_each_world_reruns_identically() {
    // The ask: re-run one world twice, identical prices. Both worlds are
    // re-run here — a superset of that.
    YearCurve wet_a, wet_b, dry_a, dry_b;
    SIM_CHECK(run_year(0, wet_a));
    SIM_CHECK(run_year(0, wet_b));  // the drought-0 year, run twice
    SIM_CHECK(run_year(3, dry_a));
    SIM_CHECK(run_year(3, dry_b));  // the drought-3 year, run twice

    for (int m = 1; m <= kMonths; ++m) {
        SIM_CHECK_EQ(wet_b.month_end_price[m - 1], wet_a.month_end_price[m - 1]);
        SIM_CHECK_EQ(dry_b.month_end_price[m - 1], dry_a.month_end_price[m - 1]);
    }
    // Stronger than the sampled prices: the whole end-of-year economy matches
    // byte for byte across reruns of the same world.
    SIM_CHECK(wet_a.end_of_year.market_by_city == wet_b.end_of_year.market_by_city);
    SIM_CHECK(wet_a.end_of_year.stock_by_city_item == wet_b.end_of_year.stock_by_city_item);
    SIM_CHECK(dry_a.end_of_year.market_by_city == dry_b.end_of_year.market_by_city);
    SIM_CHECK(dry_a.end_of_year.stock_by_city_item == dry_b.end_of_year.stock_by_city_item);
    SIM_CHECK_EQ(wet_a.day, wet_b.day);
    SIM_CHECK_EQ(dry_a.day, dry_b.day);
    return true;
}

}  // namespace

SIM_MAIN(test_drought_three_is_strictly_dearer_at_every_month_end,
         test_each_world_reruns_identically)
