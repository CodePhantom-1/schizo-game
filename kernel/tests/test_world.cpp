// test_world.cpp — the Wave 2 DoD: the world runs headless, responds to the
// drought, moves rumours, and is byte-deterministic across a ten-year run.
#include "sim/World.hpp"

#include "sim/Test.hpp"

using namespace sim;

static bool test_init_seeds_the_world() {
    WorldState w;
    w.init("../db/canon", 42);
    SIM_CHECK(w.db.rows("deities").size() >= std::size_t{16});  // canon only grows
    // every canon city has a market with the grain base on the shelf
    SIM_CHECK(price_of(w.economy, "city_of_the_moon", "grain") > 0);
    SIM_CHECK(price_of(w.economy, "city_of_jewels", "grain") > 0);
    // the three named canon people exist
    SIM_CHECK(find_npc(w.population, "the_prophet") != nullptr);
    SIM_CHECK(find_npc(w.population, "law_giver") != nullptr);
    SIM_CHECK(find_npc(w.population, "the_warchief") != nullptr);
    SIM_CHECK_EQ(w.day, DayNumber{1});
    // The approved seasons (D-015) are live in the calendar:
    SIM_CHECK_EQ(w.cal.season_id(1), std::string("rains"));
    SIM_CHECK_EQ(w.cal.season_id(200), std::string("harvest"));
    SIM_CHECK_EQ(w.cal.season_id(280), std::string("vintage"));
    return true;
}

static bool test_prices_move_with_the_drought() {
    WorldState w;
    w.init("../db/canon", 42);
    w.advance_days(60);  // two months of drought 0
    const Silver before = price_of(w.economy, "city_of_the_moon", "grain");
    w.facts.drought_stage = 3;  // the drought that is breaking the world (notes L15, L104)
    w.advance_days(60);
    const Silver after = price_of(w.economy, "city_of_the_moon", "grain");
    SIM_CHECK(after > before);
    return true;
}

static bool test_a_rumour_crosses_the_city() {
    WorldState w;
    w.init("../db/canon", 42);
    // No canon social links exist yet (D-011): the caller wires the edge.
    w.population.knows["the_prophet"].push_back("the_warchief");
    witness(w.population, "the_prophet", "city_of_the_moon",
            "the Great Lighthouse burns sea-gem oil tonight", w.day);
    w.advance_days(2);  // heard on day D, retold at the tick of day D+1
    const Npc* heard = find_npc(w.population, "the_warchief");
    SIM_CHECK(heard != nullptr);
    bool found = false;
    for (const MemoryEntry& m : heard->memory)
        if (m.subject == "city_of_the_moon" && m.fact.find("Lighthouse") != std::string::npos)
            found = true;
    SIM_CHECK(found);
    return true;
}

static bool test_rites_refuse_the_unknowing() {
    WorldState w;
    w.init("../db/canon", 42);
    // The five powers begin with knowledge: a rite the performer never learned
    // is refused before any dice are thrown (rpg-systems §10.1).
    RiteResult refused = perform_rite(w.context(), w.magic, "barutu_haruspicy", RiteInputs{});
    SIM_CHECK_EQ(refused.performed, false);
    SIM_CHECK_EQ(refused.refusal_reason, std::string("rite_not_known"));
    // favour is bookkept and clamped (the deities.csv id space)
    add_favour(w.magic, "inanna", 200);
    SIM_CHECK_EQ(favour(w.magic, "inanna"), 100);
    add_favour(w.magic, "inanna", -300);
    SIM_CHECK_EQ(favour(w.magic, "inanna"), 0);
    return true;
}

static bool test_justice_sees_a_crime_through() {
    WorldState w;
    w.init("../db/canon", 42);
    report_crime(w.justice, "the_warchief", "", "theft", w.day, "the_prophet");
    SIM_CHECK_EQ(w.justice.open_crimes.size(), std::size_t{1});
    const Hearing h = hold_hearing(w.context(), w.justice,
                                   w.justice.open_crimes.front().id);
    // laws.csv is canon-empty: the documented fallback is compensation
    SIM_CHECK_EQ(h.verdict, std::string("compensation"));
    SIM_CHECK(w.justice.open_crimes.empty());
    SIM_CHECK_EQ(w.justice.verdicts.size(), std::size_t{1});
    return true;
}

static bool test_ten_years_are_deterministic() {
    WorldState a;
    a.init("../db/canon", 7);
    WorldState b;
    b.init("../db/canon", 7);
    a.advance_days(3600);  // ten 360-day years, one tick at a time
    b.advance_days(3600);
    SIM_CHECK_EQ(a.day, b.day);
    SIM_CHECK(a.economy.market_by_city == b.economy.market_by_city);
    SIM_CHECK(a.economy.stock_by_city_item == b.economy.stock_by_city_item);
    SIM_CHECK(a.population.npcs.size() == b.population.npcs.size());
    SIM_CHECK(a.justice.verdicts.size() == b.justice.verdicts.size());
    SIM_CHECK(a.property.steward_reports.size() == b.property.steward_reports.size());
    return true;
}

SIM_MAIN(test_init_seeds_the_world,
         test_prices_move_with_the_drought,
         test_a_rumour_crosses_the_city,
         test_rites_refuse_the_unknowing,
         test_justice_sees_a_crime_through,
         test_ten_years_are_deterministic)
